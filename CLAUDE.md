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
- Test runner: `build-<preset>/tests/gtest/Release/numkit_gtest.exe`.
- WASM: `build.sh --wasm` with emsdk env sourced; redeploy IDE via `deploy.sh`.

## Commits

- Conversational Russian, code/commits/tests English.
- Co-authored trailer required (see prior commits for style).
- `main` is the integration branch. Never force-push.

## Public API conventions

Every public function in `libs/<ns>/include/numkit/<ns>/**` follows
[docs/LIBRARY_API.md](docs/LIBRARY_API.md) — the authoritative API
ruleset (argument order, native scalar types vs `const Value &` vs
`Span<const double>`, `FnHandle` callbacks, no `Engine *` in public
signatures, magic-polymorphism → typed overloads, multi-output return
shape). Read it before adding or refactoring any public `libs/`
function.

## Smoke tests

- Hand-runnable `.m` smokes live in `libs/<name>/tests/smoke/*_smoke.m`
  (one per public function or related cluster). Run via
  `build/desktop-fast/tests/smoke/Release/numkit_smoke.exe <path>`.
- **Every smoke MUST start with `clear` on the very first line**, then
  the usual `import compat.*` and the body. This ensures no leftover
  workspace state from a prior run leaks into the test.

## Each /loop cycle ships FOUR artefacts (mandatory)

Every function added in a /loop cycle produces all four of these. Three
artefacts is incomplete and must be flagged before the cycle is closed.

### 1. C++ implementation
In `libs/<lib>/src/...` and `libs/<lib>/include/...`. Probe MATLAB
(`help fnname` + `doc fnname`) before writing code; implement every
documented branch or document the gap explicitly in PROGRESS.md.

### 2. Parity spec — `tools/parity/specs/<name>.json`
Cross-engine validation against MATLAB R2025b (and Octave when it ships
the function). Minimum shape:
```json
{
  "name": "<fn>",
  "namespace": "<lib>",
  "setup": "...inputs...",
  "expr": "y = <fn>(...);",
  "iters": 5,
  "tol": 1e-12,
  "fingerprint": ["y(1)", "y(end)", "..."],
  "comment": "Sig + every covered branch + any deferred branch."
}
```
Run `python tools/parity/run_parity.py tools/parity/specs/<name>.json`
(no `--no-matlab` flag — we always validate against MATLAB). Must
report `correctness=OK` before commit. If MATLAB / Octave doesn't ship
the function the run reports `correctness=N/A` — document that in the
spec comment.

### 3. gtest unit test — `libs/<lib>/tests/<name>_test.cpp`
Offline regression guard with hardcoded expected values. Pattern (used
across libs/stats, libs/builtin, libs/signal):
```cpp
#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class HaartTest : public ::testing::Test {
public:
    numkit::Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(HaartTest, Level1Vector) {
    eval("[a, d] = haart([1 2 3 4 5 6 7 8], 1);");
    EXPECT_NEAR(evalScalar("a(1)"), 2.121320343559643, 1e-12);
    // ...one TEST_F per documented branch.
}
```
Wire via `libs/<lib>/tests/CMakeLists.txt` (create if missing — pattern
`target_sources(numkit_gtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/...)`).
Tests must pass under `numkit_gtest.exe`. At least one TEST_F per
documented branch.

### 4. Smoke `.m` — `libs/<lib>/tests/smoke/<name>_smoke.m`
Hand-runnable demo. **Every smoke MUST start with `clear` on the first
line**, then `import compat.*`, then the body. Use `fprintf` to print
expected values inline ("expect ~..."). Run via
`build/desktop-fast/tests/smoke/Release/numkit_smoke.exe libs/<lib>/tests/smoke/<name>_smoke.m`.

Three real bugs in cycles 65-75 were caught only by parity cross-check —
hand-written smokes had passed all three. Don't trust your own
expected values: trust the reference engine.

## Backfill «по дороге» (in-flight)

The full 4-artefact rule started 2026-05-04. ~17 functions shipped
before that ([list in audit/BACKFILL_QUEUE.md](audit/BACKFILL_QUEUE.md))
have only the C++ + parity spec — no gtest, no smoke. Each /loop cycle
now closes one item from that queue alongside the new function:

  cycle = (1 NEW function with 4 artefacts) + (1 BACKFILL: gtest+smoke for one queue entry)

The queue file lists priority order (simpler functions first). When you
backfill an entry, move it from "open" to "closed" in
`audit/BACKFILL_QUEUE.md` and reference both the new function and the
backfill in the commit message:
```
wavelet: implement <new> + backfill gtest+smoke for <old>
```
When the queue empties, the rule continues for new functions only.

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

## Bug catalog (`bugs/`)

Structured one-file-per-bug catalog (distinct from the flat append-only
`BUGS.md` and from the auditor's `audit/findings/**`). **Every bug you find
gets TWO things:**

1. its own `bugs/<namespace>/<fn>.md` with a self-contained repro (numkit
   output vs MATLAB R2025b) so any session can act on it cold; and
2. a matching **`DISABLED_` gtest** in `libs/<ns>/tests/known_bugs_test.cpp`
   asserting the MATLAB-correct behaviour — **found a bug → add a test.**
   `DISABLED_` keeps the green baseline green (it doesn't run normally) but
   the test is real: it fails under `--gtest_also_run_disabled_tests` and
   becomes a live regression guard the instant you remove the prefix.

See [bugs/README.md](bugs/README.md) for the template + index. When you fix
a bug, remove the test's `DISABLED_` prefix, flip the md status to ✅ FIXED
with the commit hash, and update the index row (keep the md as a record).

## Memory

Auto-memory at
`C:/Users/User/.claude/projects/c--Users-User-Projects-numkit-m/memory/`.
Always check `MEMORY.md` index for context before non-trivial work.
