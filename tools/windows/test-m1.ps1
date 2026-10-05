<#
.SYNOPSIS
    One-command in-game test for A Link Between Wilds on Windows + Ryujinx.

.DESCRIPTION
    1. Finds the module build (subsdk9 + main.npdm), from a CI artifact zip or a folder.
    2. Installs it into Ryujinx's mods folder for Breath of the Wild.
    3. Turns on Ryujinx "guest logs" so the module's [albw] messages are logged.
    4. Starts the relay server in its own window, logging to a file.
    5. Optionally launches Ryujinx straight into the game.
    6. Watches the logs, then writes a test report (Markdown) with a verdict.

    The report goes to docs/test-reports/ so results become part of the repo's
    paper trail. Commit it on a branch like any other change.

.EXAMPLE
    # Simplest: newest albw-exefs*.zip from Downloads, you start the game yourself.
    .\tools\windows\test-m1.ps1

.EXAMPLE
    # Fully automatic: also launch Ryujinx into the game.
    .\tools\windows\test-m1.ps1 -RyujinxExe "C:\Emulators\Ryujinx\Ryujinx.exe" -GamePath "D:\Switch\BotW.nsp"

.NOTES
    Works with Windows PowerShell 5.1 and PowerShell 7. If scripts are blocked, run once:
        Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
#>
[CmdletBinding()]
param(
    # Zip downloaded from a CI run, or a folder containing subsdk9 and main.npdm.
    # Default: newest albw-exefs*.zip in your Downloads folder, else client\deploy.
    [string]$Artifact,

    # Ryujinx data folder (has Config.json, mods\, Logs\). Default: %APPDATA%\Ryujinx.
    [string]$RyujinxDir = (Join-Path $env:APPDATA 'Ryujinx'),

    # Optional: Ryujinx.exe and the game file, to launch the game automatically.
    [string]$RyujinxExe,
    [string]$GamePath,

    # How long to watch the logs for the module to join, in seconds.
    [int]$WaitSeconds = 120,

    # Must match ServerPort compiled into the module (albw_config.hpp); only change both together.
    [int]$Port = 55420,

    # Skip starting the server (e.g. it's already running, or runs on another machine).
    [switch]$NoServer,

    # Only install the module, then stop.
    [switch]$InstallOnly,

    # Remove the installed module and stop.
    [switch]$Uninstall
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$TitleId  = '01007ef00011e000'
$ModName  = 'albw'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ModDir   = Join-Path $RyujinxDir "mods\contents\$TitleId\$ModName\exefs"
$Stamp    = Get-Date -Format 'yyyy-MM-dd_HHmmss'
$Started  = Get-Date

function Write-Step([string]$msg) { Write-Host "==> $msg" -ForegroundColor Cyan }
function Write-Warn([string]$msg) { Write-Host "    $msg" -ForegroundColor Yellow }

# --- uninstall -----------------------------------------------------------------
if ($Uninstall) {
    $modRoot = Split-Path $ModDir -Parent
    if (Test-Path $modRoot) {
        Remove-Item $modRoot -Recurse -Force
        Write-Step "Removed $modRoot"
    } else {
        Write-Step "Nothing installed at $modRoot"
    }
    return
}

# --- 1. find the build ---------------------------------------------------------
function Find-Artifact {
    if ($Artifact) { return (Resolve-Path $Artifact).Path }

    $downloads = Join-Path $env:USERPROFILE 'Downloads'
    if (Test-Path $downloads) {
        $zip = Get-ChildItem $downloads -Filter 'albw-exefs*.zip' -ErrorAction SilentlyContinue |
               Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($zip) { return $zip.FullName }
    }
    $local = Join-Path $RepoRoot 'client\deploy'
    if (Test-Path (Join-Path $local 'subsdk9')) { return $local }

    throw "No build found. Download 'albw-exefs' from a CI run (Actions tab -> latest run -> Artifacts) " +
          "into Downloads, build locally with 'make' in client\, or pass -Artifact <zip or folder>."
}

Write-Step 'Locating module build'
$source = Find-Artifact
$buildDir = $source
if ($source -like '*.zip') {
    $buildDir = Join-Path $env:TEMP "albw-artifact-$Stamp"
    Expand-Archive -Path $source -DestinationPath $buildDir -Force
}
$subsdk = Get-ChildItem $buildDir -Recurse -File -Filter 'subsdk9' | Select-Object -First 1
$npdm   = Get-ChildItem $buildDir -Recurse -File -Filter 'main.npdm' | Select-Object -First 1
if (-not $subsdk -or -not $npdm) { throw "$source doesn't contain both subsdk9 and main.npdm." }
$subsdkHash = (Get-FileHash $subsdk.FullName -Algorithm SHA256).Hash.ToLower()
Write-Host "    from $source"
Write-Host "    subsdk9 sha256 $subsdkHash"

# --- 2. install ----------------------------------------------------------------
Write-Step "Installing to $ModDir"
if (-not (Test-Path $RyujinxDir)) {
    throw "Ryujinx folder not found at $RyujinxDir. Pass -RyujinxDir (for a portable install it's the 'portable' folder next to Ryujinx.exe)."
}
New-Item -ItemType Directory -Force -Path $ModDir | Out-Null
Copy-Item $subsdk.FullName (Join-Path $ModDir 'subsdk9') -Force
Copy-Item $npdm.FullName   (Join-Path $ModDir 'main.npdm') -Force
if ($InstallOnly) { Write-Step 'Installed. Done.'; return }

# --- 3. enable guest logs ------------------------------------------------------
$ryuRunning = @(Get-Process -Name 'Ryujinx*' -ErrorAction SilentlyContinue).Count -gt 0
$configPath = Join-Path $RyujinxDir 'Config.json'
$guestLogNote = ''
if (Test-Path $configPath) {
    $configText = Get-Content $configPath -Raw
    if ($configText -match '"logging_enable_guest"\s*:\s*true') {
        $guestLogNote = 'already on'
    } elseif ($ryuRunning) {
        # Ryujinx rewrites Config.json on exit, so editing it now would be lost.
        $guestLogNote = 'OFF (Ryujinx was running; close it and rerun, or enable Options > Settings > Logging > Guest logs)'
        Write-Warn "Ryujinx is running, so guest logs can't be switched on automatically. [albw] lines may be missing."
    } elseif ($configText -match '"logging_enable_guest"\s*:\s*false') {
        Copy-Item $configPath "$configPath.albw-backup" -Force
        $configText = $configText -replace '"logging_enable_guest"\s*:\s*false', '"logging_enable_guest": true'
        [System.IO.File]::WriteAllText($configPath, $configText)
        $guestLogNote = 'turned on by this script (backup: Config.json.albw-backup)'
    } else {
        $guestLogNote = 'unknown (setting not found in Config.json; enable it in Options > Settings > Logging)'
    }
} else {
    $guestLogNote = 'unknown (no Config.json found)'
}
Write-Step "Guest logs: $guestLogNote"

# --- 4. start the server -------------------------------------------------------
$reportDir = Join-Path $RepoRoot 'docs\test-reports'
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
$serverLog = Join-Path $env:TEMP "albw-server-$Stamp.log"

if (-not $NoServer) {
    $python = $null
    foreach ($cand in @('py', 'python', 'python3')) {
        if (Get-Command $cand -ErrorAction SilentlyContinue) { $python = $cand; break }
    }
    if (-not $python) { throw 'Python not found. Install Python 3.10+ from python.org (tick "Add to PATH"), or use -NoServer.' }

    Write-Step "Starting server on port $Port (window titled 'ALBW server')"
    $serverScript = Join-Path $RepoRoot 'server\albw_server.py'
    $cmd = "`$host.UI.RawUI.WindowTitle = 'ALBW server'; & $python -u '$serverScript' --port $Port 2>&1 | Tee-Object -FilePath '$serverLog'"
    Start-Process powershell -ArgumentList '-NoExit', '-NoProfile', '-Command', $cmd | Out-Null
    Start-Sleep -Seconds 2
}

# --- 5. launch the game --------------------------------------------------------
if ($RyujinxExe -and $GamePath) {
    if ($ryuRunning) {
        Write-Warn 'Ryujinx is already running; not launching a second copy. Start BotW in it yourself.'
    } else {
        Write-Step 'Launching Ryujinx'
        Start-Process -FilePath $RyujinxExe -ArgumentList "`"$GamePath`"" | Out-Null
    }
} else {
    Write-Step 'Start Breath of the Wild in Ryujinx now.'
}

# --- 6. watch the logs ---------------------------------------------------------
function Get-RyujinxLog {
    $dirs = @((Join-Path $RyujinxDir 'Logs'))
    if ($RyujinxExe) { $dirs += (Join-Path (Split-Path $RyujinxExe -Parent) 'Logs') }
    $logs = foreach ($d in $dirs) {
        if (Test-Path $d) { Get-ChildItem $d -Filter '*.log' -File | Where-Object { $_.LastWriteTime -ge $Started.AddSeconds(-5) } }
    }
    return $logs | Sort-Object LastWriteTime -Descending | Select-Object -First 1
}

function Read-Shared([string]$path) {
    # Ryujinx keeps its log open; read with sharing so we don't block or fail.
    $fs = [System.IO.File]::Open($path, 'Open', 'Read', 'ReadWrite')
    try { (New-Object System.IO.StreamReader($fs)).ReadToEnd() } finally { $fs.Dispose() }
}

Write-Step "Watching logs for up to $WaitSeconds s (Ctrl+C to stop early)"
$deadline = (Get-Date).AddSeconds($WaitSeconds)
$ryuLog = $null; $albwLines = @(); $problemLines = @(); $serverText = ''
while ((Get-Date) -lt $deadline) {
    $ryuLog = Get-RyujinxLog
    if ($ryuLog) {
        $lines = (Read-Shared $ryuLog.FullName) -split "`r?`n"
        $albwLines    = @($lines | Where-Object { $_ -match '\[albw' })
        $problemLines = @($lines | Where-Object { $_ -match '(?i)unresolved|subsdk9|albw.*(abort|fail)|Unhandled exception|InvalidAccess' } | Select-Object -First 40)
    }
    if (Test-Path $serverLog) { $serverText = Read-Shared $serverLog }
    if (($albwLines -match 'joined as player') -or ($serverText -match 'joined from')) { Start-Sleep -Seconds 5; break }
    Start-Sleep -Seconds 3
}
if (Test-Path $serverLog) { $serverText = Read-Shared $serverLog }

# --- verdict -------------------------------------------------------------------
$checks = [ordered]@{
    'Module loaded'           = [bool]($albwLines -match 'A Link Between Wilds loaded')
    'Socket opened'           = [bool]($albwLines -match 'socket ready')
    'HELLO sent'              = [bool]($albwLines -match 'sent HELLO')
    'Joined (module log)'     = [bool]($albwLines -match 'joined as player')
    'Joined (server log)'     = [bool]($serverText -match 'joined from')
}
$joined = $checks['Joined (module log)'] -or $checks['Joined (server log)']
if ($joined) {
    $verdict = 'PASS: the module loaded and joined the server.'
} elseif ($checks['HELLO sent']) {
    $verdict = 'PARTIAL: module is sending HELLO but never joined. Check the server is running, the IP/port in albw_config.hpp, and the firewall.'
} elseif ($checks['Module loaded']) {
    $verdict = 'PARTIAL: module loaded but networking failed. See the [albw] lines (socket errors) and any unresolved-symbol lines below.'
} elseif ($problemLines.Count -gt 0) {
    $verdict = 'FAIL: module did not report in, and the log has related errors (below).'
} elseif (-not $ryuLog) {
    $verdict = 'NO DATA: no Ryujinx log written since the script started. Was the game launched?'
} else {
    $verdict = 'FAIL: no [albw] lines. Either the module did not load, or guest logs are off.'
}

$commit = ''
try { $commit = (& git -C $RepoRoot rev-parse --short HEAD 2>$null) } catch { }
$branch = ''
try { $branch = (& git -C $RepoRoot rev-parse --abbrev-ref HEAD 2>$null) } catch { }

$fence = '```'
$report = @"
# M1 in-game test: $($Started.ToString('yyyy-MM-dd HH:mm'))

**Verdict:** $verdict

| Check | Result |
|---|---|
$(($checks.GetEnumerator() | ForEach-Object { "| $($_.Key) | $(if ($_.Value) { 'yes' } else { 'no' }) |" }) -join "`n")

## Setup
- Repo: ``$branch`` @ ``$commit``
- Build: ``$(Split-Path $source -Leaf)``, subsdk9 sha256 ``$subsdkHash``
- Installed to: ``$ModDir``
- Guest logs: $guestLogNote
- Ryujinx log: ``$(if ($ryuLog) { $ryuLog.Name } else { 'none found' })``
- Server: $(if ($NoServer) { 'not started by script' } else { "port $Port" })

## [albw] lines from Ryujinx
$fence
$(if ($albwLines.Count) { ($albwLines | Select-Object -First 80) -join "`n" } else { '(none)' })
$fence

## Possibly related Ryujinx lines
$fence
$(if ($problemLines.Count) { $problemLines -join "`n" } else { '(none)' })
$fence

## Server log
$fence
$(if ($serverText) { $serverText.Trim() } else { '(none)' })
$fence

## Notes
<!-- Add anything you observed in-game: crashes, stutter, what you did. -->
"@

$reportPath = Join-Path $reportDir "$Stamp-m1.md"
[System.IO.File]::WriteAllText($reportPath, $report)

Write-Host ''
Write-Host $verdict -ForegroundColor $(if ($joined) { 'Green' } else { 'Yellow' })
Write-Host "Report: $reportPath"
Write-Host 'Commit the report on a branch so the result is recorded (see CONTRIBUTING.md).'
