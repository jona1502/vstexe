[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('Install', 'Uninstall', 'Status')]
    [string] $Action = 'Status',

    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug',

    [switch] $EnableTestSigning
)

$ErrorActionPreference = 'Stop'
$hardwareId = 'Root\InputRackVirtualMic'
$serviceName = 'sysvad_componentizedaudiosample'
$certificateSubject = 'CN=InputRack Development Driver'
$windowsKits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$repositoryRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-WdkTool([string] $Folder, [string] $Executable, [string] $Architecture) {
    $root = Join-Path $windowsKits $Folder
    $tool = Get-ChildItem -LiteralPath $root -Filter $Executable -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "\\$Architecture\\$([regex]::Escape($Executable))$" } |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if ($null -eq $tool) {
        throw "$Executable ($Architecture) was not found in the Windows Driver Kit."
    }
    return $tool.FullName
}

function Invoke-Native([string] $Executable, [string[]] $Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable failed with exit code $LASTEXITCODE."
    }
}

function Get-TestSigningEnabled {
    $configuration = (& bcdedit.exe /enum '{current}' 2>&1) -join "`n"
    return $configuration -match '(?im)^testsigning\s+(Yes|Ja)\s*$'
}

function Show-Status {
    $device = Get-PnpDevice -ErrorAction SilentlyContinue |
        Where-Object { $_.InstanceId -like 'ROOT\INPUTRACKVIRTUALMIC*' }
    $endpoint = Get-PnpDevice -Class AudioEndpoint -ErrorAction SilentlyContinue |
        Where-Object { $_.FriendlyName -like 'InputRack Microphone*' }
    $service = Get-Service -Name $serviceName -ErrorAction SilentlyContinue

    [pscustomobject]@{
        TestSigning = Get-TestSigningEnabled
        Device       = if ($device) { ($device.Status | Select-Object -First 1) } else { 'Not installed' }
        Driver       = if ($service) { $service.Status } else { 'Not installed' }
        Endpoint     = if ($endpoint) { ($endpoint.Status | Select-Object -First 1) } else { 'Not available' }
    } | Format-List
}

if ($Action -eq 'Status') {
    Show-Status
    return
}

if (-not (Test-Administrator)) {
    throw 'Run PowerShell as Administrator for driver installation or removal.'
}

$devcon = Get-WdkTool 'Tools' 'devcon.exe' 'x64'

if ($Action -eq 'Uninstall') {
    $driverPackages = Get-CimInstance Win32_PnPSignedDriver |
        Where-Object { $_.DeviceID -like 'ROOT\INPUTRACKVIRTUALMIC*' } |
        Select-Object -ExpandProperty InfName -Unique

    & $devcon remove $hardwareId
    if ($LASTEXITCODE -gt 1) {
        throw "DevCon could not remove $hardwareId (exit code $LASTEXITCODE)."
    }
    foreach ($driverPackage in $driverPackages) {
        Invoke-Native 'pnputil.exe' @('/delete-driver', $driverPackage, '/uninstall', '/force')
    }
    Write-Host 'InputRack Virtual Microphone was removed.'
    return
}

if (-not (Get-TestSigningEnabled)) {
    if (-not $EnableTestSigning) {
        throw 'Windows test-signing is disabled. Re-run with -EnableTestSigning, restart Windows, then run Install again.'
    }
    Invoke-Native 'bcdedit.exe' @('/set', 'testsigning', 'on')
    Write-Warning 'Test-signing was enabled. Restart Windows, then run this Install command again.'
    return
}

& (Join-Path $PSScriptRoot 'build-driver.ps1') -Configuration $Configuration -Platform x64

$source = Join-Path $PSScriptRoot "sysvad\TabletAudioSample\x64\$Configuration"
$package = Join-Path $repositoryRoot "build\driver-package\x64\$Configuration"
$inf = Join-Path $package 'ComponentizedAudioSample.inf'
$sys = Join-Path $package 'TabletAudioSample.sys'
$cat = Join-Path $package 'sysvad.cat'
New-Item -ItemType Directory -Path $package -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $source 'ComponentizedAudioSample.inf') -Destination $inf -Force
Copy-Item -LiteralPath (Join-Path $source 'TabletAudioSample.sys') -Destination $sys -Force
if (Test-Path -LiteralPath $cat) {
    Remove-Item -LiteralPath $cat -Force
}

$certificate = Get-ChildItem Cert:\LocalMachine\My |
    Where-Object { $_.Subject -eq $certificateSubject -and $_.NotAfter -gt (Get-Date) } |
    Sort-Object NotAfter -Descending |
    Select-Object -First 1
if ($null -eq $certificate) {
    $certificate = New-SelfSignedCertificate -Type CodeSigningCert `
        -Subject $certificateSubject -CertStoreLocation Cert:\LocalMachine\My `
        -HashAlgorithm SHA256 -KeyExportPolicy Exportable
}

$certificateFile = Join-Path $package 'InputRackDevelopmentDriver.cer'
Export-Certificate -Cert $certificate -FilePath $certificateFile -Force | Out-Null
Import-Certificate -FilePath $certificateFile -CertStoreLocation Cert:\LocalMachine\Root | Out-Null
Import-Certificate -FilePath $certificateFile -CertStoreLocation Cert:\LocalMachine\TrustedPublisher | Out-Null

$signTool = Get-WdkTool 'bin' 'signtool.exe' 'x64'
$inf2Cat = Get-WdkTool 'bin' 'Inf2Cat.exe' 'x86'
$signArguments = @('sign', '/v', '/fd', 'SHA256', '/ph', '/s', 'My', '/sm',
                   '/sha1', $certificate.Thumbprint)
Invoke-Native $signTool ($signArguments + $sys)
Invoke-Native $inf2Cat @("/driver:$package", '/os:10_X64')
Invoke-Native $signTool ($signArguments + $cat)
Invoke-Native $signTool @('verify', '/kp', '/v', $cat)

& $devcon install $inf $hardwareId
if ($LASTEXITCODE -gt 1) {
    throw "DevCon could not install the driver (exit code $LASTEXITCODE)."
}
if ($LASTEXITCODE -eq 1) {
    Write-Warning 'The driver was installed and Windows requested a restart.'
} else {
    Write-Host 'InputRack Virtual Microphone is installed.'
}
Show-Status
