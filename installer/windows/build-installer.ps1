[CmdletBinding()]
param(
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string] $Version = '0.1.0',

    [string] $BuildDirectory,

    [string] $OutputDirectory
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repositoryRoot 'build\windows-release'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'out\windows'
}

$application = Join-Path $BuildDirectory 'apps\desktop\VocalChain_artefacts\Release\VocalChain.exe'
if (-not (Test-Path -LiteralPath $application)) {
    throw "Release application not found at $application. Build the windows-release preset first."
}
$application = (Resolve-Path -LiteralPath $application).Path

$compilerCandidates = @(
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
)
$compiler = $compilerCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($compiler)) {
    $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        $compiler = $command.Source
    }
}
if ([string]::IsNullOrWhiteSpace($compiler)) {
    throw 'Inno Setup 6 was not found. Install it with: winget install JRSoftware.InnoSetup'
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$definition = Join-Path $PSScriptRoot 'VocalChain.iss'
& $compiler "/DAppVersion=$Version" "/DAppBinary=$application" `
    "/DOutputDirectory=$OutputDirectory" $definition
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE."
}

$installer = Join-Path $OutputDirectory "VocalChain-$Version-Windows-x64-Setup.exe"
if (-not (Test-Path -LiteralPath $installer)) {
    throw "Expected installer was not created at $installer"
}

$checksum = Get-FileHash -LiteralPath $installer -Algorithm SHA256
$checksumFile = "$installer.sha256"
[IO.File]::WriteAllText($checksumFile, "$($checksum.Hash.ToLowerInvariant())  $($checksum.Path | Split-Path -Leaf)`n")
Write-Host "Installer created: $installer"
Write-Host "SHA-256: $($checksum.Hash)"
