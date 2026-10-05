# 0007. Ship Ryujinx as an installer built from an unmodified copy; never ship keys

- **Date:** 2026-10-05
- **Status:** Accepted

## Context
Ryujinx shut down and its official downloads were removed. Lee's working copy (1.1.1380) was only on his laptop, and he needs it on his desktop. Friends who'll play the mod can't do a complicated setup (one installer, minimal steps). Ryujinx also needs Nintendo keys and firmware, which this repo must never contain (see README *Legal* and CONTRIBUTING *Never commit*).

## Decision
- An Inno Setup installer (`tools/windows/ryujinx-installer/`) packages an **unmodified** official Ryujinx 1.1.1380 build. Ryujinx is MIT-licensed; its `LICENSE.txt` and `THIRDPARTY.md` are installed with it and the license is shown in the wizard.
- Per-user install, no admin prompt. Data stays in the standard `%APPDATA%\Ryujinx` so it matches existing setups and `test-m1.ps1`.
- Keys/firmware/saves come only from files the user picks: a transfer zip made by `export-ryujinx-data.ps1` on their own other PC, or their own `prod.keys`. Existing data is renamed to a backup, never overwritten or deleted.
- **Ryujinx binaries are not committed to git** (about 80 MB that would stay in history forever). Build inputs are pinned by SHA-256 in `docs/installing-ryujinx.md`; the built installer is a release asset or CI artifact, not a tracked file. `.gitignore` blocks build output and transfer zips.
- CI (`ryujinx-installer.yml`) builds on demand from a URL + SHA-256 and compile-checks the script on PRs with a stub.

## Consequences
- Moving between PCs and onboarding friends are each one installer run plus picking a file.
- The Ryujinx zip has to live somewhere outside git (a release asset is the plan); if it's lost, any copy whose `Ryujinx.exe` matches the pinned hash works.
- Friends still have to supply their own keys and firmware; the installer can only make that easy, not do it for them.
- 1.1.1380 is frozen. If we move to a fork (e.g. Ryubing), write a new record and re-pin hashes.
- Later, the installer can also install the ALBW module and `config.ini` once the mod is playable.
