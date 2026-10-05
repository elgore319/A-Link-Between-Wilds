<# : move-ryujinx.bat - just double-click this file. (This first part is for Windows; the rest is PowerShell.)
@echo off
setlocal
set "ALBW_SELF=%~f0"
powershell -NoProfile -ExecutionPolicy Bypass -Command "& ([ScriptBlock]::Create([IO.File]::ReadAllText($env:ALBW_SELF)))"
echo.
pause
exit /b
#>

<#
.SYNOPSIS
    Moves your own Ryujinx setup (emulator, keys, firmware, saves, mods and your
    Breath of the Wild files) from one of your PCs to another.

.DESCRIPTION
    Double-click on the OLD PC: it finds Ryujinx and BotW, asks where to put the
    copy (USB drive or a shared folder) and makes a folder "ALBW Ryujinx Transfer".
    Move that folder to the NEW PC and double-click move-ryujinx.bat inside it:
    it installs everything, fixes the game paths and makes a desktop shortcut.

    The transfer folder holds your keys and your game. It's for moving between
    your own PCs; never upload or share it. It's never meant to go into the repo.

    Part of A Link Between Wilds (tools/windows/). Developer options below are for
    testing; players never need them.
#>
param(
    [ValidateSet('Auto', 'Pack', 'Unpack')] [string]$Mode = 'Auto',
    [string]$RyujinxExe,          # pack: skip the search
    [string[]]$GamePaths,         # pack: skip the game search (base, update, DLC files)
    [string]$Destination,         # pack: folder to create the transfer folder in
    [string]$TransferDir,         # unpack: transfer folder (default: next to this script)
    [string]$InstallRoot,         # unpack: where Ryujinx goes (default: %LOCALAPPDATA%\Programs\Ryujinx)
    [string]$GamesDir,            # unpack: where game files go (default: %USERPROFILE%\Ryujinx Games)
    [switch]$Yes                  # answer yes to every question (tests)
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$BotwTitleId     = '01007ef00011e000'
$TransferName    = 'ALBW Ryujinx Transfer'
$ManifestName    = 'manifest.json'
$ScriptName      = 'move-ryujinx.bat'
$GameExtensions  = @('.nsp', '.xci', '.nsz', '.xcz')
$GameNamePattern = '(?i)zelda|breath.?of.?the.?wild|botw|01007ef00011e000'
$IsWin           = [System.Environment]::OSVersion.Platform -eq 'Win32NT'
$Self            = $env:ALBW_SELF

# ---------------------------------------------------------------- talking to the player

function Say([string]$msg, [string]$color = 'Gray') { Write-Host $msg -ForegroundColor $color }
function Step([string]$msg) { Write-Host ''; Write-Host "== $msg" -ForegroundColor Cyan }
function Good([string]$msg) { Write-Host "   OK  $msg" -ForegroundColor Green }
function Warn([string]$msg) { Write-Host "   !   $msg" -ForegroundColor Yellow }

function Stop-WithMessage([string]$msg) {
    Write-Host ''
    Write-Host 'Stopped:' -ForegroundColor Red
    Write-Host "   $msg" -ForegroundColor Red
    Write-Host ''
    Write-Host 'Nothing was deleted. Fix the above and double-click the file again.'
    exit 1
}

function Ask-YesNo([string]$question) {
    if ($Yes) { return $true }
    while ($true) {
        $a = Read-Host "$question (y/n)"
        if ($a -match '^(y|yes)$') { return $true }
        if ($a -match '^(n|no)$')  { return $false }
    }
}

function Format-Size([double]$bytes) {
    if ($bytes -ge 1GB) { return ('{0:N1} GB' -f ($bytes / 1GB)) }
    if ($bytes -ge 1MB) { return ('{0:N0} MB' -f ($bytes / 1MB)) }
    return ('{0:N0} KB' -f ($bytes / 1KB))
}

function Pick-File([string]$title, [string]$filter) {
    if (-not $IsWin) { return $null }
    Add-Type -AssemblyName System.Windows.Forms
    $dlg = New-Object System.Windows.Forms.OpenFileDialog
    $dlg.Title = $title
    $dlg.Filter = $filter
    if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { return $dlg.FileName }
    return $null
}

function Pick-Files([string]$title, [string]$filter) {
    if (-not $IsWin) { return @() }
    Add-Type -AssemblyName System.Windows.Forms
    $dlg = New-Object System.Windows.Forms.OpenFileDialog
    $dlg.Title = $title
    $dlg.Filter = $filter
    $dlg.Multiselect = $true
    if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { return @($dlg.FileNames) }
    return @()
}

function Pick-Folder([string]$description) {
    if (-not $IsWin) { return $null }
    Add-Type -AssemblyName System.Windows.Forms
    $dlg = New-Object System.Windows.Forms.FolderBrowserDialog
    $dlg.Description = $description
    $dlg.ShowNewFolderButton = $true
    if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { return $dlg.SelectedPath }
    return $null
}

# ---------------------------------------------------------------- files

function Get-RelativePath([string]$root, [string]$path) {
    $r = [System.IO.Path]::GetFullPath($root).TrimEnd('\', '/')
    $p = [System.IO.Path]::GetFullPath($path)
    if (-not $p.StartsWith($r, [System.StringComparison]::OrdinalIgnoreCase)) { throw "$path is not inside $root" }
    return $p.Substring($r.Length).TrimStart('\', '/')
}

# Copies one file with a progress line (game files are many GB; Copy-Item shows nothing).
function Copy-WithProgress([string]$src, [string]$dst, [string]$label) {
    $dir = Split-Path $dst -Parent
    if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    $in = [System.IO.File]::Open($src, 'Open', 'Read', 'Read')
    try {
        $out = [System.IO.File]::Open($dst, 'Create', 'Write', 'None')
        try {
            $total = [double]$in.Length
            $buf = New-Object byte[] (4MB)
            $done = 0.0; $lastPct = -1
            while (($n = $in.Read($buf, 0, $buf.Length)) -gt 0) {
                $out.Write($buf, 0, $n)
                $done += $n
                $pct = if ($total -gt 0) { [int](100 * $done / $total) } else { 100 }
                if ($pct -ne $lastPct -and $total -gt 50MB) {
                    Write-Progress -Activity "Copying $label" -Status "$pct% of $(Format-Size $total)" -PercentComplete $pct
                    $lastPct = $pct
                }
            }
        } finally { $out.Dispose() }
    } finally { $in.Dispose() }
    Write-Progress -Activity "Copying $label" -Completed
    (Get-Item -LiteralPath $dst).LastWriteTime = (Get-Item -LiteralPath $src).LastWriteTime
}

# Copies a folder tree, skipping anything whose relative path matches $exclude.
function Copy-Tree([string]$src, [string]$dst, [string[]]$exclude, [string]$label) {
    $files = @(Get-ChildItem -LiteralPath $src -Recurse -File -Force -ErrorAction SilentlyContinue)
    $kept = @()
    foreach ($f in $files) {
        $rel = Get-RelativePath $src $f.FullName
        $relSlash = $rel -replace '\\', '/'
        $skip = $false
        foreach ($pattern in $exclude) { if ($relSlash -match $pattern) { $skip = $true; break } }
        if (-not $skip) { $kept += , @($f, $rel) }
    }
    $total = [double](($kept | ForEach-Object { $_[0].Length } | Measure-Object -Sum).Sum)
    $done = 0.0; $i = 0
    foreach ($item in $kept) {
        $f = $item[0]; $rel = $item[1]
        $i++
        if ($f.Length -gt 200MB) {
            Copy-WithProgress $f.FullName (Join-Path $dst $rel) $f.Name
        } else {
            $target = Join-Path $dst $rel
            $dir = Split-Path $target -Parent
            if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
            Copy-Item -LiteralPath $f.FullName -Destination $target -Force
        }
        $done += $f.Length
        if ($i % 50 -eq 0 -and $total -gt 0) {
            Write-Progress -Activity "Copying $label" -Status "$i of $($kept.Count) files" -PercentComplete ([int](100 * $done / $total))
        }
    }
    Write-Progress -Activity "Copying $label" -Completed
    return [pscustomobject]@{ Files = $kept.Count; Bytes = $total }
}

# Folders inside the Ryujinx data folder that are safe to leave behind: they rebuild
# themselves (logs, shader caches) and can be several GB.
$DataExcludes = @('^Logs/', '^games/[^/]+/cache/', '^portable/Logs/')

# ---------------------------------------------------------------- finding things (pack)

function Find-RyujinxExe {
    Step 'Looking for Ryujinx'
    if ($RyujinxExe) {
        if (Test-Path -LiteralPath $RyujinxExe) { return (Resolve-Path -LiteralPath $RyujinxExe).Path }
        Stop-WithMessage "Ryujinx.exe isn't at $RyujinxExe."
    }

    $found = New-Object System.Collections.Generic.List[string]

    # 1. Running right now?
    foreach ($p in @(Get-Process -Name 'Ryujinx*' -ErrorAction SilentlyContinue)) {
        try { if ($p.Path) { $found.Add($p.Path) } } catch { }
    }

    # 2. Shortcuts on the desktop / Start menu.
    if ($IsWin) {
        $shell = New-Object -ComObject WScript.Shell
        $lnkDirs = @([Environment]::GetFolderPath('Desktop'), [Environment]::GetFolderPath('CommonDesktopDirectory'),
                     [Environment]::GetFolderPath('Programs'), [Environment]::GetFolderPath('CommonPrograms'))
        foreach ($d in $lnkDirs) {
            if (-not $d -or -not (Test-Path -LiteralPath $d)) { continue }
            foreach ($lnk in @(Get-ChildItem -LiteralPath $d -Filter '*.lnk' -Recurse -Depth 2 -ErrorAction SilentlyContinue)) {
                try {
                    $t = $shell.CreateShortcut($lnk.FullName).TargetPath
                    if ($t -and $t -match '(?i)ryujinx[^\\]*\.exe$' -and (Test-Path -LiteralPath $t)) { $found.Add($t) }
                } catch { }
            }
        }
    }

    # 3. The usual places people unzip it.
    if ($found.Count -eq 0) {
        $roots = @()
        foreach ($name in @('Desktop', 'Downloads', 'Documents')) { $roots += (Join-Path $env:USERPROFILE $name) }
        $roots += $env:LOCALAPPDATA, (Join-Path $env:LOCALAPPDATA 'Programs'), $env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:USERPROFILE
        if ($IsWin) { foreach ($drive in [System.IO.DriveInfo]::GetDrives()) { if ($drive.DriveType -eq 'Fixed' -and $drive.IsReady) { $roots += $drive.RootDirectory.FullName } } }
        foreach ($root in ($roots | Where-Object { $_ } | Select-Object -Unique)) {
            if (-not (Test-Path -LiteralPath $root)) { continue }
            Say "   searching $root ..." 'DarkGray'
            foreach ($f in @(Get-ChildItem -LiteralPath $root -Filter 'Ryujinx.exe' -Recurse -Depth 3 -File -ErrorAction SilentlyContinue)) { $found.Add($f.FullName) }
            if ($found.Count -gt 0) { break }
        }
    }

    $unique = @($found | Select-Object -Unique | Sort-Object { (Get-Item -LiteralPath $_).LastWriteTime } -Descending)
    if ($unique.Count -ge 1) {
        if ($unique.Count -gt 1) { Warn "Found more than one Ryujinx; using the newest: $($unique[0])" }
        Good "Ryujinx: $($unique[0])"
        return $unique[0]
    }

    Warn "Couldn't find Ryujinx automatically. A window will open: pick Ryujinx.exe."
    $picked = Pick-File 'Where is Ryujinx.exe?' 'Ryujinx|Ryujinx*.exe|Programs|*.exe'
    if (-not $picked) { Stop-WithMessage "Couldn't find Ryujinx.exe. Find the folder you run Ryujinx from and try again." }
    return $picked
}

function Get-DataDir([string]$exe) {
    $portable = Join-Path (Split-Path $exe -Parent) 'portable'
    if (Test-Path -LiteralPath $portable) { return @{ Path = $portable; Portable = $true } }
    $appdata = Join-Path $env:APPDATA 'Ryujinx'
    if (Test-Path -LiteralPath $appdata) { return @{ Path = $appdata; Portable = $false } }
    Stop-WithMessage "Found Ryujinx, but not its data folder (keys, saves). Looked in $portable and $appdata. Has Ryujinx been run on this PC?"
}

function Read-JsonFile([string]$path) {
    try { return (Get-Content -LiteralPath $path -Raw | ConvertFrom-Json) } catch { return $null }
}

function Find-GameFiles([string]$dataDir) {
    Step 'Looking for Breath of the Wild'
    if ($GamePaths) {
        foreach ($g in $GamePaths) { if (-not (Test-Path -LiteralPath $g)) { Stop-WithMessage "Game file not found: $g" } }
        return @($GamePaths | ForEach-Object { (Resolve-Path -LiteralPath $_).Path })
    }

    $found = New-Object System.Collections.Generic.List[string]

    # Update and DLC files Ryujinx was told about (absolute paths in these JSON files).
    $titleDir = Join-Path (Join-Path $dataDir 'games') $BotwTitleId
    foreach ($json in @('updates.json', 'dlc.json')) {
        $p = Join-Path $titleDir $json
        if (Test-Path -LiteralPath $p) {
            $text = Get-Content -LiteralPath $p -Raw
            foreach ($m in [regex]::Matches($text, '"([^"]+\.(?:nsp|xci|nsz|xcz))"', 'IgnoreCase')) {
                $path = $m.Groups[1].Value -replace '\\\\', '\'
                if (Test-Path -LiteralPath $path) { $found.Add($path) }
            }
        }
    }

    # Game folders from Ryujinx's settings.
    $config = Read-JsonFile (Join-Path $dataDir 'Config.json')
    $dirs = @()
    if ($config -and ($config.PSObject.Properties.Name -contains 'game_dirs')) { $dirs = @($config.game_dirs) }
    foreach ($d in $dirs) {
        if (-not $d -or -not (Test-Path -LiteralPath $d)) { continue }
        foreach ($f in @(Get-ChildItem -LiteralPath $d -Recurse -File -ErrorAction SilentlyContinue)) {
            if ($GameExtensions -contains $f.Extension.ToLower() -and $f.Name -match $GameNamePattern) { $found.Add($f.FullName) }
        }
    }

    $unique = @($found | Select-Object -Unique)
    if ($unique.Count -gt 0) {
        foreach ($g in $unique) { Good "$(Split-Path $g -Leaf)  ($(Format-Size (Get-Item -LiteralPath $g).Length))" }
        if ($unique.Count -eq 1) {
            Warn 'Only one BotW file found. BotW 1.6.0 is usually a base game file plus an update file.'
            if (Ask-YesNo '   Add more files (e.g. the 1.6.0 update) with a file picker?') {
                $unique += @(Pick-Files 'Pick your other BotW files (update, DLC)' 'Switch games|*.nsp;*.xci;*.nsz;*.xcz')
            }
        }
        return @($unique | Select-Object -Unique)
    }

    Warn "Couldn't find your BotW files automatically. A window will open: pick the game file AND the 1.6.0 update (hold Ctrl to pick several)."
    $picked = @(Pick-Files 'Pick your Breath of the Wild files (base game + 1.6.0 update, hold Ctrl)' 'Switch games|*.nsp;*.xci;*.nsz;*.xcz')
    if ($picked.Count -eq 0) {
        if (Ask-YesNo '   No game picked. Continue without the game (copy it yourself later)?') { return @() }
        Stop-WithMessage 'No game files picked.'
    }
    return $picked
}

# ---------------------------------------------------------------- pack

function Invoke-Pack {
    Say ''
    Say 'Move Ryujinx to another PC: step 1 of 2 (on the OLD PC)' 'White'
    Say 'This copies Ryujinx, your keys, firmware, saves, mods and BotW into one folder.'

    if (@(Get-Process -Name 'Ryujinx*' -ErrorAction SilentlyContinue).Count -gt 0) {
        Warn 'Ryujinx is open. Close it first so your latest save and settings are copied.'
        if (-not (Ask-YesNo '   Continue anyway?')) { Stop-WithMessage 'Close Ryujinx and double-click the file again.' }
    }

    $exe = Find-RyujinxExe
    $exeDir = Split-Path $exe -Parent
    $data = Get-DataDir $exe
    Good "Data folder: $($data.Path)$(if ($data.Portable) { ' (portable)' })"
    foreach ($must in @((Join-Path 'system' 'prod.keys'))) {
        if (-not (Test-Path -LiteralPath (Join-Path $data.Path $must))) { Warn "No $must in the data folder. The game won't start without keys." }
    }
    $games = @(Find-GameFiles $data.Path)

    # Sizes.
    $gameBytes = [double](($games | ForEach-Object { (Get-Item -LiteralPath $_).Length } | Measure-Object -Sum).Sum)
    $dataBytes = [double]((Get-ChildItem -LiteralPath $data.Path -Recurse -File -Force -ErrorAction SilentlyContinue |
                  Where-Object { $rel = (Get-RelativePath $data.Path $_.FullName) -replace '\\', '/'; -not ($DataExcludes | Where-Object { $rel -match $_ }) } |
                  Measure-Object -Property Length -Sum).Sum)
    $exeBytes  = [double]((Get-ChildItem -LiteralPath $exeDir -Recurse -File -Force -ErrorAction SilentlyContinue |
                  Where-Object { -not $data.Portable -or -not $_.FullName.StartsWith($data.Path, [StringComparison]::OrdinalIgnoreCase) } |
                  Measure-Object -Property Length -Sum).Sum)
    $needed = $gameBytes + $dataBytes + $exeBytes
    Say ''
    Say "   Total to copy: $(Format-Size $needed)  (game $(Format-Size $gameBytes), data $(Format-Size $dataBytes), Ryujinx $(Format-Size $exeBytes))"

    # Where to.
    Step 'Where should the copy go?'
    $dest = $Destination
    if (-not $dest) {
        Say '   A window will open. Pick a USB drive or a shared network folder.'
        $dest = Pick-Folder "Pick where to put the transfer folder (USB drive or shared folder). It needs $(Format-Size $needed) free."
        if (-not $dest) { Stop-WithMessage 'No folder picked.' }
    }
    if (-not (Test-Path -LiteralPath $dest)) { Stop-WithMessage "Folder doesn't exist: $dest" }
    $out = Join-Path $dest $TransferName
    if (Test-Path -LiteralPath $out) {
        if (-not (Ask-YesNo "   '$TransferName' already exists there. Replace it?")) { Stop-WithMessage "Pick a different place, or delete the old '$TransferName' folder." }
        Remove-Item -LiteralPath $out -Recurse -Force
    }

    if ($IsWin -and -not $dest.StartsWith('\\')) {
        $drive = New-Object System.IO.DriveInfo ([System.IO.Path]::GetPathRoot((Resolve-Path -LiteralPath $dest).Path))
        if ($drive.AvailableFreeSpace -lt $needed) {
            Stop-WithMessage "Not enough space on $($drive.Name): needs $(Format-Size $needed), has $(Format-Size $drive.AvailableFreeSpace)."
        }
        $biggest = ($games | ForEach-Object { (Get-Item -LiteralPath $_).Length } | Measure-Object -Maximum).Maximum
        if ($drive.DriveFormat -eq 'FAT32' -and $biggest -ge 4GB) {
            Stop-WithMessage ("$($drive.Name) is formatted FAT32, which can't hold files over 4 GB, and your game file is $(Format-Size $biggest). " +
                              "Use a shared network folder instead, or reformat the USB drive as exFAT (this erases it).")
        }
    }

    # Copy.
    New-Item -ItemType Directory -Force -Path $out | Out-Null
    Step 'Copying Ryujinx'
    $exeExcl = @('^Logs/')
    if ($data.Portable) { $exeExcl += '^portable/' }
    $r1 = Copy-Tree $exeDir (Join-Path $out 'Ryujinx') $exeExcl 'Ryujinx'
    Good "$($r1.Files) files"

    Step 'Copying keys, firmware, saves and mods'
    $r2 = Copy-Tree $data.Path (Join-Path $out 'data') $DataExcludes 'Ryujinx data'
    Good "$($r2.Files) files (logs and shader caches skipped; they rebuild themselves)"

    $gameEntries = @()
    if ($games.Count -gt 0) {
        Step 'Copying Breath of the Wild (the slow part)'
        $n = 0
        foreach ($g in $games) {
            $n++
            $leaf = Split-Path $g -Leaf
            Say "   [$n/$($games.Count)] $leaf"
            Copy-WithProgress $g (Join-Path (Join-Path $out 'games') $leaf) $leaf
            $gameEntries += [pscustomobject]@{ file = $leaf; original_path = $g; bytes = (Get-Item -LiteralPath $g).Length }
        }
        Good 'Game copied'
    }

    $manifest = [pscustomobject]@{
        format         = 1
        created        = (Get-Date).ToString('s')
        source_pc      = $env:COMPUTERNAME
        ryujinx_exe    = (Split-Path $exe -Leaf)
        original_exe   = $exe
        portable       = [bool]$data.Portable
        original_data  = $data.Path
        original_game_dirs = @()
        games          = $gameEntries
    }
    $config = Read-JsonFile (Join-Path $data.Path 'Config.json')
    if ($config -and ($config.PSObject.Properties.Name -contains 'game_dirs')) { $manifest.original_game_dirs = @($config.game_dirs) }
    [System.IO.File]::WriteAllText((Join-Path $out $ManifestName), ($manifest | ConvertTo-Json -Depth 5))

    if ($Self -and (Test-Path -LiteralPath $Self)) { Copy-Item -LiteralPath $Self -Destination (Join-Path $out $ScriptName) -Force }
    @"
A Link Between Wilds - Ryujinx transfer
=======================================
Made on $env:COMPUTERNAME at $((Get-Date).ToString('g')).

ON YOUR OTHER PC: copy this whole folder there (or open it from the USB drive)
and double-click  $ScriptName  inside it.

This folder contains your Switch keys and your game. It's only for moving between
your own PCs: don't upload it, share it, or put it in the GitHub repo.
"@ | Set-Content -LiteralPath (Join-Path $out 'READ ME.txt')

    Say ''
    Say "Done. Your transfer folder is: $out" 'Green'
    Say "Next: on the other PC, open that folder and double-click $ScriptName." 'Green'
    Say 'It has your keys and game in it, so keep it to yourself.' 'Yellow'
    if ($IsWin) { Start-Process explorer.exe $out }
}

# ---------------------------------------------------------------- unpack

function Update-PathsInJson([string]$file, [hashtable]$map) {
    $text = [System.IO.File]::ReadAllText($file)
    $orig = $text
    foreach ($old in $map.Keys) {
        $new = $map[$old]
        # JSON stores backslashes doubled; also catch forward-slash spellings.
        $text = $text.Replace(($old -replace '\\', '\\'), ($new -replace '\\', '\\'))
        $text = $text.Replace(($old -replace '\\', '/'), ($new -replace '\\', '/'))
        $text = $text.Replace($old, $new)
    }
    if ($text -ne $orig) { [System.IO.File]::WriteAllText($file, $text); return $true }
    return $false
}

function Invoke-Unpack([string]$src) {
    Say ''
    Say 'Move Ryujinx to another PC: step 2 of 2 (on the NEW PC)' 'White'

    $manifest = Read-JsonFile (Join-Path $src $ManifestName)
    if (-not $manifest) { Stop-WithMessage "$ManifestName in $src is missing or damaged. Make the transfer folder again on the old PC." }
    $from = $manifest.source_pc; if (-not $from) { $from = 'your other PC' }
    Say "   Copy made on $from ($($manifest.created))."

    if (@(Get-Process -Name 'Ryujinx*' -ErrorAction SilentlyContinue).Count -gt 0) {
        Stop-WithMessage 'Ryujinx is open on this PC. Close it and double-click the file again.'
    }

    $installRoot = $InstallRoot
    if (-not $installRoot) { $installRoot = Join-Path (Join-Path $env:LOCALAPPDATA 'Programs') 'Ryujinx' }
    $gamesDir = $GamesDir
    if (-not $gamesDir) { $gamesDir = Join-Path $env:USERPROFILE 'Ryujinx Games' }
    if ($manifest.portable) { $dataDir = Join-Path $installRoot 'portable' } else { $dataDir = Join-Path $env:APPDATA 'Ryujinx' }

    Say ''
    Say "   Ryujinx will go to:      $installRoot"
    Say "   Keys, saves, mods go to: $dataDir"
    Say "   The game goes to:        $gamesDir"
    if (-not (Ask-YesNo '   OK to install there?')) { Stop-WithMessage 'Cancelled. Nothing was changed.' }

    # Keep anything already there instead of overwriting it.
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    foreach ($existing in @($installRoot, $dataDir)) {
        if (Test-Path -LiteralPath $existing) {
            $backup = "$existing.before-transfer-$stamp"
            Rename-Item -LiteralPath $existing -NewName (Split-Path $backup -Leaf)
            Warn "There was already something at $existing; moved it to $backup"
        }
    }

    Step 'Installing Ryujinx'
    $r1 = Copy-Tree (Join-Path $src 'Ryujinx') $installRoot @() 'Ryujinx'
    Good "$($r1.Files) files"

    Step 'Installing keys, firmware, saves and mods'
    $r2 = Copy-Tree (Join-Path $src 'data') $dataDir @() 'Ryujinx data'
    Good "$($r2.Files) files"

    $pathMap = @{}
    $games = @($manifest.games)
    if ($games.Count -gt 0) {
        Step 'Installing Breath of the Wild (the slow part)'
        $n = 0
        foreach ($g in $games) {
            $n++
            $from = Join-Path (Join-Path $src 'games') $g.file
            $to = Join-Path $gamesDir $g.file
            Say "   [$n/$($games.Count)] $($g.file)"
            if (-not (Test-Path -LiteralPath $from)) { Warn "Missing from the transfer folder: $($g.file). Skipping."; continue }
            Copy-WithProgress $from $to $g.file
            $pathMap[$g.original_path] = $to
        }
        Good 'Game installed'
    }

    Step 'Pointing Ryujinx at the new locations'
    # Update/DLC lists hold absolute paths to the game files.
    $fixed = 0
    $gamesData = Join-Path $dataDir 'games'
    if ($pathMap.Count -gt 0 -and (Test-Path -LiteralPath $gamesData)) {
        foreach ($j in @(Get-ChildItem -LiteralPath $gamesData -Recurse -File -Filter '*.json' -ErrorAction SilentlyContinue)) {
            if (Update-PathsInJson $j.FullName $pathMap) { $fixed++ }
        }
    }
    # Game folder list in Config.json: replace with the new games folder.
    $configPath = Join-Path $dataDir 'Config.json'
    if (Test-Path -LiteralPath $configPath) {
        # Only the game_dirs entry is rewritten; the rest of the file stays byte-for-byte
        # as Ryujinx wrote it (no re-serializing, no BOM).
        $text = [System.IO.File]::ReadAllText($configPath)
        $newArray = '"game_dirs": [ ' + (ConvertTo-Json -InputObject $gamesDir -Compress) + ' ]'
        $rx = [regex]'"game_dirs"\s*:\s*\[[^\]]*\]'
        if ($rx.IsMatch($text)) {
            [System.IO.File]::WriteAllText($configPath, $rx.Replace($text, $newArray.Replace('$', '$$'), 1))
            $fixed++
        } else {
            Warn "Couldn't update the game folder in Ryujinx's settings. In Ryujinx: Options > Settings > General > Game Directories > Add: $gamesDir"
        }
    }
    Good "$fixed settings file(s) updated"

    $exe = Join-Path $installRoot $manifest.ryujinx_exe
    if ($IsWin) {
        Step 'Making a desktop shortcut'
        $lnkPath = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Ryujinx.lnk'
        $shell = New-Object -ComObject WScript.Shell
        $lnk = $shell.CreateShortcut($lnkPath)
        $lnk.TargetPath = $exe
        $lnk.WorkingDirectory = $installRoot
        $lnk.Save()
        Good $lnkPath
    }

    Say ''
    Say 'Done. Open Ryujinx from the desktop shortcut; Breath of the Wild should be in the list.' 'Green'
    Say "You can delete the transfer folder ($src) once the game runs. It has your keys in it." 'Yellow'
}

# ---------------------------------------------------------------- main

try {
    $here = $null
    if ($Self) { $here = Split-Path $Self -Parent }
    $src = $TransferDir
    if (-not $src -and $here -and (Test-Path -LiteralPath (Join-Path $here $ManifestName))) { $src = $here }

    $mode = $Mode
    if ($mode -eq 'Auto') { if ($src) { $mode = 'Unpack' } else { $mode = 'Pack' } }
    if ($mode -eq 'Unpack') {
        if (-not $src) { Stop-WithMessage "Run this from inside the '$TransferName' folder." }
        Invoke-Unpack $src
    } else {
        Invoke-Pack
    }
} catch {
    Write-Host ''
    Write-Host 'Something went wrong:' -ForegroundColor Red
    Write-Host "   $($_.Exception.Message)" -ForegroundColor Red
    Write-Host ''
    Write-Host 'Nothing on this PC was deleted. Take a screenshot of this window if you need help.'
    exit 1
}
