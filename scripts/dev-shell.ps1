<#
.SYNOPSIS
    Loads the Visual Studio build environment into the current PowerShell session.

.DESCRIPTION
    CMake, cl.exe and ninja ship inside the Visual Studio installation and are
    not on the global PATH. Dot-source this script once per session, then the
    CMake presets in the repository root work as documented in README.md:

        . .\scripts\dev-shell.ps1
        cmake --build --preset windows-debug

    The script keeps the current working directory instead of jumping to the
    Visual Studio default location.
#>
[CmdletBinding()]
param(
    [ValidateSet('amd64', 'x86', 'arm64')]
    [string] $Arch = 'amd64'
)

$ErrorActionPreference = 'Stop'

# vswhere is installed with any modern Visual Studio and knows every edition.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

$installPath = $null
if (Test-Path $vswhere) {
    $installPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
}

# Fall back to the well-known layouts if vswhere is missing or finds nothing.
if (-not $installPath) {
    $candidates = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\*",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*"
    )
    $installPath = Get-ChildItem $candidates -Directory -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName 'Common7\Tools\Launch-VsDevShell.ps1') } |
        Select-Object -First 1 -ExpandProperty FullName
}

if (-not $installPath) {
    throw 'No Visual Studio installation with the C++ build tools was found. Install the "Desktop development with C++" workload.'
}

$launcher = Join-Path $installPath 'Common7\Tools\Launch-VsDevShell.ps1'
if (-not (Test-Path $launcher)) {
    throw "Launch-VsDevShell.ps1 is missing from $installPath."
}

& $launcher -Arch $Arch -HostArch $Arch -SkipAutomaticLocation | Out-Null

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    throw 'The Visual Studio environment loaded but cmake is still not on PATH. Add the "C++ CMake tools for Windows" component.'
}

Write-Host "Visual Studio environment ready ($Arch)" -ForegroundColor Green
Write-Host "  cmake  $cmake"
Write-Host "  cl     $((Get-Command cl -ErrorAction SilentlyContinue).Source)"
