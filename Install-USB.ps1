[CmdletBinding()]
param(
    [ValidatePattern('^[A-Za-z]$')][string]$DriveLetter,
    [switch]$VerifyOnly
)
$ErrorActionPreference = 'Stop'
try {
    $packageRoot = $PSScriptRoot
    $manifestPath = Join-Path $packageRoot 'SHA256-MANIFEST.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $entries = @($manifest.PSObject.Properties)
    if ($entries.Count -lt 8) { throw 'Incomplete package manifest.' }
    foreach ($entry in $entries) {
        if ($entry.Name -match '(^[/\\]|:|(^|[/\\])\.\.([/\\]|$))' -or
            $entry.Value -notmatch '^[a-f0-9]{64}$') { throw 'Invalid manifest entry.' }
        $source = Join-Path $packageRoot $entry.Name
        if (!(Test-Path -LiteralPath $source -PathType Leaf) -or
            (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $entry.Value) {
            throw "Missing or changed package file: $($entry.Name)"
        }
    }
    Write-Host "Package verified: $($entries.Count) files."
    if ($VerifyOnly) { exit 0 }
    if (!$DriveLetter) { throw 'Specify -DriveLetter F (your USB letter), or -VerifyOnly.' }
    $letter = $DriveLetter.ToUpperInvariant()
    $volume = Get-Volume -DriveLetter $letter
    $partition = Get-Partition -DriveLetter $letter
    $disk = $partition | Get-Disk
    if ($volume.FileSystem -ne 'FAT32' -or $disk.BusType -ne 'USB' -or
        $partition.IsBoot -or $partition.IsSystem -or $disk.IsBoot -or $disk.IsSystem) {
        throw 'The target must be a FAT32 USB volume, not a boot/system disk.'
    }
    $target = "${letter}:\"
    if ([IO.Path]::GetPathRoot($packageRoot) -eq $target) { throw 'Run the assistant from your extracted download, not the target USB.' }
    $names = @($entries | ForEach-Object { $_.Name }) + 'SHA256-MANIFEST.json'
    $requiredBytes = 0L
    foreach ($name in $names) {
        $dest = Join-Path $target $name
        if (Test-Path -LiteralPath $dest) { throw "Target file already exists: $dest. Use a separate USB." }
        # Do not follow a target directory junction into another location.
        $parent = Split-Path -Parent $dest
        while ($parent -and $parent -ne $target) {
            if ((Test-Path -LiteralPath $parent) -and
                ((Get-Item -LiteralPath $parent -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
                throw "Target directory is a reparse point: $parent"
            }
            $parent = Split-Path -Parent $parent
        }
        $requiredBytes += (Get-Item -LiteralPath (Join-Path $packageRoot $name)).Length
    }
    if ($volume.SizeRemaining -lt ($requiredBytes + 1MB)) { throw 'Not enough free space on the USB.' }
    foreach ($name in $names) {
        $dest = Join-Path $target $name
        [IO.Directory]::CreateDirectory((Split-Path -Parent $dest)) | Out-Null
        # File.Copy with overwrite=false refuses files created after the preflight.
        [IO.File]::Copy((Join-Path $packageRoot $name), $dest, $false)
    }
    foreach ($entry in $entries) {
        if ((Get-FileHash -LiteralPath (Join-Path $target $entry.Name) -Algorithm SHA256).Hash -ne $entry.Value) {
            throw "USB verification failed: $($entry.Name)"
        }
    }
    Write-Host "USB $target prepared and verified. Read INSTALL.md before booting."
    exit 0
} catch {
    Write-Error -Message $_.Exception.Message -ErrorAction Continue
    exit 1
}
