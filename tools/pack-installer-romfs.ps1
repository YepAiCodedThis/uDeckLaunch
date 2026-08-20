# Pack uDeckLaunch payload into uManager romfs for single-NRO install.
# Output: projects/uManager/romfs/payload.zip

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$SdOut = Join-Path $Root 'SdOut'
$Vendor = Join-Path $Root '_vendor\uLaunch-1.2.5'
$Staging = Join-Path $Root 'tools\.payload-staging'
$SdOutStaging = Join-Path $Staging 'SdOut'
$ZipPath = Join-Path $Root 'projects\uManager\romfs\payload.zip'

if (-not (Test-Path (Join-Path $Vendor 'ulaunch\bin\uSystem\exefs.nsp'))) {
    throw "Missing vendor uLaunch at $Vendor"
}
if (-not (Test-Path (Join-Path $SdOut 'ulaunch\bin\uMenu\main'))) {
    throw 'Build uDeckLaunch first: make umenu'
}

if (Test-Path $Staging) { Remove-Item $Staging -Recurse -Force }
New-Item -ItemType Directory -Force -Path $SdOutStaging | Out-Null

function Copy-Tree($from, $to) {
    New-Item -ItemType Directory -Force -Path (Split-Path $to) | Out-Null
    Copy-Item -LiteralPath $from -Destination $to -Recurse -Force
}

# uSystem overlay + uLaunch backend
Copy-Tree (Join-Path $Vendor 'atmosphere\contents\0100000000001000') (Join-Path $SdOutStaging 'atmosphere\contents\0100000000001000')
Copy-Tree (Join-Path $Vendor 'ulaunch\bin') (Join-Path $SdOutStaging 'ulaunch\bin')
$langDst = Join-Path $SdOutStaging 'ulaunch\lang'
New-Item -ItemType Directory -Force -Path $langDst | Out-Null
foreach ($langSrc in @(
    (Join-Path $Vendor 'ulaunch\lang'),
    (Join-Path $SdOut 'ulaunch\lang')
)) {
    if (-not (Test-Path $langSrc)) { continue }
    Get-ChildItem $langSrc -Directory | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $langDst $_.Name) -Recurse -Force
    }
}
if (Test-Path (Join-Path $Vendor 'ulaunch\themes')) {
    Copy-Tree (Join-Path $Vendor 'ulaunch\themes') (Join-Path $SdOutStaging 'ulaunch\themes')
}

# uDeckLaunch menu
Copy-Item (Join-Path $SdOut 'ulaunch\bin\uMenu\main') (Join-Path $SdOutStaging 'ulaunch\bin\uMenu\main') -Force
Copy-Item (Join-Path $SdOut 'ulaunch\bin\uMenu\main.npdm') (Join-Path $SdOutStaging 'ulaunch\bin\uMenu\main.npdm') -Force
Copy-Item (Join-Path $SdOut 'ulaunch\bin\uMenu\romfs.bin') (Join-Path $SdOutStaging 'ulaunch\bin\uMenu\romfs.bin') -Force

New-Item -ItemType Directory -Force -Path (Join-Path $Root 'projects\uManager\romfs') | Out-Null
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path (Join-Path $Staging 'SdOut') -DestinationPath $ZipPath -CompressionLevel Optimal -Force

Remove-Item $Staging -Recurse -Force
Write-Host ("payload.zip: {0} bytes" -f (Get-Item $ZipPath).Length)
