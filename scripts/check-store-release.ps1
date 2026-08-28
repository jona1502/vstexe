[CmdletBinding()]
param(
    [string] $IdentityName = $env:INPUTRACK_STORE_IDENTITY_NAME,
    [string] $Publisher = $env:INPUTRACK_STORE_PUBLISHER,
    [string] $ProductId = $env:INPUTRACK_STORE_PRODUCT_ID,
    [string] $StoreUrl = $env:PUBLIC_INPUTRACK_STORE_URL,
    [switch] $AllowIncompleteLegal
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path $PSScriptRoot -Parent

function Require-Value([string] $Name, [string] $Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) { throw "$Name is required for a Store release." }
}

Require-Value 'INPUTRACK_STORE_IDENTITY_NAME' $IdentityName
Require-Value 'INPUTRACK_STORE_PUBLISHER' $Publisher
Require-Value 'INPUTRACK_STORE_PRODUCT_ID' $ProductId
Require-Value 'PUBLIC_INPUTRACK_STORE_URL' $StoreUrl

if ($IdentityName -eq 'InputRack.Dev' -or $Publisher -eq 'CN=InputRack Development') {
    throw 'Development package identity cannot be submitted to the Microsoft Store.'
}
if ($ProductId -notmatch '^[A-Z0-9]{12}$') {
    throw 'INPUTRACK_STORE_PRODUCT_ID must be the 12-character Partner Center Product ID.'
}
$expectedStoreUrl = "https://apps.microsoft.com/detail/$ProductId"
if ($StoreUrl.TrimEnd('/') -ne $expectedStoreUrl) {
    throw "PUBLIC_INPUTRACK_STORE_URL must be $expectedStoreUrl"
}

$productPath = Join-Path $repositoryRoot 'store\product.json'
$product = Get-Content -LiteralPath $productPath -Raw | ConvertFrom-Json
if ($product.application.acquisition -ne 'free') { throw 'The base Store application must be free.' }
if ($product.pro.productType -ne 'durable' -or $product.pro.billing -ne 'one-time') {
    throw 'InputRack Pro must be configured as a one-time durable add-on.'
}
if (@($product.pro.includedMajorVersions).Count -ne 1 -or
    $product.pro.includedMajorVersions[0] -ne 1) {
    throw 'The Pro purchase must include the agreed 1.x update line.'
}
if ($product.pro.offerToken -ne 'inputrack.pro') { throw 'Unexpected Pro offer token in store/product.json.' }
if ($product.pro.regularPrice.currency -ne 'EUR' -or $product.pro.regularPrice.amount -ne 29.99) {
    throw 'The regular Pro price must match the agreed EUR 29.99 model.'
}
if ($product.pro.launchPrice.currency -ne 'EUR' -or $product.pro.launchPrice.amount -ne 19.99) {
    throw 'The launch Pro price must match the agreed EUR 19.99 model.'
}
if ($product.trial.managedBy -ne 'application' -or $product.trial.durationDays -ne 14) {
    throw 'The Pro trial must be the agreed 14-day application-managed trial.'
}

$commerceBuild = Get-Content -LiteralPath (Join-Path $repositoryRoot 'commerce\CMakeLists.txt') -Raw
if ($commerceBuild -notmatch 'INPUTRACK_PRO_OFFER_TOKEN="inputrack\.pro"') {
    throw 'The application offer token does not match store/product.json.'
}
foreach ($locale in @('de-DE', 'en-US')) {
    $listing = Join-Path $repositoryRoot "store\listings\$locale.md"
    if (-not (Test-Path -LiteralPath $listing)) { throw "Missing Store listing: $listing" }
}

$siteData = Get-Content -LiteralPath (Join-Path $repositoryRoot 'apps\web\src\data\site.ts') -Raw
if (-not $AllowIncompleteLegal -and $siteData -match "TODO:") {
    throw 'Complete the operator/address/email values in apps/web/src/data/site.ts before launch.'
}

Write-Host 'Store release configuration is internally consistent.'
Write-Host "Identity: $IdentityName"
Write-Host "Product:  $ProductId"
Write-Host 'Model:    Free app + 14-day Pro trial + EUR 29.99 durable Pro add-on (EUR 19.99 launch offer)'
