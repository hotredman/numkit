# Parallel Session Coordination

This repository is worked on by **two parallel Claude sessions**. Read this
before touching anything. Each session has a defined territory; crossing it
is a coordination failure.

---

## Active sessions

### Session A — kernel + libs/builtin
- **Working dir:** `C:/Users/User/Projects/numkit-m/` (main worktree)
- **Branch:** `main`
- **Build dir:** `build-desktop-fast/` (in main worktree)
- **Owns (exclusive write):**
  - `core/` (engine, parser, lexer, compiler, VM, TW, AST, value, environment)
  - `libs/builtin/` (kernel-adjacent — clear, who, eval, run, import, etc.)
  - `core/tests/`, `libs/builtin/tests/`
  - `NAMESPACE_DESIGN.md`
  - Top-level `build.sh`, `dev.sh`, `test.sh`, `deploy.sh`
  - `docs/` (IDE deploy)
  - `ide/` (IDE source)
- **May read but not write:** any `libs/{signal,stats,graphics,io}/` source.

### Session B — toolbox libraries
- **Working dir:** separate git worktree (e.g. `../numkit-m-libs/`)
- **Branch:** feature branch (e.g. `feature/libs-extension`)
- **Build dir:** worktree's own `build-desktop-fast/` (NOT main's)
- **Owns (exclusive write):**
  - `libs/signal/` (DSP toolbox)
  - `libs/stats/` (statistics)
  - `libs/graphics/` (plotting)
  - `libs/io/` (file I/O)
  - Their respective `tests/` subdirs
  - `PARITY_PROGRESS.md` (live parity map; both sessions append rows
    via `tools/parity/run_parity.py`)
- **May read but not write:** anything in `core/` or `libs/builtin/`.

---

## Shared surface (both may touch — with strict protocol)

These files are touchable by either session, but a change here can break the
other. The rule:

> **If you modify a shared file, run the FULL test suite before committing.
> If a test outside your territory fails, you broke it — fix it or revert.**

Shared files:
- `CMakeLists.txt` (top-level) — adds toolbox subdirectories, shared options
- `libs/builtin/src/library.cpp` registration patterns referenced by
  Session B's libs (read-mostly for B; if B needs a new builtin in core,
  request via TODO note, don't add directly)
- `MEMORY.md` and `.claude/projects/*/memory/` — auto-memory; both write
  but to different sub-files. The index `MEMORY.md` is append-only style.
- `README.md`

If you're not sure whether a file is shared — assume yes, run full tests.

---

## Build isolation

**Each worktree builds in its own dir.** CMakePresets uses `${sourceDir}/build-*`,
so:
- Main worktree (`C:/Users/User/Projects/numkit-m/`) → builds into
  `C:/Users/User/Projects/numkit-m/build-desktop-fast/`.
- Side worktree (`C:/Users/User/Projects/numkit-m-libs/`) → builds into
  `C:/Users/User/Projects/numkit-m-libs/build-desktop-fast/`.

**Never** run cmake/build commands while standing in the OTHER worktree's
source dir. Stay in your own worktree. CMake's binaryDir is relative to source,
so if you `cd` into the other tree, you'll trash their build cache.

If you need to test against shared toolbox state: check out the other side's
branch into your own worktree, build, then revert. Do not work in their tree.

---

## Coordination protocol

### Starting a task
1. Read this file. Confirm your territory.
2. `git status` — note any unstaged changes. If they're not yours, **stop**
   and surface them to the user (don't silently work on top).
3. `git pull` if you've been idle.

### During work
- Stay strictly within your territory unless task requires shared surface.
- If you must touch shared surface: note it in your reply to the user
  ("touching CMakeLists.txt — will run full suite before commit").

### Before committing
- Run **full test suite** (`./build-*/tests/Release/numkit_tests.exe --gtest_brief=1`).
- All must pass except documented intentional skips.
- If a test in the OTHER session's territory fails:
  - It's your regression. Fix or revert.
  - Or surface to user with explicit "Session B test broken by this; need
    coordination" — don't ignore.

### Commits / branches
- Session A commits to `main` (with user's permission to push).
- Session B commits to its feature branch (`git push origin feature/...`).
- Merge happens via PR or explicit user-driven merge into `main`.
- **Never** leave uncommitted state in main working tree at end of session.
  `git stash` it or commit to a WIP branch before exiting.

### Memory writes
Each session writes its own memory files under
`C:/Users/User/.claude/projects/c--Users-User-Projects-numkit-m/memory/`.
The `MEMORY.md` index is shared — when adding a new entry, append a line
at the end of the relevant section, don't reorder.

---

## What went wrong (2026-05-01)

The first parallel attempt left Session B's library work
(`libs/signal/measurements/`, `libs/stats/moving/`, modifications to
`libs/{signal,stats}/library.cpp` and top-level `CMakeLists.txt`) in
**main working tree as uncommitted changes**. This:

1. Polluted Session A's `git status`, mixing two sessions' work.
2. Broke 3 kernel tests on TW (`FunctionTest.SimpleFunction`, etc.) —
   the library registration changes shadowed user-function names or
   touched something kernel-side. Session B did not run the full suite.
3. Forced Session A to stop mid-Phase to disentangle.

Root cause: Session B was launched against the main working dir instead of
a separate worktree, so its writes landed in main. Subsequent reload
of Session A surfaced the inconsistent state.

Fix: separate worktrees per session, separate branches, this file as the
shared contract.

---

## Quick reference

**You are Session A (kernel)?** Touch `core/`, `libs/builtin/`. Stop if you
see unstaged changes outside that.

**You are Session B (libs)?** Touch `libs/{signal,stats,graphics,io}/`.
Never touch `core/`. Build in your own worktree, not main.

**You see uncommitted changes outside your territory?** Surface them to
the user. Do not work on top of them.

**Tests fail outside your territory after your change?** You broke them.
Fix or revert.
