# numkit-m repo notes for Claude

## STOP — read first

This repo is worked on by **three parallel Claude sessions** (core / ide / lib).
Before doing anything, read [COORDINATION.md](dev-docs/COORDINATION.md). It
defines:

- which territory each session owns (core engine / libs toolbox / IDE)
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
- WASM: `scripts/build.sh --wasm` with emsdk env sourced; redeploy IDE via `scripts/deploy.sh`.

## Commits

- Conversational Russian, code/commits/tests English.
- Co-authored trailer required (see prior commits for style).
- `main` is the integration branch. Never force-push.

## Public API conventions

Every public function in `src/toolboxes/<ns>/include/numkit/<ns>/**` follows
[dev-docs/LIBRARY_API.md](dev-docs/LIBRARY_API.md) — the authoritative API
ruleset (argument order, native scalar types vs `const Value &` vs
`Span<const double>`, `FnHandle` callbacks, no `Engine *` in public
signatures, magic-polymorphism → typed overloads, multi-output return
shape). Read it before adding or refactoring any public `src/toolboxes/`
function.

## Smoke tests

- Hand-runnable `.m` smokes live in `src/toolboxes/<name>/tests/smoke/*_smoke.m`
  (one per public function or related cluster). Run via
  `build/desktop-fast/tests/smoke/Release/numkit_smoke.exe <path>`.
- **Every smoke MUST start with `clear` on the very first line**, then
  the usual `import compat.*` and the body. This ensures no leftover
  workspace state from a prior run leaks into the test.

## Each /loop cycle ships FOUR artefacts (mandatory)

Every function added in a /loop cycle produces all four of these. Three
artefacts is incomplete and must be flagged before the cycle is closed.

### 1. C++ implementation
In `src/toolboxes/<lib>/src/...` and `src/toolboxes/<lib>/include/...`. Probe MATLAB
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

### 3. gtest unit test — `src/toolboxes/<lib>/tests/<name>_test.cpp`
Offline regression guard with hardcoded expected values. Pattern (used
across src/toolboxes/stats, tests/builtin, src/toolboxes/signal):
```cpp
#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class HaartTest : public ::testing::Test {
public:
    numkit::StandardEngine engine;
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
Wire via `src/toolboxes/<lib>/tests/CMakeLists.txt` (create if missing — pattern
`target_sources(numkit_gtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/...)`).
Tests must pass under `numkit_gtest.exe`. At least one TEST_F per
documented branch.

### 4. Smoke `.m` — `src/toolboxes/<lib>/tests/smoke/<name>_smoke.m`
Hand-runnable demo. **Every smoke MUST start with `clear` on the first
line**, then `import compat.*`, then the body. Use `fprintf` to print
expected values inline ("expect ~..."). Run via
`build/desktop-fast/tests/smoke/Release/numkit_smoke.exe src/toolboxes/<lib>/tests/smoke/<name>_smoke.m`.

Three real bugs in cycles 65-75 were caught only by parity cross-check —
hand-written smokes had passed all three. Don't trust your own
expected values: trust the reference engine.

## Bug catalog (`bugs/`)

Structured one-file-per-bug catalog.
**Every bug you find gets TWO things:**

1. its own `bugs/<namespace>/<fn>.md` with a self-contained repro (numkit
   output vs MATLAB R2025b) so any session can act on it cold; and
2. a matching **`DISABLED_` gtest** in that namespace's `known_bugs_test.cpp`
   (`src/toolboxes/<ns>/tests/` for a toolbox; `tests/builtin/` for the
   math / lang / runtime base layers) asserting the MATLAB-correct behaviour —
   **found a bug → add a test.**
   `DISABLED_` keeps the green baseline green (it doesn't run normally) but
   the test is real: it fails under `--gtest_also_run_disabled_tests` and
   becomes a live regression guard the instant you remove the prefix.

Each file carries a **`Kind:`** tag separating real defects from parity
feature-gaps: `bug` (implemented fn, wrong/divergent result or ignored
option), `stub` (documented option/branch throws "not supported"),
`missing-output` (documented Nth output not emitted), `missing-fn` (function
not implemented at all — a **parity gap, also tracked in PROGRESS.md, NOT a
defect**), `perf` (correct but slower than MATLAB — use a `Slowdown:` line +
a benchmark, not a `DISABLED_` gtest). Don't conflate them — a missing
function is not a bug. **`perf` threshold:** numkit is single-threaded vs
MATLAB's multithreaded MKL/FFTW, so flag only ≥3× (or 1.5–3× with a FIXABLE
cause, or ANY ratio if worse big-O); <1.5× is noise. See bugs/README.md.

See [bugs/README.md](bugs/README.md) for the template + Kind legend + index.
When you fix a bug, remove the test's `DISABLED_` prefix, flip the md status
to ✅ FIXED with the commit hash, and update the index row (keep the md).

## Memory

Auto-memory at
`C:/Users/User/.claude/projects/c--Users-User-Projects-numkit-m/memory/`.
Always check `MEMORY.md` index for context before non-trivial work.
