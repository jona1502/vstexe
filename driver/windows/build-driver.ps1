[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug',

    [ValidateSet('x64', 'ARM64')]
    [string] $Platform = 'x64'
)

$ErrorActionPreference = 'Stop'
$driverRoot = Join-Path $PSScriptRoot 'sysvad'
$solution = Join-Path $driverRoot 'sysvad.sln'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$windowsKits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'

if (-not (Test-Path -LiteralPath $solution)) {
    throw "SYSVAD solution not found at $solution"
}

$wdmHeaders = Get-ChildItem -LiteralPath (Join-Path $windowsKits 'Include') `
    -Filter wdm.h -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
$driverTargets = Get-ChildItem -LiteralPath (Join-Path $windowsKits 'build') `
    -Filter WindowsDriver.Common.targets -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1

if ($null -eq $wdmHeaders -or $null -eq $driverTargets) {
    throw 'Windows Driver Kit is missing. Install the Windows 11 WDK matching the installed SDK before building the driver.'
}

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio Installer discovery tool (vswhere.exe) was not found.'
}

$installation = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if ([string]::IsNullOrWhiteSpace($installation)) {
    throw 'A Visual Studio installation containing MSBuild was not found.'
}

$msbuild = Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) {
    throw "MSBuild was not found at $msbuild"
}

& $msbuild $solution /m /t:Build "/p:Configuration=$Configuration" "/p:Platform=$Platform"
if ($LASTEXITCODE -ne 0) {
    throw "Driver build failed with exit code $LASTEXITCODE"
}
