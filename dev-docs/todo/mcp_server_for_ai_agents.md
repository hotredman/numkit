# todo: numkit-mcp — MCP server for AI agents

*Kind:* feature / ecosystem · *Status:* open · *Surfaced:* 2026-08-30 (reconciled from two conflicting drafts: the original todo + the PUBLISH.md follow-up note)

> Lifecycle: open -> done. On completion, record the outcome in
> `dev-docs/memory/` (per the AGENTS.md project-memory protocol) and
> delete this file.

## Goal

An MCP server that makes numkit a first-class TOOL for AI agents (Claude
Desktop, Cursor, Claude Code, ZCode, Continue, …) instead of something
they shell out to. NOT a 0.1.0 blocker: publish the CLI first; this is
the 0.2.0 fast-follow (two announcement beats; the mcp package pins the
published `numkit` from npm instead of rebuilding the WASM).

## Design (locked, reconciled)

1. **Package**: `packages/numkit-mcp/`, published as `numkit-mcp` (no @scope
   org — registration is an extra step for zero value at this size).
   Depends on `numkit` (the published CLI package) — no second WASM copy.
2. **Zero dependencies**: hand-rolled stdio JSON-RPC (initialize /
   tools/list / tools/call) — the needed protocol subset is ~200 lines,
   and the zero-dep identity applies to this package too.
3. **In-process engine, NOT spawn**: reuse the engine loader — extract
   cli.js's `loadEngine()` into a shared `bin/engine.js` used by both the
   CLI and the MCP server. ONE engine instance per server = the killer
   feature: a PERSISTENT MATLAB workspace across conversation turns
   (agents define functions and iterate). Spawn-per-call would cost
   ~250 ms of engine init and destroy statefulness — rejected.
4. **Tools — three, no sprawl** (agents know MATLAB syntax; specialized
   tools cost prompt tokens and routing accuracy while adding zero
   capability):

   | tool | params | returns |
   |---|---|---|
   | `numkit_eval` | `code` (string) | stdout, stderr (diagnostic line), duration_ms, error flag |
   | `numkit_reset` | — | confirmation |
   | `numkit_help` | `fn` (string) | the existing help catalog entry |

5. **Watchdog**: per-call timeout (default 3000 ms, configurable via env
   `NUMKIT_MCP_TIMEOUT`) — agents WILL write infinite loops; the engine's
   StackGuard covers recursion, the watchdog covers time. On timeout:
   kill the eval, return a structured timeout error, KEEP the server
   alive (the workspace may be dirty — the agent decides to reset).
6. **Honesty**: same wording as llms.txt — not a filesystem sandbox; the
   tool description says scripts read/write the working directory.
7. **Registration**: MCP directories (glama.ai/mcp/servers, smithery.ai,
   pulseMCP) — where agents discover tools today.

## Explicitly deferred

- SSE/HTTP transport (remote hosting) — every local agent speaks stdio.
- Specialized tools (matrix_solve, filter_design) — `numkit_eval` covers
  them; revisit only if routing stats show agents failing to find syntax.
- `@modelcontextprotocol/sdk` — reconsider only if the protocol subset
  grows beyond the hand-rolled ~200 lines.

## Acceptance criteria

- [ ] stdio handshake + tools/list + tools/call pass against a real
      agent client (ZCode or Claude Desktop config).
- [ ] `numkit_eval` roundtrip < 5 ms in-process (excl. first engine load).
- [ ] Persistence proven: define a function in call 1, use it in call 2.
- [ ] Watchdog: an infinite loop returns a timeout error, server survives.
- [ ] `numkit-mcp` published; registered in >= 1 MCP directory.

## References

- Engine loader to extract: `packages/numkit/bin/cli.js` (`loadEngine`).
- Protocol subset reference: modelcontextprotocol.io (stdio transport).
- Distribution note (replaces the PUBLISH.md mini-section):
  `packages/numkit/PUBLISH.md` "MCP server (follow-up)".
