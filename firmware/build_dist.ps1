# build_dist.ps1 — build all three release artifacts into dist/: full, ota, littlefs.
# See README.md's Build section for what each one is for and the merge_bin recipe this
# automates. Run from anywhere; always operates on firmware/ (this script's own folder).
#
# By default, deletes any *other* version's bins already in dist/ once the new ones are
# built — dist/ is scratch output (gitignored), not an archive, and stale binaries sitting
# next to the current ones is how you flash the wrong one by accident. Pass -KeepOld to
# keep every version around instead.
#
# Usage: pwsh firmware/build_dist.ps1  [-Env esp32dev] [-KeepOld]

param(
    [string]$Env = "esp32dev",
    [switch]$KeepOld
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$core = if ($env:PLATFORMIO_CORE_DIR) { $env:PLATFORMIO_CORE_DIR } else { Join-Path $env:USERPROFILE ".platformio" }
$esptool  = Join-Path $core "packages\tool-esptoolpy\esptool.py"
$corePkg  = Join-Path $core "packages\framework-arduinoespressif32"
$bootApp0 = Join-Path $corePkg "tools\partitions\boot_app0.bin"
$genPart  = Join-Path $corePkg "tools\gen_esp32part.py"

# --- Firmware version, read from platformio.ini so dist/ filenames can never drift from
# what's actually embedded in the binary (-D FW_VERSION).
$ini = Get-Content "platformio.ini" -Raw
if ($ini -notmatch 'FW_VERSION=\\"([\d.]+)\\"') { throw "Could not find FW_VERSION in platformio.ini" }
$version = $Matches[1]
Write-Host "== Somfy BLE Hub firmware v$version ($Env) =="

$buildDir = ".pio\build\$Env"

Write-Host "-- pio run -e $Env"
python -m platformio run -e $Env
if ($LASTEXITCODE -ne 0) { throw "firmware build failed" }

Write-Host "-- pio run -e $Env -t buildfs"
python -m platformio run -e $Env -t buildfs
if ($LASTEXITCODE -ne 0) { throw "filesystem build failed" }

New-Item -ItemType Directory -Force -Path "dist" | Out-Null

# --- Derive the app and LittleFS partition offsets from the ACTUAL partition table for
# this build, rather than hardcoding them — a partition-scheme change (e.g. MAX motors
# growing, board_build.partitions changing) must never silently produce a bad -full- image
# again (see CHANGELOG v0.1.0: the very first -full- build omitted LittleFS entirely).
$partTable = python $genPart "$buildDir\partitions.bin" 2>&1 | Out-String
$appOffset = $null
$fsOffset  = $null
foreach ($line in ($partTable -split "`n")) {
    if ($line -match "^\w+,app,ota_0,(0x[0-9a-fA-F]+),") { $appOffset = $Matches[1] }
    if ($line -match "^\w+,data,spiffs,(0x[0-9a-fA-F]+),") { $fsOffset = $Matches[1] }
}
if (-not $appOffset -or -not $fsOffset) {
    Write-Host $partTable
    throw "Could not find app (ota_0) or spiffs partition offset — check the partition table above"
}
Write-Host "-- app partition at $appOffset, LittleFS partition at $fsOffset"

Write-Host "-- merging full image"
python $esptool --chip esp32 merge_bin `
    --flash_mode dio --flash_freq 40m --flash_size 4MB `
    -o "dist\somfy-ble-$Env-full-v$version.bin" `
    0x1000    "$buildDir\bootloader.bin" `
    0x8000    "$buildDir\partitions.bin" `
    0xe000    "$bootApp0" `
    $appOffset "$buildDir\firmware.bin" `
    $fsOffset  "$buildDir\littlefs.bin"
if ($LASTEXITCODE -ne 0) { throw "merge_bin failed" }

Copy-Item "$buildDir\firmware.bin" "dist\somfy-ble-$Env-ota-v$version.bin" -Force
Copy-Item "$buildDir\littlefs.bin" "dist\somfy-ble-$Env-littlefs-v$version.bin" -Force

if (-not $KeepOld) {
    $stale = Get-ChildItem "dist\*.bin" | Where-Object { $_.Name -notlike "*v$version*" }
    if ($stale) {
        Write-Host "-- removing $($stale.Count) old-version bin(s) from dist/ (use -KeepOld to keep them)"
        $stale | Remove-Item -Force
    }
}

Write-Host "`n== dist/ =="
Get-ChildItem "dist\*v$version*" | Format-Table Name, Length
