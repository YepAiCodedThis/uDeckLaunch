# Remove uDeckLaunch / uLaunch / DeckHome from the Switch SD (G:).
# Keeps NXThemes romfs under 0100000000001000. Restores stock qlaunch from NAND after reboot.
# Also removes DeckHome atmosphere overrides that break Album (100D overlay, override_config.ini).

$ErrorActionPreference = 'Stop'
$Sd = 'G:'

if (-not (Test-Path $Sd)) { throw 'SD card G: is not mounted (Hekate USB mass storage)' }
$vol = Get-Volume -DriveLetter G -ErrorAction SilentlyContinue
if ($vol.FileSystemLabel -ne 'SWITCH SD') { throw 'G: is not SWITCH SD' }

Write-Host 'Removing uDeckLaunch / uLaunch / DeckHome from SD...'

$paths = @(
    "$Sd\ulaunch",
    "$Sd\switch\uManager.nro",
    "$Sd\switch\.uManager.nro.star",
    "$Sd\switch\uDeckLaunch",
    "$Sd\switch\DeckHome",
    "$Sd\deckhome",
    "$Sd\atmosphere\contents\0100000000001000\exefs.nsp",
    "$Sd\atmosphere\contents\010000000000100D"
)

foreach ($p in $paths) {
    if (Test-Path $p) {
        Write-Host "  delete $p"
        Remove-Item -LiteralPath $p -Recurse -Force
    } else {
        Write-Host "  skip (absent): $p"
    }
}

# DeckHome backup folders for 100D overlay
Get-ChildItem "$Sd\atmosphere\contents" -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like '010000000000100D.off-*' -or $_.Name -like '010000000000100D.off-udeck' } |
    ForEach-Object {
        Write-Host "  delete $($_.FullName)"
        Remove-Item -LiteralPath $_.FullName -Recurse -Force
    }

Write-Host ''
Write-Host 'Left intact: hbmenu.nro, atmosphere/hbl.nsp, 0100000000001000/romfs (NXThemes).'
Write-Host 'Reboot the Switch for stock Nintendo HOME from NAND.'
Write-Host 'For R+Album homebrew: run tools/fix-r-album-hbl.ps1 after reboot test.'
