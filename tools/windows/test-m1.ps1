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
    # Find where 1.0.0 keeps Link's position (find-link.bat runs this): no server,
    # waits up to 10 minutes for the module's "scan RESULT" line.
    .\tools\windows\test-m1.ps1 -Scan

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
    [switch]$Uninstall,

    # Player scan mode (decision 0010): don't start the server, show the module's
    # scan progress live and wait for its RESULT line instead of a server join.
    [switch]$Scan,

    # Answer "yes" to the setup fixes the script offers (turning off a BotW title
    # update, re-enabling the mod in Ryujinx) instead of asking.
    [switch]$Yes
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$TitleId  = '01007ef00011e000'
if ($Scan) {
    $NoServer = $true
    if (-not $PSBoundParameters.ContainsKey('WaitSeconds')) { $WaitSeconds = 600 }
}
$ModName  = 'albw'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ModDir   = Join-Path $RyujinxDir "mods\contents\$TitleId\$ModName\exefs"
$Stamp    = Get-Date -Format 'yyyy-MM-dd_HHmmss'
$Started  = Get-Date

function Write-Step([string]$msg) { Write-Host "==> $msg" -ForegroundColor Cyan }
function Write-Warn([string]$msg) { Write-Host "    $msg" -ForegroundColor Yellow }
function Write-Ok([string]$msg)   { Write-Host "    $msg" -ForegroundColor Green }

# Asks a yes/no question; Enter on its own means yes. -Yes answers for you.
function Confirm-Fix([string]$question) {
    if ($Yes) { return $true }
    $answer = Read-Host "    $question [Y/n]"
    return ($answer -eq '' -or $answer -match '^(?i)y')
}

# Ryujinx keeps its log open; read with sharing so we don't block or fail.
function Read-Shared([string]$path) {
    $fs = [System.IO.File]::Open($path, 'Open', 'Read', 'ReadWrite')
    try { (New-Object System.IO.StreamReader($fs)).ReadToEnd() } finally { $fs.Dispose() }
}

# Saves JSON the way Ryujinx reads it, keeping a one-time backup of the original.
function Save-JsonWithBackup([string]$path, $object) {
    if (-not (Test-Path "$path.albw-backup")) { Copy-Item $path "$path.albw-backup" -Force }
    [System.IO.File]::WriteAllText($path, ($object | ConvertTo-Json -Depth 10))
}

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
$ryuProcs   = @(Get-Process -Name 'Ryujinx*' -ErrorAction SilentlyContinue)
$ryuRunning = $ryuProcs.Count -gt 0

# --- 2b. pre-flight checks -------------------------------------------------------
# Each of these has stopped a first test somewhere before: catch them here, where
# they're cheap to fix, instead of after a game boot. Notes go into the report.
Write-Step 'Checking Ryujinx setup'
$preflight = New-Object System.Collections.Generic.List[string]
$gameDir = Join-Path $RyujinxDir "games\$TitleId"

# config.ini: the module works without it (built-in defaults), but put the
# documented starting file in place so there's one obvious file to edit later.
$sdConfig = Join-Path $RyujinxDir 'sdcard\albw\config.ini'
$repoConfig = Join-Path $RepoRoot 'client\sdcard\albw\config.ini'
if (Test-Path $sdConfig) {
    $preflight.Add("config.ini: present at ``$sdConfig``")
} elseif (Test-Path $repoConfig) {
    New-Item -ItemType Directory -Force -Path (Split-Path $sdConfig -Parent) | Out-Null
    Copy-Item $repoConfig $sdConfig
    Write-Ok "Copied the default config.ini to $sdConfig"
    $preflight.Add("config.ini: copied the default to ``$sdConfig``")
} else {
    $preflight.Add('config.ini: none (module uses built-in settings)')
}

# Title update: the mod only works on 1.0.0. Ryujinx remembers the chosen update
# in games\<title id>\updates.json ("selected" is empty for "No Update").
$updatesJson = Join-Path $gameDir 'updates.json'
if (Test-Path $updatesJson) {
    try {
        $updates = Get-Content $updatesJson -Raw | ConvertFrom-Json
        $selected = if ($updates.PSObject.Properties['selected']) { [string]$updates.selected } else { '' }
        if ($selected) {
            Write-Warn "Ryujinx is set to run BotW with an update: $(Split-Path $selected -Leaf)"
            Write-Warn 'The mod needs the base game (1.0.0, no update).'
            if ($ryuRunning) {
                Write-Warn 'Close Ryujinx and rerun to fix this automatically, or right-click BotW > Manage Title Updates > No Update.'
                $preflight.Add("Title update: **an update is selected** ($(Split-Path $selected -Leaf)); Ryujinx was running, not changed")
            } elseif (Confirm-Fix 'Switch BotW to "No Update" for this test? You can switch back in Manage Title Updates.') {
                $updates.selected = ''
                Save-JsonWithBackup $updatesJson $updates
                Write-Ok 'BotW will start without the update (backup: updates.json.albw-backup).'
                $preflight.Add("Title update: was $(Split-Path $selected -Leaf), switched to No Update by this script")
            } else {
                $preflight.Add("Title update: **an update is selected** ($(Split-Path $selected -Leaf)), left as is")
            }
        } else {
            $preflight.Add('Title update: none selected (good)')
        }
    } catch {
        $preflight.Add("Title update: couldn't read updates.json ($($_.Exception.Message))")
    }
} else {
    $preflight.Add('Title update: none set up in Ryujinx (good)')
}

# Mod switched off in Ryujinx's mod manager (games\<title id>\mods.json).
$modsJson = Join-Path $gameDir 'mods.json'
if (Test-Path $modsJson) {
    try {
        $mods = Get-Content $modsJson -Raw | ConvertFrom-Json
        $ours = @($mods.mods | Where-Object { $_.path -like "*\$ModName" -or $_.path -like "*\$ModName\*" -or $_.name -eq $ModName })
        $off  = @($ours | Where-Object { -not $_.enabled })
        if ($off.Count -gt 0) {
            Write-Warn 'The mod is switched off in Ryujinx (right-click BotW > Manage Mods).'
            if ($ryuRunning) {
                Write-Warn 'Close Ryujinx and rerun to fix this automatically, or tick it in Manage Mods.'
                $preflight.Add('Mod enabled in Ryujinx: **no**; Ryujinx was running, not changed')
            } elseif (Confirm-Fix 'Switch the mod back on?') {
                foreach ($m in $off) { $m.enabled = $true }
                Save-JsonWithBackup $modsJson $mods
                Write-Ok 'Mod switched on.'
                $preflight.Add('Mod enabled in Ryujinx: was off, switched on by this script')
            } else {
                $preflight.Add('Mod enabled in Ryujinx: **no**, left off')
            }
        } else {
            $preflight.Add('Mod enabled in Ryujinx: yes')
        }
    } catch {
        $preflight.Add("Mod enabled in Ryujinx: couldn't read mods.json ($($_.Exception.Message))")
    }
}

# Other code mods for BotW. Only one subsdk9 can load; others may also patch main.
$modsRoot = Split-Path (Split-Path $ModDir -Parent) -Parent
$others = @(Get-ChildItem $modsRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -ne $ModName -and (Test-Path (Join-Path $_.FullName 'exefs')) })
if ($others.Count -gt 0) {
    $names = ($others | ForEach-Object { $_.Name }) -join ', '
    $clash = @($others | Where-Object { Test-Path (Join-Path $_.FullName 'exefs\subsdk9') })
    if ($clash.Count -gt 0) {
        Write-Warn "Another mod also installs subsdk9 and will clash: $(($clash | ForEach-Object { $_.Name }) -join ', '). Turn it off in Manage Mods for this test."
    } else {
        Write-Warn "Other code (exefs) mods are installed for BotW: $names. If anything goes wrong, turn them off and retry."
    }
    $preflight.Add("Other exefs mods: $names")
} else {
    $preflight.Add('Other exefs mods: none')
}
foreach ($line in $preflight) { Write-Host "    $line" }

if ($InstallOnly) { Write-Step 'Installed. Done.'; return }

# --- 3. enable guest logs ------------------------------------------------------
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

$serverNote = if ($NoServer) { 'not started by script' } else { "port $Port" }
if (-not $NoServer) {
    # A server left open from an earlier run already owns the port: use it.
    $portBusy = $false
    try { $portBusy = @(Get-NetUDPEndpoint -LocalPort $Port -ErrorAction Stop).Count -gt 0 } catch { }
    if ($portBusy) {
        Write-Step "Port $Port is already in use, probably by the ALBW server from an earlier run. Using that one."
        Write-Warn "If the test fails, close the old 'ALBW server' window and rerun."
        $NoServer = $true
        $serverNote = "port $Port already in use; assumed an earlier ALBW server window"
    }
}
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
    Start-Sleep -Seconds 3
    $startText = if (Test-Path $serverLog) { Read-Shared $serverLog } else { '' }
    if ($startText -match 'listening on') {
        Write-Ok 'Server is up. Windows may ask whether Python can use the network: click Allow.'
    } else {
        Write-Warn "The server hasn't said it's listening yet. Check the 'ALBW server' window for an error."
        if ($startText) { Write-Warn (($startText.Trim() -split "`r?`n" | Select-Object -Last 3) -join ' | ') }
    }
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
if ($Scan) {
    Write-Host ''
    Write-Host '    1. Load your save and wait until you can move Link.' -ForegroundColor White
    Write-Host '    2. Run (or ride, or glide) in a straight line for about 30 seconds.' -ForegroundColor White
    Write-Host '    3. Keep going until this window says FOUND. Progress shows below.' -ForegroundColor White
    Write-Host ''
}

# --- 6. watch the logs ---------------------------------------------------------
function Get-RyujinxLog {
    # Ryujinx writes Logs\ either in its data folder or next to Ryujinx.exe,
    # depending on version and on portable mode, so look in every candidate.
    $dirs = @((Join-Path $RyujinxDir 'Logs'))
    $exes = @()
    if ($RyujinxExe) { $exes += $RyujinxExe }
    foreach ($proc in @(Get-Process -Name 'Ryujinx*' -ErrorAction SilentlyContinue)) {
        try { if ($proc.Path) { $exes += $proc.Path } } catch { }
    }
    foreach ($exe in $exes) {
        $exeDir = Split-Path $exe -Parent
        $dirs += (Join-Path $exeDir 'Logs')
        $dirs += (Join-Path $exeDir 'portable\Logs')
    }
    $dirs = @($dirs | Select-Object -Unique)
    $logs = foreach ($d in $dirs) {
        if (Test-Path $d) { Get-ChildItem $d -Filter '*.log' -File | Where-Object { $_.LastWriteTime -ge $Started.AddSeconds(-5) } }
    }
    return $logs | Sort-Object LastWriteTime -Descending | Select-Object -First 1
}

Write-Step "Watching logs for up to $WaitSeconds s (Ctrl+C to stop early)"
$deadline = (Get-Date).AddSeconds($WaitSeconds)
$ryuLog = $null; $albwLines = @(); $problemLines = @(); $serverText = ''; $appLine = ''
$scanShown = 0
$joinedAt = $null
while ((Get-Date) -lt $deadline) {
    $ryuLog = Get-RyujinxLog
    if ($ryuLog) {
        $lines = (Read-Shared $ryuLog.FullName) -split "`r?`n"
        $albwLines    = @($lines | Where-Object { $_ -match '\[albw' })
        $problemLines = @($lines | Where-Object { $_ -match '(?i)unresolved|subsdk9|albw.*(abort|fail)|Unhandled exception|InvalidAccess' } | Select-Object -First 40)
        # Ryujinx names the game and its version when it starts it, e.g. "Application Loaded: ... v1.0.0 [01007EF00011E000]".
        $appLine = @($lines | Where-Object { $_ -match '(?i)Application Loaded' } | Select-Object -Last 1) -join ''
    }
    if (Test-Path $serverLog) { $serverText = Read-Shared $serverLog }
    if ($Scan) {
        $scanNow = @($albwLines | Where-Object { $_ -match '\] scan' })
        for ($i = $scanShown; $i -lt $scanNow.Count; $i++) {
            Write-Host ('    ' + ($scanNow[$i] -replace '^.*\[albw\] ', ''))
        }
        $scanShown = $scanNow.Count
        if ($albwLines -match 'scan (RESULT|: skipped)') { break }
        Start-Sleep -Seconds 3
        continue
    }
    # Show the module's progress as it happens, so a stall is visible.
    for ($i = $scanShown; $i -lt $albwLines.Count; $i++) {
        Write-Host ('    ' + ($albwLines[$i] -replace '^.*\[albw\] ', ''))
    }
    $scanShown = $albwLines.Count
    if ($albwLines -match 'networking (off|stopped)') { break }
    if ($problemLines -match '(?i)Unhandled exception|InvalidAccess') { Start-Sleep -Seconds 3; break }
    # After joining, keep watching until the module's 30-second heartbeat shows it
    # is still running (or ~45 s pass), so a crash just after joining is caught too.
    if (($albwLines -match 'joined as player') -or ($serverText -match 'joined from')) {
        if (-not $joinedAt) {
            $joinedAt = Get-Date
            Write-Ok 'Joined! Watching for another 30-45 seconds to make sure the game stays up...'
        }
        if ($albwLines -match '\] status: player') { break }
        if (((Get-Date) - $joinedAt).TotalSeconds -gt 45) { break }
    }
    Start-Sleep -Seconds 3
}
if (Test-Path $serverLog) { $serverText = Read-Shared $serverLog }

# --- game version ----------------------------------------------------------------
# The mod only supports BotW 1.0.0 with no update (docs/decisions/0009). Ryujinx's own
# line says what it ran; the module's line says what build ID it found in memory.
$ryuVersion = if ($appLine -match '\bv(\d+\.\d+\.\d+)') { $Matches[1] } else { 'unknown' }
$modVersionLine = @($albwLines | Where-Object { $_ -match 'game (version|build id)' } | Select-Object -First 1) -join ''
$modVersionOk = $modVersionLine -match 'version 1\.0\.0 .*supported' -and $modVersionLine -notmatch 'NOT supported'
if ($ryuVersion -ne 'unknown' -and $ryuVersion -ne '1.0.0') {
    Write-Warn "Ryujinx ran BotW v$ryuVersion. This mod needs v1.0.0: right-click BotW > Manage Title Updates > No Update."
}

# --- verdict -------------------------------------------------------------------
$checks = [ordered]@{
    'Module loaded'           = [bool]($albwLines -match 'A Link Between Wilds loaded')
    'Socket opened'           = [bool]($albwLines -match 'socket ready')
    'HELLO sent'              = [bool]($albwLines -match 'sent HELLO')
    'Joined (module log)'     = [bool]($albwLines -match 'joined as player')
    'Joined (server log)'     = [bool]($serverText -match 'joined from')
    'Game is 1.0.0 (module)'  = [bool]$modVersionOk
    'Still running (heartbeat)' = [bool]($albwLines -match '\] status: player')
}
$scanResult = @($albwLines | Where-Object { $_ -match 'scan RESULT' } | Select-Object -Last 1) -join ''
if ($Scan) {
    $checks.Remove('HELLO sent'); $checks.Remove('Joined (module log)'); $checks.Remove('Joined (server log)')
    $checks.Remove('Still running (heartbeat)')
    $checks['Scan started'] = [bool]($albwLines -match 'scan: will search')
    $checks['Scan found Link'] = [bool]$scanResult
}
$crashLine = @($problemLines | Where-Object { $_ -match '(?i)Unhandled exception|InvalidAccess' } | Select-Object -First 1) -join ''
$lastAlbw  = if ($albwLines.Count) { ($albwLines[-1] -replace '^.*\[albw\] ', '') } else { '' }
$netOff    = @($albwLines | Where-Object { $_ -match 'networking off' } | Select-Object -First 1) -join ''
$joined = $false
if (-not $Scan) { $joined = $checks['Joined (module log)'] -or $checks['Joined (server log)'] }
if ($Scan -and $scanResult) {
    $joined = $true   # green verdict colour
    $verdict = 'FOUND: ' + ($scanResult -replace '^.*scan RESULT: ', '') + '. Send this report to Claude.'
} elseif ($Scan -and ($albwLines -match 'scan: skipped')) {
    $verdict = "NOT RUN: the module didn't recognise this game version (Ryujinx ran v$ryuVersion), so it didn't scan. It needs BotW 1.0.0 with no update."
} elseif ($Scan -and ($albwLines -match 'scan round \d+: \d+ chain')) {
    $verdict = "NOT YET: it found possible spots but Link didn't move far enough in time. Rerun and run in a straight line for longer (no fast travel)."
} elseif ($Scan -and ($albwLines -match 'scan round')) {
    $verdict = 'NOT FOUND: the scan ran but saw nothing that looks like Link. Were you in the world (past the title screen)? If yes, send this report to Claude: 1.0.0 is laid out differently than expected.'
} elseif ($Scan -and $checks['Module loaded']) {
    $verdict = 'NO SCAN: the module loaded but never started scanning. Send this report to Claude.'
} elseif ($Scan) {
    $verdict = 'FAIL: no [albw] lines. Either the module did not load, or guest logs are off.'
} elseif ($crashLine) {
    $when = if ($joined) { ' after joining the server' } elseif ($checks['Module loaded']) { ' after the module loaded' } else { '' }
    $verdict = "CRASH: the game crashed$when. Last [albw] line: '$lastAlbw'. Send this report to Claude."
    $joined = $false   # yellow verdict colour
} elseif ($netOff) {
    $verdict = 'NETWORK OFF: the module loaded and the game kept running, but it could not find the Switch socket functions it needs. Send this report to Claude: it lists the names the game does have.'
} elseif ($joined) {
    $verdict = 'PASS: the module loaded and joined the server.'
    if (-not $modVersionOk) {
        $verdict += " But the module did not confirm BotW 1.0.0 (Ryujinx ran v$ryuVersion), so it won't read the game; remove the update in Ryujinx and rerun."
    }
} elseif ($checks['HELLO sent']) {
    $verdict = 'PARTIAL: module is sending HELLO but never joined. Check the server window is open, server_ip/server_port in config.ini (sdcard\albw\config.ini in the Ryujinx folder), and, if the server is on another PC, its firewall.'
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
# $(if ($Scan) { 'Player scan' } else { 'M1 in-game test' }): $($Started.ToString('yyyy-MM-dd HH:mm'))

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
- Server: $serverNote
- Game version (Ryujinx): $ryuVersion
- Game version (module): $(if ($modVersionLine) { "``$($modVersionLine.Trim())``" } else { 'no line logged' })

## Pre-flight
$(($preflight | ForEach-Object { "- $_" }) -join "`n")

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

$reportPath = Join-Path $reportDir "$Stamp-$(if ($Scan) { 'scan' } else { 'm1' }).md"
[System.IO.File]::WriteAllText($reportPath, $report)

Write-Host ''
Write-Host $verdict -ForegroundColor $(if ($joined) { 'Green' } else { 'Yellow' })
Write-Host "Report: $reportPath"
Write-Host 'Commit the report on a branch so the result is recorded (see CONTRIBUTING.md).'
