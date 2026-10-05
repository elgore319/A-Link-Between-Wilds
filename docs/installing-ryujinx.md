# Installing Ryujinx

Ryujinx's official downloads are gone since the project shut down, so this repo has an installer for the build we test against: **Ryujinx 1.1.1380** (win-x64, commit `3c61d56`). It's an unmodified official build, packaged with Inno Setup. Why it's done this way: [decision 0007](decisions/0007-ryujinx-installer.md).

The installer **never contains keys, firmware or games**. You bring your own, dumped from your own Switch.

## Moving your setup to another PC

1. On the **old PC**, close Ryujinx and double-click `tools\windows\ryujinx-installer\export-ryujinx-data.bat`. It saves `ryujinx-transfer-<date>.zip` to your Desktop with your keys, firmware, saves, settings and mods. Add `-SkipCaches` for a smaller zip (Ryujinx rebuilds shader caches).
2. Copy the zip to the new PC yourself (USB stick, your own network). **It contains your keys: don't upload or share it.**
3. On the **new PC**, run `ryujinx-1.1.1380-setup.exe`, choose **Moving from another PC**, and pick the zip.
4. Open Ryujinx. If your games are in a different folder on this PC, add it under *Options > Settings > General > Game Directories*.

If Ryujinx already had data on the new PC, it's renamed to `Ryujinx.backup-<date>` first, never deleted.

## Fresh setup (friends)

1. Run `ryujinx-1.1.1380-setup.exe`, choose **New setup**, and pick your `prod.keys`.
2. Open Ryujinx, then *Tools > Install Firmware* and pick your firmware dump.
3. Add your games folder under *Options > Settings > General > Game Directories*.

## Details

- Installs per user to `%LOCALAPPDATA%\Programs\Ryujinx`, no admin prompt. Data stays in the standard `%APPDATA%\Ryujinx`, which is where `test-m1.ps1` looks too.
- Uninstalling removes only the program, never saves, keys or settings.
- Silent install: `ryujinx-1.1.1380-setup.exe /SILENT /KEYS="C:\path\prod.keys"` or `/TRANSFERZIP="C:\path\ryujinx-transfer.zip"`.

## Building the installer

Ryujinx's binaries aren't committed. You need an unmodified Ryujinx folder (or a zip of it) and [Inno Setup 6](https://jrsoftware.org/isinfo.php).

```powershell
tools\windows\ryujinx-installer\build.bat "C:\path\to\folder-with-Ryujinx.exe"
```

The output is `tools\windows\ryujinx-installer\build\ryujinx-<version>-setup.exe` (gitignored). The version is read from `Ryujinx.exe`.

**In CI:** upload the Ryujinx zip somewhere (e.g. a release asset on this repo), then run the **Ryujinx installer** workflow from the Actions tab with its URL and SHA-256. It downloads the zip, checks the hash, and builds the installer as a downloadable artifact. Pull requests that touch the installer also get a compile check with a stub exe.

Known input for 1.1.1380:

| File | SHA-256 |
|---|---|
| `Ryujinx.exe` (1.1.1380+3c61d56) | `f5a0aa6b856f6cae12799daf0258174c2a13f9ff0e4bd8d41788ddedac57f30a` |
| `ryujinx-1.1.1380-win_x64.zip` (the 15 files of `publish\`, no `Logs\`) | `54293afe00e2e8b3afb7c162ba8d4b7e2573ec2922350cde77af63e47a01f622` |

**Without Windows:** ISCC 6.2.2 runs under Wine (tested with Kron4ek's wine-10.15 build on Linux). Inno Setup's installer can be unpacked with `innoextract` 1.9 to get `ISCC.exe` without running it.
