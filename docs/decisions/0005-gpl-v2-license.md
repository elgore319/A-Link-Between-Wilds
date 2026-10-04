# 0005. GPLv2 for the whole repo

- **Date:** 2026-10-04
- **Status:** Accepted

## Context
The client is built on exlaunch, which is GPLv2, so the client must be GPLv2. The server, protocol and tools could have used a permissive license like MIT.

## Decision
License everything GPLv2 for one simple, consistent license, and so improvements to the mod get shared back.

## Consequences
Anyone reusing the server code must also release their changes under GPLv2. If a permissive server becomes important later, the non-client parts could be relicensed (all contributors would need to agree).
