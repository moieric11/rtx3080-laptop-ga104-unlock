[CmdletBinding()]
param([ValidatePattern('^[A-Za-z]$')][string]$DriveLetter = 'F')

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$buildRoot = Join-Path $repoRoot 'build\ga104-cold-selector-mask'
$usbRoot = $DriveLetter.ToUpperInvariant() + ':\'
$oldHash = '4b58554e18b0a2e987040cf3c006aa122bfd84ccd0f2dbee11cb225da7340000'
function Get-Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Copy-Verified([string]$Source, [string]$Target) {
    Copy-Item -LiteralPath $Source -Destination $Target -ErrorAction Stop
    if ((Get-Sha $Source) -ne (Get-Sha $Target)) { throw "Copy mismatch: $Target" }
}

# Complete all preflight checks before changing the USB. This updater only
# migrates the known v1 image; it never formats a drive or removes run reports.
$volume = Get-Volume -DriveLetter $DriveLetter
$partition = Get-Partition -DriveLetter $DriveLetter
$disk = $partition | Get-Disk
if ($volume.FileSystem -ne 'FAT32' -or $disk.BusType -ne 'USB' -or
    $partition.IsBoot -or $partition.IsSystem -or $disk.IsBoot -or $disk.IsSystem) {
    throw 'Expected a non-system FAT32 USB volume.'
}
$boot = Join-Path $usbRoot 'EFI\BOOT\BOOTX64.EFI'
if ((Get-Sha $boot) -ne $oldHash) { throw 'USB boot differs from the expected v1 image.' }
$activePath = Join-Path $usbRoot 'ACTIVE-BOOT.json'
$oldActive = Get-Content -LiteralPath $activePath -Raw | ConvertFrom-Json
if ($oldActive.sha256 -ne $oldHash) { throw 'Unexpected active boot manifest.' }
$efi = Join-Path $buildRoot 'GA104-UNLOCK.EFI'
$verification = Get-Content (Join-Path $buildRoot 'verification.json') -Raw | ConvertFrom-Json
$newHash = Get-Sha $efi
if ($newHash -ne $verification.sha256 -or $verification.machine -ne 'AMD64' -or
    $verification.subsystem -ne 10 -or $verification.imports -ne 0) { throw 'EFI verification mismatch.' }
$testPath = Join-Path $repoRoot 'build\ga104-probe\offline-tests.json'
$tests = Get-Content $testPath -Raw | ConvertFrom-Json
if ($tests.cold_selector_mask_v2 -ne 'PASS') { throw 'Mask regression tests have not passed.' }
$assets = Get-Content (Join-Path $repoRoot 'unlock\assets.json') -Raw | ConvertFrom-Json
foreach ($asset in $assets) {
    $assetPath = Join-Path $usbRoot ('RTX3080-UNLOCK\ASSETS\' + $asset.Name)
    if ((Get-Item -LiteralPath $assetPath).Length -ne $asset.Bytes -or
        (Get-Sha $assetPath) -ne $asset.Sha256) { throw "Wrong USB firmware: $($asset.Name)" }
}
$backup = Join-Path $usbRoot 'EFI\TOOLS\COLD-SELECTORS-V1-BACKUP'
if (Test-Path -LiteralPath $backup) { throw 'Backup exists; inspect the previous update first.' }
$sourceNames = @('unlock/board_profile.h', 'ga104_unlock_app.c', 'tools/build_ga104_unlock.py',
    'tools/test_ga104_unlock_callback.c', 'tools/test_ga104_port.py',
    'tools/update_ga104_selector_usb.ps1', 'GA104-COLD-SELECTORS.md', 'README.md')
$sourceHashes = [ordered]@{}
foreach ($name in $sourceNames) { $sourceHashes[$name] = Get-Sha (Join-Path $repoRoot $name) }
$manifest = [ordered]@{
    variant = 'cold-selector-mask-v2'; sha256 = $newHash; hardware_tested = $false
    ss0_rule = 'SS0 != 0 && (SS0 & 0x88888888) == 0'; required_ss1 = 6
    ecc_readout = $false; other_cold_gates_unchanged = $true
    observed_ss0 = @('0x61014100','0x60014100','0x61004100','0x61014101','0x60004100')
    verification = $verification; offline_tests = $tests; source_sha256 = $sourceHashes
    previous_sha256 = $oldHash; backup = 'EFI/TOOLS/COLD-SELECTORS-V1-BACKUP'
    installed_utc = [DateTime]::UtcNow.ToString('o')
}
$localManifest = Join-Path $buildRoot 'ACTIVE-BOOT.json'
$manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $localManifest -Encoding UTF8
$restore = Join-Path $buildRoot 'RESTAURER-SELECTEURS-V1.cmd'
@'
@echo off
copy /y "%~dp0EFI\TOOLS\COLD-SELECTORS-V1-BACKUP\BOOTX64.EFI" "%~dp0EFI\BOOT\BOOTX64.EFI"
if errorlevel 1 exit /b 1
copy /y "%~dp0EFI\TOOLS\COLD-SELECTORS-V1-BACKUP\ACTIVE-BOOT.json" "%~dp0ACTIVE-BOOT.json"
if errorlevel 1 exit /b 1
copy /y "%~dp0EFI\TOOLS\COLD-SELECTORS-V1-BACKUP\UNLOCK-CORRECTION-LIRE-MOI.md" "%~dp0UNLOCK-CORRECTION-LIRE-MOI.md"
if errorlevel 1 exit /b 1
echo Selecteurs v1 restaures.
'@ | Set-Content -LiteralPath $restore -Encoding ASCII

New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Verified $boot (Join-Path $backup 'BOOTX64.EFI')
Copy-Verified $activePath (Join-Path $backup 'ACTIVE-BOOT.json')
Copy-Verified (Join-Path $usbRoot 'UNLOCK-CORRECTION-LIRE-MOI.md') (Join-Path $backup 'UNLOCK-CORRECTION-LIRE-MOI.md')
Copy-Verified $restore (Join-Path $usbRoot 'RESTAURER-SELECTEURS-V1.cmd')
Copy-Verified $efi (Join-Path $usbRoot 'EFI\TOOLS\GA104-COLD-SELECTOR-MASK-V2.EFI')
Copy-Verified $efi $boot
Copy-Verified $localManifest $activePath
Copy-Verified (Join-Path $repoRoot 'GA104-COLD-SELECTORS.md') (Join-Path $usbRoot 'UNLOCK-CORRECTION-LIRE-MOI.md')
Write-Output "USB updated; boot SHA256 verified: $newHash"
Write-Output "Previous boot and manifest backed up: $backup"
