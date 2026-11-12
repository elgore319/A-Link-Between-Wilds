# Test reports

One file per in-game test run, written by `tools/windows/test-m1.ps1` (named `YYYY-MM-DD_HHMMSS-<milestone>.md`). Each records the commit and build hash tested, the verdict, and the relevant Ryujinx and server log lines, so a regression can be traced to the exact build that introduced it.

Commit reports on a branch with a PR, like any other change. Add what you saw in-game under **Notes** before committing. Failed runs are worth keeping too.

## Verdicts

| Verdict | Meaning | What to do |
|---|---|---|
| PASS | The module loaded, joined the server and was still running afterwards. | Commit the report. |
| CRASH | Ryujinx logged a crash. The report shows the last `[albw]` line before it, which says how far the module got. | Send the report to Claude. |
| NETWORK OFF | The module couldn't find a socket function it needs (decision 0011). The game kept running. The report lists the names the game has. | Send the report to Claude. |
| PARTIAL | The module started but didn't join: either it never got a socket, or the server never answered. | Check the server window is open and `config.ini` points at it. |
| FAIL / NO DATA | No `[albw]` lines at all: the module didn't load, guest logs are off, or the game wasn't started. | Check the Pre-flight section of the report. |
| FOUND / NOT YET / NOT FOUND | Player scan results (`find-link.bat`, decision 0010). | Follow the verdict text. |
