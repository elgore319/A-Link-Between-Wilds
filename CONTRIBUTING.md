# Contributing

## Workflow
1. **Open or pick an issue** describing the change.
2. **Branch from `main`**: `feature/<short-name>`, `fix/<short-name>`, `docs/<short-name>` or `research/<short-name>`.
3. **Commit in small, focused steps** using [Conventional Commits](https://www.conventionalcommits.org/): `feat(client): ...`, `fix(server): ...`, `docs: ...`, `test: ...`, `chore: ...`, `ci: ...`. Explain *why* in the body when it isn't obvious.
4. **Open a pull request** using the template. CI must pass. Reference the issue (`Closes #12`).
5. **Squash or rebase merge**, never merge commits on `main`. Delete the branch after.

`main` should always build and pass tests.

## Paper trail (required)
This project is reverse engineering-heavy, so undocumented knowledge gets lost. With every change:
- **Changelog:** add a line under `[Unreleased]` in `CHANGELOG.md`.
- **Offsets/structures found or ruled out:** log in `docs/research/offsets-log.md`, with how they were found.
- **Design decisions** (new dependency, protocol change, architecture change): add a record in `docs/decisions/`.
- **Protocol changes:** update `protocol/albw_protocol.h`, `server/protocol.py` and `docs/protocol.md` together and bump `ALBW_PROTOCOL_VERSION`.
- **Bugs:** file an issue with the bug template even if you fix it right away, so the symptoms are searchable later.

## Layout
| Path | What |
|---|---|
| `client/` | Switch module. Our code is in `client/source/program/`; the rest is vendored exlaunch (see `client/EXLAUNCH.md`) and shouldn't be edited except to update it. |
| `server/` | Python relay server and tests |
| `protocol/` | Packet definitions shared with the client |
| `tools/` | Dev utilities |
| `docs/` | Everything written down |

## Running tests
```bash
cd server && python3 -m unittest -v
```

## Never commit
Game files, dumps, keys, or anything copyrighted by Nintendo. `.gitignore` blocks the common ones; double-check `git status` anyway.
