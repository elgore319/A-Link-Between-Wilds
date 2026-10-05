<#
.SYNOPSIS
    Zips your Ryujinx data folder so the installer can restore it on another PC.

.DESCRIPTION
    Packs everything in %APPDATA%\Ryujinx (keys, firmware, saves, settings, mods, profiles)
    into ryujinx-transfer-<date>.zip on your Desktop. On the new PC, run the Ryujinx
    installer and choose "Moving from another PC", then pick this zip.

    THE ZIP CONTAINS YOUR PERSONAL KEYS. Move it with a USB stick or your own network.
    Don't upload it, share it, or commit it (.gitignore blocks ryujinx-transfer-*.zip).

.EXAMPLE
    .\export-ryujinx-data.ps1

.EXAMPLE
    .\export-ryujinx-data.ps1 -OutDir E:\ -SkipCaches
#>
[CmdletBinding()]
param(
    # Where to put the zip. Default: your Desktop.
    [string]$OutDir = [Environment]::GetFolderPath('Desktop'),

    # Ryujinx data folder. Default: %APPDATA%\Ryujinx.
    [string]$RyujinxDir = (Join-Path $env:APPDATA 'Ryujinx'),

    # Leave out per-game shader/PPTC caches (games\*\cache). Smaller zip; Ryujinx rebuilds them.
    [switch]$SkipCaches
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path (Join-Path $RyujinxDir 'Config.json'))) {
    throw "No Ryujinx data found at $RyujinxDir (no Config.json). Pass -RyujinxDir if it's somewhere else."
}
if (@(Get-Process -Name 'Ryujinx*' -ErrorAction SilentlyContinue).Count -gt 0) {
    throw 'Ryujinx is running. Close it first so saves and settings are fully written.'
}

$out = Join-Path $OutDir ('ryujinx-transfer-{0:yyyy-MM-dd}.zip' -f (Get-Date))
if (Test-Path $out) { Remove-Item -Force $out }

# Zip the folder's contents (not the folder itself) so the installer can extract straight
# into %APPDATA%\Ryujinx. ZipFile is used instead of Compress-Archive because it keeps
# empty folders and handles large files reliably on Windows PowerShell 5.1.
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$root = (Resolve-Path $RyujinxDir).Path.TrimEnd('\') + '\'
$zip = [System.IO.Compression.ZipFile]::Open($out, 'Create')
try {
    $count = 0
    foreach ($item in Get-ChildItem -LiteralPath $root -Recurse -Force) {
        $rel = $item.FullName.Substring($root.Length)
        if ($SkipCaches -and $rel -match '^games\\[^\\]+\\cache(\\|$)') { continue }
        $entry = $rel -replace '\\', '/'
        if ($item.PSIsContainer) {
            if (-not (Get-ChildItem -LiteralPath $item.FullName -Force | Select-Object -First 1)) {
                $zip.CreateEntry("$entry/") | Out-Null   # keep empty folders (mods\, sdcard\ ...)
            }
        } else {
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $item.FullName, $entry, 'Optimal') | Out-Null
            $count++
        }
    }
} finally {
    $zip.Dispose()
}

$size = '{0:N0} MB' -f ((Get-Item $out).Length / 1MB)
Write-Host "Saved $count files ($size) to:`n  $out"
Write-Host "`nThis zip has your keys in it. Copy it to the new PC yourself; don't upload or share it." -ForegroundColor Yellow
