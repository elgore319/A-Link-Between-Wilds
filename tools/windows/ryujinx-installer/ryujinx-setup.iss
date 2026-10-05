; Ryujinx installer for A Link Between Wilds.
;
; Installs an unmodified copy of Ryujinx for the current user (no admin prompt),
; then optionally sets up the Ryujinx data folder (%APPDATA%\Ryujinx) from:
;   - a transfer zip made by export-ryujinx-data.ps1 on another PC, or
;   - a prod.keys file the user picks.
; It never contains keys, firmware or game files itself.
;
; Build with build.ps1 (Windows) or see docs/installing-ryujinx.md. Required defines:
;   /DRyujinxDir=<folder containing Ryujinx.exe>   /DRyujinxVersion=1.1.1380
; Written for Inno Setup 6.2+ (tested with 6.2.2).

#ifndef RyujinxDir
  #error Pass /DRyujinxDir=<folder containing Ryujinx.exe>
#endif
#ifndef RyujinxVersion
  #error Pass /DRyujinxVersion=<version, e.g. 1.1.1380>
#endif

[Setup]
; Never change AppId: it's how Windows matches upgrades and the uninstaller.
AppId={{591FD319-4BC7-45FD-842B-E5A7950E4B9D}
AppName=Ryujinx
AppVersion={#RyujinxVersion}
AppVerName=Ryujinx {#RyujinxVersion}
AppPublisher=Ryujinx team (packaged by A Link Between Wilds)
AppComments=Unmodified Ryujinx build, packaged for A Link Between Wilds. MIT licensed.
DefaultDirName={localappdata}\Programs\Ryujinx
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=lowest
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
MinVersion=10.0
LicenseFile={#RyujinxDir}\LICENSE.txt
OutputBaseFilename=ryujinx-{#RyujinxVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\Ryujinx.exe
UninstallDisplayName=Ryujinx {#RyujinxVersion}
CloseApplications=yes

[Files]
; Logs\ and portable\ are per-machine data, not part of the program.
Source: "{#RyujinxDir}\*"; Excludes: "\Logs,\portable"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Icons]
Name: "{autoprograms}\Ryujinx"; Filename: "{app}\Ryujinx.exe"
Name: "{autodesktop}\Ryujinx"; Filename: "{app}\Ryujinx.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Ryujinx.exe"; Description: "{cm:LaunchProgram,Ryujinx}"; Flags: nowait postinstall skipifsilent

[Messages]
; The uninstaller only removes the program. Say so, because people worry about saves.
ConfirmUninstall=Remove Ryujinx from this PC?%n%nYour saves, keys, firmware and settings in %APPDATA%\Ryujinx are NOT deleted.

[Code]
var
  ModePage: TInputOptionWizardPage;
  ZipPage: TInputFileWizardPage;
  KeysPage: TInputFileWizardPage;

const
  ModeTransfer = 0;
  ModeKeys = 1;
  ModeSkip = 2;

function DataDir: String;
begin
  Result := ExpandConstant('{userappdata}\Ryujinx');
end;

procedure InitializeWizard;
begin
  ModePage := CreateInputOptionPage(wpSelectTasks,
    'Your Switch files',
    'Ryujinx needs your own keys and firmware. How do you want to set them up?',
    'Nothing here is uploaded anywhere; files are only copied on this PC.',
    True, False);
  ModePage.Add('Moving from another PC: restore everything from a transfer zip (keys, firmware, saves, settings, mods)');
  ModePage.Add('New setup: copy in my prod.keys file (install firmware afterwards from Ryujinx: Tools > Install Firmware)');
  ModePage.Add('Skip: I''ll set it up myself, or it''s already set up on this PC');
  { Command line: /TRANSFERZIP="path" or /KEYS="path" preselect (and, with /SILENT, do) the step. }
  if ExpandConstant('{param:TransferZip|}') <> '' then
    ModePage.SelectedValueIndex := ModeTransfer
  else if ExpandConstant('{param:Keys|}') <> '' then
    ModePage.SelectedValueIndex := ModeKeys
  else if DirExists(DataDir) or WizardSilent then
    ModePage.SelectedValueIndex := ModeSkip
  else
    ModePage.SelectedValueIndex := ModeKeys;

  ZipPage := CreateInputFilePage(ModePage.ID,
    'Transfer zip',
    'Pick the ryujinx-transfer-....zip you made on your other PC.',
    'If Ryujinx already has data on this PC, it is renamed to a Ryujinx.backup-... folder first, not deleted.');
  ZipPage.Add('Transfer zip:', 'Zip files|*.zip', '.zip');
  ZipPage.Values[0] := ExpandConstant('{param:TransferZip|}');

  KeysPage := CreateInputFilePage(ZipPage.ID,
    'Keys',
    'Pick your prod.keys file (dumped from your own Switch).',
    'It is copied to %APPDATA%\Ryujinx\system\prod.keys. An existing prod.keys there is kept as prod.keys.bak.');
  KeysPage.Add('prod.keys:', 'Key files|*.keys|All files|*.*', '.keys');
  KeysPage.Values[0] := ExpandConstant('{param:Keys|}');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if PageID = ZipPage.ID then
    Result := ModePage.SelectedValueIndex <> ModeTransfer
  else if PageID = KeysPage.ID then
    Result := ModePage.SelectedValueIndex <> ModeKeys;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = ZipPage.ID) and not FileExists(ZipPage.Values[0]) then begin
    MsgBox('Pick the transfer zip first (or go Back and choose another option).', mbError, MB_OK);
    Result := False;
  end else if (CurPageID = KeysPage.ID) and not FileExists(KeysPage.Values[0]) then begin
    MsgBox('Pick your prod.keys file first (or go Back and choose Skip).', mbError, MB_OK);
    Result := False;
  end;
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
  MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result := MemoDirInfo;
  if MemoTasksInfo <> '' then
    Result := Result + NewLine + NewLine + MemoTasksInfo;
  Result := Result + NewLine + NewLine + 'Switch files:' + NewLine + Space;
  case ModePage.SelectedValueIndex of
    ModeTransfer: Result := Result + 'Restore from ' + ZipPage.Values[0];
    ModeKeys:     Result := Result + 'Copy keys from ' + KeysPage.Values[0];
  else
    Result := Result + 'Leave as is';
  end;
end;

{ Single quotes are the only special character inside a PowerShell '...' string. }
function PsQuote(S: String): String;
begin
  StringChangeEx(S, '''', '''''', True);
  Result := '''' + S + '''';
end;

procedure RestoreTransferZip;
var
  Backup, Cmd: String;
  Code: Integer;
begin
  if DirExists(DataDir) then begin
    Backup := DataDir + '.backup-' + GetDateTimeString('yyyymmdd-hhnnss', '-', '-');
    if not RenameFile(DataDir, Backup) then begin
      MsgBox('Couldn''t move the existing Ryujinx data folder out of the way, so nothing was restored.' + #13#10 +
        'Close Ryujinx and run the installer again.', mbError, MB_OK);
      Exit;
    end;
    Log('Existing data folder moved to ' + Backup);
  end;
  ForceDirectories(DataDir);

  WizardForm.StatusLabel.Caption := 'Restoring your Ryujinx data (this can take a minute)...';
  Cmd := '-NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = ''Stop''; ' +
    'Expand-Archive -LiteralPath ' + PsQuote(ZipPage.Values[0]) +
    ' -DestinationPath ' + PsQuote(DataDir) + ' -Force"';
  if not Exec('powershell.exe', Cmd, '', SW_HIDE, ewWaitUntilTerminated, Code) or (Code <> 0) then
    MsgBox('Restoring the transfer zip failed (code ' + IntToStr(Code) + ').' + #13#10 +
      'Ryujinx itself is installed. You can unzip the transfer zip into %APPDATA%\Ryujinx by hand.',
      mbError, MB_OK)
  else if not FileExists(DataDir + '\system\prod.keys') then
    MsgBox('The transfer zip was restored, but it has no system\prod.keys in it. ' +
      'Ryujinx will ask for keys when it starts.', mbInformation, MB_OK);
end;

procedure CopyKeys;
var
  Dest: String;
begin
  ForceDirectories(DataDir + '\system');
  Dest := DataDir + '\system\prod.keys';
  if FileExists(Dest) then
    FileCopy(Dest, Dest + '.bak', False);
  if not FileCopy(KeysPage.Values[0], Dest, False) then
    MsgBox('Couldn''t copy prod.keys to ' + Dest + '. Copy it there by hand.', mbError, MB_OK);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then begin
    { Page validation doesn't run in silent installs, so check the files again here. }
    case ModePage.SelectedValueIndex of
      ModeTransfer: if FileExists(ZipPage.Values[0]) then RestoreTransferZip
                    else Log('Transfer zip not found, skipped: ' + ZipPage.Values[0]);
      ModeKeys:     if FileExists(KeysPage.Values[0]) then CopyKeys
                    else Log('Keys file not found, skipped: ' + KeysPage.Values[0]);
    end;
  end;
end;
