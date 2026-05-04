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

## Memory

Auto-memory at
`C:/Users/User/.claude/projects/c--Users-User-Projects-numkit-m/memory/`.
Always check `MEMORY.md` index for context before non-trivial work.
