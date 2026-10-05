# 0008. Setup must be doable by someone who has never modded anything

- **Date:** 2026-10-05
- **Status:** Accepted

## Context
The people this mod is for are Lee's friends. They'll play it on their own Windows PCs, and they "have no clue how to set up anything complex." Today, setting up means installing Ryujinx, finding hidden folders, copying files into a mods path, editing `config.ini` and running a Python server from a terminal. Most of them would give up before getting in-game.

## Decision
Ease of setup is a **requirement**, weighed in every change, not polish for the end:

- **Players do one thing:** run a single installer (*ALBW Setup*). It finds or sets up Ryujinx and checks that the game (1.6.0), keys and firmware are present, explaining anything missing in plain English with what to do about it. It installs or updates the mod and asks only for a **player name** and a **join code** from the host. Rerunning it updates everything.
- **The host does one thing:** a single launcher starts the server and shows the join code to send to friends. Connecting over the internet must not require router knowledge from players.
- **Nothing is typed into a terminal or edited by hand** in the normal path. Config files, folder paths and command-line options exist for developers and troubleshooting only.
- **Errors are written for the player**, saying what happened and the next step ("Couldn't find Breath of the Wild. Click *Choose game file*…"). Codes and log paths come after that, for when they send a screenshot.
- **Out of scope:** the installer never bundles, downloads or shares the game, keys or firmware, and the repo never contains them ([CONTRIBUTING](../../CONTRIBUTING.md#never-commit)). It asks where they are and checks them.
- **Test it on a clean PC** (or a fresh Windows user account) before calling a release done, following only the player-facing instructions.

## Consequences
- New features need a "how does a player turn this on?" answer. If it's "edit a file", that's a gap to close before release.
- Developer tools (`test-m1.ps1`, `config.ini`, compile-time switches) stay, but they sit behind the player tooling, not in front of it.
- How friends reach the host's server (a private network like Tailscale, port forwarding, or a relay on the home server) is an open question, to be settled in its own decision record when we get there. The choice is driven by "least setup for players."
- Windows only to start, since that's what the players have.
