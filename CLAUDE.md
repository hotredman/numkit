# numkit-m repo notes for Claude

## STOP — read first

This repo is worked on by **two parallel Claude sessions**. Before doing
anything, read [COORDINATION.md](COORDINATION.md). It defines:

- which territory each session owns (kernel vs toolbox libs)
- shared-surface rules (when full-suite tests are mandatory)
- build isolation (separate worktrees, separate build dirs)
- commit/branch protocol

If `git status` shows unstaged changes outside your territory, **stop and
surface to the user**. Do not silently work on top of someone else's work.

## Project quick facts

- C++ MATLAB-compatible runtime: parser → AST → (TreeWalker | Bytecode VM).
- Two backends, dual-engine tests via `DualEngineTest` fixture.
- Build presets: `desktop-fast` (default, x86_64 + Highway SIMD),
  `portable`, `apple-m`, `browser` (WASM via emsdk).
- Build dir = `build-<preset>/` per source root; runs of cmake never share
  binaryDir between worktrees.
- Test runner: `build-<preset>/tests/Release/numkit_tests.exe`.
- WASM: `build.sh --wasm` with emsdk env sourced; redeploy IDE via `deploy.sh`.

## Commits

- Conversational Russian, code/commits/tests English.
- Co-authored trailer required (see prior commits for style).
- `main` is the integration branch. Never force-push.

## Smoke tests

- Hand-runnable `.m` smokes live in `libs/<name>/tests/smoke/*_smoke.m`
  (one per public function or related cluster). Run via
  `build-desktop-fast/Release/numkit_example.exe <path>`.
- **Every smoke MUST start with `clear` on the very first line**, then
  the usual `import compat.*` and the body. This ensures no leftover
  workspace state from a prior run leaks into the test.

## Cross-check against MATLAB / Octave (mandatory)

A smoke alone proves the implementation matches **my** expectations,
not MATLAB's. For every new function you add:

1. Write a JSON spec in `tools/parity/specs/<name>.json`. Minimal:
   ```json
   {
     "name": "<fn>",
     "namespace": "<lib>",
     "setup": "...inputs...",
     "expr": "y = <fn>(...);",
     "iters": 5,
     "tol": 1e-12,
     "out_var": "y",
     "comment": "Sig + brief input description."
   }
   ```
2. Run `python tools/parity/run_parity.py tools/parity/specs/<name>.json
   --no-matlab` (or omit `--no-matlab` if MATLAB is licenced + on PATH).
3. **Must report `correctness=OK`**. If it reports `MISMATCH`, the
   numkit implementation is wrong (or has a different convention than
   MATLAB) — fix BEFORE landing the cycle. Smoke alone is not enough.
4. If MATLAB / Octave doesn't have the function, the run reports
   `correctness=N/A`. Document that in the spec's comment so future
   runs don't think the harness is broken.

Three real bugs in cycles 65-75 were caught only by this cross-check —
hand-written smokes had passed all three. Don't trust your own
expected values: trust the reference engine.

## Audit findings (`audit/`)

Parallel parity-auditor worker (running in `../numkit-m-audit/` worktree
on branch `audit/findings`) writes ТЗ — technical-debt tickets — under
`audit/findings/<namespace>/<fn>.md`. Each ТЗ describes a parity gap
found in an already-shipped function, with reference outputs from MATLAB
already captured.

**Main worker rules for `audit/`:**

- `audit/AUDITOR_GUIDE.md` is the auditor's manual — do not edit (read
  if you need to understand what it produces).
- `audit/findings/**` — pick up a ТЗ when you want to close a parity
  gap. Implement the fix in `libs/`, verify against the ТЗ's reference
  outputs, then **move the file** to `audit/closed/<ns>/<fn>.md` and
  add a closing block at the bottom of the file:
  ```
  ## Closed
  - Closed in commit: <hash>
  - Closed date: YYYY-MM-DD
  - Notes: [one-line summary of what changed]
  ```
- `audit/INDEX.md` — update the row when you close a ТЗ (move from
  "Open ТЗ" to "Closed ТЗ").
- Never write **new** files in `audit/findings/**` from the main
  worker. That's the auditor's job.

In commit messages reference the ТЗ path explicitly:
```
stats: close audit ТЗ stats/normlike — add freq + censoring

Implements audit/findings/stats/normlike.md.
```

## Memory

Auto-memory at
`C:/Users/User/.claude/projects/c--Users-User-Projects-numkit-m/memory/`.
Always check `MEMORY.md` index for context before non-trivial work.
