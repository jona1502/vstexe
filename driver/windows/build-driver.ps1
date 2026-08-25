[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug',

    [ValidateSet('x64', 'ARM64')]
    [string] $Platform = 'x64'
)

$ErrorActionPreference = 'Stop'
$driverRoot = Join-Path $PSScriptRoot 'sysvad'
$projects = @(
    (Join-Path $driverRoot 'EndpointsCommon\EndpointsCommon.vcxproj'),
    (Join-Path $driverRoot 'TabletAudioSample\TabletAudioSample.vcxproj')
)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$windowsKits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'

foreach ($project in $projects) {
    if (-not (Test-Path -LiteralPath $project)) {
        throw "Virtual microphone driver project not found at $project"
    }
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

$msbuild = Join-Path $installation 'MSBuild\Current\Bin\amd64\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) {
    throw "MSBuild was not found at $msbuild"
}

$buildTemp = Join-Path (Split-Path $PSScriptRoot -Parent | Split-Path -Parent) 'build\driver-temp'
New-Item -ItemType Directory -Path $buildTemp -Force | Out-Null

# Some terminals expose both PATH and Path. MSBuild runs on .NET Framework,
# whose process launcher treats those case-insensitive names as duplicates.
# Build an explicit, case-insensitive environment and keep temp files writable.
$cleanEnvironment = [Collections.Generic.Dictionary[string, string]]::new(
    [StringComparer]::OrdinalIgnoreCase)
foreach ($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) {
    $cleanEnvironment[$entry.Key] = $entry.Value
}
$cleanEnvironment['TEMP'] = $buildTemp
$cleanEnvironment['TMP'] = $buildTemp

foreach ($project in $projects) {
    $msbuildArguments = @(
        $project,
        '/m',
        '/t:Build',
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        '/p:SignMode=Off'
    )
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $msbuild
    $startInfo.UseShellExecute = $false
    foreach ($argument in $msbuildArguments) {
        $startInfo.ArgumentList.Add($argument)
    }
    $startInfo.Environment.Clear()
    foreach ($entry in $cleanEnvironment.GetEnumerator()) {
        $startInfo.Environment[$entry.Key] = $entry.Value
    }

    $build = [Diagnostics.Process]::Start($startInfo)
    $build.WaitForExit()
    if ($build.ExitCode -ne 0) {
        throw "Driver build failed for $project with exit code $($build.ExitCode)"
    }
}

$output = Join-Path $driverRoot "TabletAudioSample\$Platform\$Configuration"
Write-Host "Driver build completed: $output"
