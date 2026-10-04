# Notes for Claude

Project: A Link Between Wilds, online co-op mod for BotW Switch 1.6.0. Read README.md, CONTRIBUTING.md and docs/decisions/ before making changes.

Rules for every change, as the owner (Lee) requires a full paper trail:
- Work on a branch and open a PR into `main`; never push directly to `main`. Conventional Commit messages.
- Update CHANGELOG.md, and docs/research/offsets-log.md / docs/decisions/ / docs/protocol.md whenever the change touches those areas.
- Don't edit vendored exlaunch files in client/ outside client/source/program/ unless deliberately updating exlaunch (record it).
- Run `cd server && python3 -m unittest` before pushing. The Switch module can't be compiled in the Claude sandbox; CI builds it, so check the CI run after pushing.
