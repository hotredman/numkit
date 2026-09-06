# todo: IDE adapter for `input()` (and the `pause`/`keyboard` interactive-wait family)

*Kind:* feature / parity · *Status:* open · *Filed:* 2026-09-06 (portion-3 design discussion with user)

> Lifecycle: open → done. On completion, record the outcome in
> `dev-docs/memory/` and delete this file.

## Problem

Portion 3 ships `input()` at the engine level with an injectable input
provider (`Engine::setInputProvider`, the `setOutputFunc` pattern) wired
for the native CLI (stdin), node-WASM (piped stdin pre-read), and the
fieldtest harness (canned answers). The IDE gets none of that: a script
running in the IDE that hits `input()` must ask the USER through the UI,
which needs an IPC round trip the current session protocol doesn't have.

Interim behavior (until this task closes): `input()` in `--ide-session`
throws an explicit "not supported in this context" error — never a silent
hang.

## Design (agreed 2026-09-06)

Two tiers:

1. **Native CLI `--ide-session`** (pipe protocol): engine emits
   `__INPUT_REQUEST__:{"prompt":"<text>"}` on the session channel; the
   IDE console panel switches to "input mode" (echo the prompt, the next
   submitted line goes back to the process stdin instead of the REPL
   command path). Thin adapter — the engine side is already provider-
   based from portion 3.

2. **Web IDE (WASM)**: no stdin exists; a synchronous JS callback from
   the engine is the wrong shape. Use the VM's EXISTING pausability
   (debugger infrastructure, `dev-docs/handbook/callback_pausability.md`):
   `input()` pauses execution like a breakpoint and resumes when the IDE
   delivers the line through the bindings. No blocking `window.prompt`
   bridge except, optionally, as a stopgap.

Same seam, same family: `pause` (wait for keypress) and `keyboard`
(suspend into the caller's workspace) are the identical
"engine waits for external event" contract — implement their IDE/CLI
behavior on the same provider rather than as one-offs. This also unlocks
the corpus scripts currently filtered by the harness `pause(` token.

## Acceptance

- IDE console: running a script with `input()` shows the prompt inline,
  accepts a line, execution continues with the value; 's'-form and
  empty-line semantics identical to the engine tests.
- Web IDE: same via the pausable-VM path (debugger resume flow).
- `pause` / `keyboard` behave on the same adapter.
- Dual-engine guards stay green; IDE-side smoke recorded.

## References

- Engine seam: portion 3 (bugs/PRs of 2026-09-06; `Engine::setInputProvider`)
- Existing IPC gap in the same area: `bugs/opened/ide/object-inspection-json-ipc.md`
  (consider doing both as one "IDE IPC round 2" portion)
- Pausability handbook: `dev-docs/handbook/callback_pausability.md`
