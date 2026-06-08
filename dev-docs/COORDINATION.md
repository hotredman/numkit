# Parallel Worker Coordination

> **Status (2026-06): ACTIVE — parallel multi-worktree model.** Work runs in
> three worktrees, one chat per directory/territory:
> `numkit-m-core` (`core-dev`, owns `core/`), `numkit-m-ide` (`ide-dev`, owns
> `ide/` + `wasm/`), `numkit-m-lib` (`lib-dev`, owns `toolboxes/`). `numkit-m/` is
> the **base** — it holds the shared `.git` and stays on `main` (the
> integration branch + the user's view); it is not a work dir.
>
> **HARD RULE: a chat merges/pushes its branch into `main` ONLY on the user's
> explicit command — never auto-fast-forward.** Each chat commits to its own
> branch, pushes that branch for visibility, and waits.

This repository can be worked on by **multiple parallel Claude sessions**
("workers"), each owning a defined territory. Read this before touching
anything. Crossing territory without protocol is a coordination failure.

The model is **append-only territories on dedicated git worktrees + branches,
merged back to `main` when chunks are shippable**. The user controls the
merge cadence; workers only push their own feature branches.

---

## Worker types

| Worker | Territory (exclusive write) | Branch | Worktree path |
|---|---|---|---|
| **CORE** | `core/` (engine, parser, lexer, compiler, VM, TreeWalker, AST, value, environment, debugger, vfs, scratch) · `core/tests/` · top-level `tests/` · `core/CMakeLists.txt` · `NAMESPACE_DESIGN.md` | `core-dev` | `numkit-m-core/` |
| **LIBS** | all of `toolboxes/` (pure-compute MATLAB toolboxes: signal / stats / image / comm / control / wavelet / graphics / builtin / audio / graph) **and** `core-libs/` (engine-coupled support libs: io / optim / ode): `include/`, `src/`, `tests/`, `benchmarks/`, per-lib `CMakeLists.txt`. Plus `tools/parity/` (PROGRESS.md / BENCHMARK.md) · `bugs/` | `lib-dev` | `numkit-m-lib/` |
| **IDE** | `ide/` (React/Vite + Electron desktop) · `wasm/` (Emscripten bindings) · `brand/` · scripts in `scripts/`: dev / desktop / deploy / build-desktop / build (the `--wasm` path) | `ide-dev` | `numkit-m-ide/` |

`numkit-m/` (the base, on `main`) is the integration target, not a worker.

Currently a single LIBS worker (`lib-dev` in `numkit-m-lib/`) owns all of
`toolboxes/`. If `toolboxes/` work ever needs to fan out across concurrent chats, split
by lib name — each chat owning disjoint libs (never the same lib as another),
on its own branch + worktree. Example split:

- LIBS-signal — `toolboxes/signal/`
- LIBS-image — `toolboxes/image/`
- LIBS-stats — `toolboxes/{builtin,stats}/`
- LIBS-comm — `toolboxes/{comm,wavelet,control}/`

---

## Shared surface (any worker may touch — with strict protocol)

These files are touchable by any worker, but a change here can break
others. The rule:

> **If you modify a shared file, run the FULL test suite before committing.
> If a test outside your territory fails, you broke it — fix it or revert.**

| Shared file | Who typically touches | Protocol |
|---|---|---|
| `CMakeLists.txt` (top-level) | rarely after the per-area split — project setup, options (Highway / Threads / Emscripten), final target assembly | Full build + tests on all three areas before commit |
| `toolboxes/CMakeLists.txt` | LIBS workers when adding a NEW lib (one-line append in the lib-list loop) | Trivial conflict resolution; both adds keep both libs |
| `CMakePresets.json` | mostly CORE | Full build + tests |
| `README.md` | all | Append-style sections |
| `MEMORY.md` (index) | all | Append-only list of links; do not reorder |
| `bugs/` | LIBS + CORE workers file bugs | One `.md` per bug under `bugs/<ns>/`; no number collisions |
| `PROGRESS.md` | LIBS workers via `tools/parity/run_parity.py` (rewrites rows in place) | Row-level updates rarely overlap; if they do, accept whichever ran last |
| `.gitignore` / `.gitattributes` / `.claude/settings.json` | any | Append-only |

**Rule of thumb:** if you're not sure whether a file is shared, assume yes
and run the full test suite.

---

## Worktree workflow

**One-time setup** (run from the main worktree):

```bash
cd C:/Users/User/Projects/numkit-m
git worktree add ../numkit-m-ide          feature/ide
git worktree add ../numkit-m-libs-signal  feature/libs-signal
git worktree add ../numkit-m-libs-image   feature/libs-image
# ... add more as needed
```

Each worktree is a separate working directory pointing to the same `.git/`.
Branches are shared, file states are independent, builds are independent.

**Tearing down a worktree** when its branch is merged:

```bash
git worktree remove ../numkit-m-libs-signal
git branch -d feature/libs-signal   # local
git push origin --delete feature/libs-signal   # remote (if pushed)
```

---

## Worker session lifecycle

### Starting a session

1. Read this file. Confirm your territory.
2. `cd` to your worktree. Verify `git rev-parse --show-toplevel` matches.
3. `git status` — note any unstaged changes. **If they're not yours, stop**
   and surface them to the user. Don't silently work on top.
4. `git fetch && git pull --rebase origin main` — sync your branch with
   the latest `main`. (For CORE on `main` directly: just `git pull`.)

### During work

- Stay strictly within your territory.
- If you must touch shared surface: announce it in your reply to the user
  ("touching `CMakeLists.txt` — will run full suite before commit").
- Use the parity harness for LIBS work: `python tools/parity/run_parity.py
  tools/parity/specs/<fn>.json` — never use `--no-matlab` if MATLAB is on PATH.

### Before committing

- **Full test suite** must pass:
  ```bash
  ./build/desktop-fast/tests/gtest/Release/numkit_gtest.exe --gtest_brief=1
  ```
  All 6300+ tests except the 1 documented skip + 4 disabled.
- For an API-breaking change in `toolboxes/<area>/include/`: also rebuild any
  downstream lib that depends on you. `toolboxes/image` / `toolboxes/control` /
  `toolboxes/comm` depend on `toolboxes/signal`; `toolboxes/builtin` is the base.
- For a CORE API change (`Value`, `Engine`, `CallContext`): the LIBS build
  is a strict downstream — must be verified.
- For a libc++-incompatible change (e.g. `std::cyl_bessel_*`,
  `std::format` other than the basics): WASM build (`scripts/build.bat --wasm`)
  must succeed. Emscripten ships libc++ which lacks several optional
  C++17/20 features. See the retired BUGS.md #36 (git history) for what NOT to do.

### Pushing

- Each worker pushes only its own feature branch:
  ```bash
  git push origin feature/<your-branch>
  ```
- CORE may push directly to `main` for small atomic changes (one-function
  fixes, kernel bug fixes). For large refactors, use `feature/core`.
- **Never** force-push `main`. Force-push your own feature branch only.

### Reporting completion

Reply to user with:
- Branch name + last commit hash
- Test count summary ("6383 / 6387 passing")
- What functions / features landed
- Any known followups or downstream work needed

User decides when to merge.

---

## Merge protocol (user-driven)

The **user** initiates merges, not the worker. Two options:

**Local merge:**
```bash
cd C:/Users/User/Projects/numkit-m   # main worktree
git fetch
git merge --no-ff origin/feature/libs-signal
# resolve conflicts if any, then:
./build/desktop-fast/tests/gtest/Release/numkit_gtest.exe --gtest_brief=1
git push origin main
```

**GitHub PR:**
```bash
gh pr create --base main --head feature/libs-signal --title "..."
# review + merge via GitHub UI
```

After `main` advances, other live feature branches should rebase:

```bash
cd ../numkit-m-libs-image
git fetch
git rebase origin/main
# resolve any conflicts
git push --force-with-lease origin feature/libs-image   # since rebase rewrites history
```

`--force-with-lease` is safer than `--force` — it refuses to overwrite if
someone else pushed to your remote branch in the meantime.

---

## Cross-lib dependency rules

The dependency DAG (verified 2026-05-04):

```
                  builtin
                     │
        ┌────────────┼────────────┬─────────────┬────────┐
        │            │            │             │        │
      signal       stats       wavelet       graphics    io
        │
   ┌────┼────┐
   │    │    │
 image control comm
```

**Rules:**

- `Builtin` is self-contained — depends on nothing in `toolboxes/`. **Invariant.**
- All toolbox libs depend on `Builtin`.
- `Image`, `Control`, `Communications` depend on `Signal` (DSP primitives:
  conv, FFT, DCT). Document as: if you change `toolboxes/signal` public C++ API
  (`include/numkit/signal/...`), rebuild + test these three.

**Practical consequence for multi-LIBS-workers:**

If LIBS-signal-worker and LIBS-image-worker run concurrently, and signal
worker changes a public API, image worker's branch will break on rebase.
Coordination: signal worker merges first, image worker rebases on the new
main, fixes any breakage, then merges.

---

## Build isolation

CMake's preset `binaryDir = ${sourceDir}/build-*`, so each worktree builds
into its own tree:

```
numkit-m/build/desktop-fast/                ← main worktree's build
numkit-m-libs-signal/build/desktop-fast/    ← LIBS-signal's build
numkit-m-ide/build/browser/                 ← IDE's WASM build
```

**Never** run `cmake` / `cmake --build` from one worktree's source dir
while pointing at another's build dir. Stay in your own worktree.

If you need to test against another worker's in-progress state without
committing: in your own worktree, `git fetch && git checkout
origin/feature/<other-branch> -- <path>` to pull specific files, build,
test, then `git restore <path>` to undo. Don't work in the other tree.

---

## Memory writes

Each session writes its own memory files under
`C:/Users/User/.claude/projects/c--Users-User-Projects-numkit-m/memory/`.

Naming is **semantic, not prefixed**: `project_signal_perf.md`,
`feedback_no_premature_renames.md`, etc. Each worker writes about its own
domain — file collisions are unlikely because territories don't overlap.

The `MEMORY.md` index is shared and append-only. When adding a new memory
entry, append a line at the end of the relevant section; do not reorder.

---

## Quick reference

**You are CORE?** Touch `core/` and `core/tests/`. Full test suite + WASM
smoke after public-API changes.

**You are LIBS-`<area>`?** Touch `toolboxes/<area>/` (whatever areas you own
this session). Use the parity harness. Never touch `core/` or `ide/`.

**You are IDE?** Touch `ide/`, `wasm/`, `docs/`, `brand/`, deploy/dev
scripts. Never touch `core/` or `toolboxes/`.

**You see uncommitted changes outside your territory?** Surface them to
the user. Do not work on top of them.

**Tests fail outside your territory after your change?** You broke them.
Fix or revert.

**Public API change?** Rebuild + test downstream. Document in commit.

**Adding a new lib (LIBS only)?** Create `toolboxes/<name>/{include,src,tests,
benchmarks}/`, give it its own `toolboxes/<name>/CMakeLists.txt`, append the
name to the loop in `toolboxes/CMakeLists.txt`. Run full build to verify.
