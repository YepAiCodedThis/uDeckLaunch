# Install uDeckLaunch onto the Switch SD (G:).
# Keeps NXThemes romfs under 0100000000001000. Removes hello-sys 100D overlay.
# Prefers a local SdOut build; falls back to official uLaunch 1.2.5 for uSystem/uLoader.

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$SdOut = Join-Path $Root 'SdOut'
$Vendor = Join-Path $Root '_vendor\uLaunch-1.2.5'
$Sd = 'G:'

if (-not (Test-Path $Sd)) { throw 'SD card G: is not mounted' }
$dest1000 = Join-Path $Sd 'atmosphere\contents\0100000000001000'
$romfs = Join-Path $dest1000 'romfs'
if (-not (Test-Path $dest1000)) { New-Item -ItemType Directory -Force -Path $dest1000 | Out-Null }
if (-not (Test-Path $romfs)) {
    New-Item -ItemType Directory -Force -Path $romfs | Out-Null
    Write-Host 'Created empty 1000/romfs (NXThemes slot — add themes later if needed)'
}

function Copy-Tree($from, $to) {
    if (-not (Test-Path $from)) { throw "missing $from" }
    New-Item -ItemType Directory -Force -Path $to | Out-Null
    Copy-Item -LiteralPath $from -Destination $to -Recurse -Force
}

$sysNsp = $null
if (Test-Path (Join-Path $SdOut 'atmosphere\contents\0100000000001000\exefs.nsp')) {
    $sysNsp = Join-Path $SdOut 'atmosphere\contents\0100000000001000\exefs.nsp'
    $binSrc = Join-Path $SdOut 'ulaunch\bin'
} elseif (Test-Path (Join-Path $Vendor 'atmosphere\contents\0100000000001000\exefs.nsp')) {
    $sysNsp = Join-Path $Vendor 'atmosphere\contents\0100000000001000\exefs.nsp'
    $binSrc = Join-Path $Vendor 'ulaunch\bin'
} else {
    throw 'No SdOut or vendor uLaunch 1.2.5 binaries'
}

Copy-Item -LiteralPath $sysNsp -Destination (Join-Path $dest1000 'exefs.nsp') -Force
Write-Host ("1000 exefs.nsp <= {0} ({1} bytes)" -f $sysNsp, (Get-Item $sysNsp).Length)

# hello-sys PhotoViewer overlay must not sit on 100D
Get-ChildItem (Join-Path $Sd 'atmosphere\contents') -Directory | Where-Object {
    $_.Name -eq '010000000000100D'
} | ForEach-Object {
    $off = Join-Path $_.Parent.FullName ($_.Name + '.off-udeck')
    if (Test-Path $off) { Remove-Item $off -Recurse -Force }
    Move-Item $_.FullName $off -Force
    Write-Host "100D overlay moved to $off"
}

New-Item -ItemType Directory -Force -Path (Join-Path $Sd 'ulaunch\bin') | Out-Null
Copy-Item (Join-Path $binSrc '*') (Join-Path $Sd 'ulaunch\bin') -Recurse -Force

$ulaunchSrc = Split-Path -Parent $binSrc
foreach ($extra in @('lang', 'themes')) {
    $from = Join-Path $ulaunchSrc $extra
    if (Test-Path $from) {
        Copy-Item $from (Join-Path $Sd "ulaunch\$extra") -Recurse -Force
        Write-Host "ulaunch/$extra <= vendor"
    }
}
$sdOutUlaunch = Join-Path $SdOut 'ulaunch'
foreach ($extra in @('lang', 'themes')) {
    $from = Join-Path $sdOutUlaunch $extra
    if (Test-Path $from) {
        Copy-Item $from (Join-Path $Sd "ulaunch\$extra") -Recurse -Force
        Write-Host "ulaunch/$extra <= SdOut"
    }
}

$menuBuilt = Join-Path $SdOut 'ulaunch\bin\uMenu\main'
if (Test-Path $menuBuilt) {
    Copy-Item $menuBuilt (Join-Path $Sd 'ulaunch\bin\uMenu\main') -Force
    Copy-Item (Join-Path $SdOut 'ulaunch\bin\uMenu\main.npdm') (Join-Path $Sd 'ulaunch\bin\uMenu\main.npdm') -Force
    if (Test-Path (Join-Path $SdOut 'ulaunch\bin\uMenu\romfs.bin')) {
        Copy-Item (Join-Path $SdOut 'ulaunch\bin\uMenu\romfs.bin') (Join-Path $Sd 'ulaunch\bin\uMenu\romfs.bin') -Force
    }
    Write-Host 'uMenu replaced with uDeckLaunch build'
}

$mgr = Join-Path $SdOut 'switch\uManager.nro'
if (-not (Test-Path $mgr)) { $mgr = Join-Path $Vendor 'switch\uManager.nro' }
if (Test-Path $mgr) {
    Copy-Item $mgr (Join-Path $Sd 'switch\uManager.nro') -Force
}

Write-Host 'uDeckLaunch install done. Eject SD and reboot Atmosphere.'
Write-Host 'romfs kept:' (Test-Path $romfs)
Write-Host '100D overlay gone:' (-not (Test-Path (Join-Path $Sd 'atmosphere\contents\010000000000100D\exefs.nsp')))
