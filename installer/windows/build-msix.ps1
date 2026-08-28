[CmdletBinding()]
param(
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string] $Version = '0.1.0',

    [string] $BuildDirectory,

    [string] $OutputDirectory,

    [string] $IdentityName = 'InputRack.Dev',

    [string] $Publisher = 'CN=InputRack Development'
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repositoryRoot 'build\windows-store'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'out\windows'
}

$application = Join-Path $BuildDirectory 'apps\desktop\InputRack_artefacts\Release\InputRack.exe'
$scanner = Join-Path $BuildDirectory 'apps\scanner\InputRackPluginScanner_artefacts\Release\InputRackPluginScanner.exe'
foreach ($binary in @($application, $scanner)) {
    if (-not (Test-Path -LiteralPath $binary)) {
        throw "Release binary not found at $binary. Build the windows-release configuration first."
    }
}

$cache = Join-Path $BuildDirectory 'CMakeCache.txt'
$isStoreBuild = (Test-Path -LiteralPath $cache) -and
    (Select-String -LiteralPath $cache -Pattern '^INPUTRACK_STORE_BUILD:BOOL=ON$' -Quiet)
if (-not $isStoreBuild) {
    throw "The build at $BuildDirectory is not configured with INPUTRACK_STORE_BUILD=ON."
}

$makeAppx = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" `
    -Recurse -Filter makeappx.exe -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\makeappx\.exe$' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName
if ([string]::IsNullOrWhiteSpace($makeAppx)) {
    throw 'MakeAppx.exe was not found. Install the Windows 11 SDK.'
}

$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
$staging = [IO.Path]::GetFullPath((Join-Path $outputRoot 'msix-staging'))
if (-not $staging.StartsWith($outputRoot + [IO.Path]::DirectorySeparatorChar,
                             [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to stage outside the output directory: $staging"
}
if (Test-Path -LiteralPath $staging) {
    Remove-Item -LiteralPath $staging -Recurse -Force
}
$assets = Join-Path $staging 'Assets'
New-Item -ItemType Directory -Path $assets -Force | Out-Null

Copy-Item -LiteralPath $application -Destination (Join-Path $staging 'InputRack.exe')
Copy-Item -LiteralPath $scanner -Destination (Join-Path $staging 'InputRackPluginScanner.exe')

Add-Type -AssemblyName System.Drawing
$sourceLogo = [Drawing.Image]::FromFile((Join-Path $repositoryRoot 'voice.png'))
try {
    $assetSizes = @{
        'StoreLogo.png' = @(50, 50)
        'Square44x44Logo.png' = @(44, 44)
        'Square150x150Logo.png' = @(150, 150)
        'Wide310x150Logo.png' = @(310, 150)
    }
    foreach ($asset in $assetSizes.GetEnumerator()) {
        $width, $height = $asset.Value
        $bitmap = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([Drawing.Color]::Transparent)
                $graphics.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
                $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
                $side = [Math]::Min($width, $height)
                $left = [int](($width - $side) / 2)
                $top = [int](($height - $side) / 2)
                $graphics.DrawImage($sourceLogo, $left, $top, $side, $side)
            } finally {
                $graphics.Dispose()
            }
            $bitmap.Save((Join-Path $assets $asset.Key), [Drawing.Imaging.ImageFormat]::Png)
        } finally {
            $bitmap.Dispose()
        }
    }
} finally {
    $sourceLogo.Dispose()
}

$manifestTemplate = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'msix\AppxManifest.xml.in') -Raw
$packageVersion = "$Version.0"
$manifest = $manifestTemplate.Replace('@IDENTITY_NAME@', [Security.SecurityElement]::Escape($IdentityName))
$manifest = $manifest.Replace('@PUBLISHER@', [Security.SecurityElement]::Escape($Publisher))
$manifest = $manifest.Replace('@PACKAGE_VERSION@', $packageVersion)
[IO.File]::WriteAllText((Join-Path $staging 'AppxManifest.xml'), $manifest,
                        [Text.UTF8Encoding]::new($false))

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$package = Join-Path $outputRoot "InputRack-$Version-Windows-x64.msix"
if (Test-Path -LiteralPath $package) {
    Remove-Item -LiteralPath $package -Force
}
& $makeAppx pack /d $staging /p $package /o
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $package)) {
    throw "MakeAppx failed to create $package"
}

Write-Host "MSIX created: $package"
Write-Host "Identity: $IdentityName"
Write-Host "Publisher: $Publisher"
