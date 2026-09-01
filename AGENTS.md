# numkit repo notes for AI coding agents (AGENTS.md)

## STOP — read first

This repo **can** be split across parallel AI-agent sessions (core / ide / lib
territories), but that model is **currently dormant — normally you are the
single session working on `main`**. The territory / worktree protocol is kept
in [coordination.md](dev-docs/handbook/coordination.md) as reference for if it is
revived (territory ownership, shared-surface test rules, build isolation,
commit/branch protocol).

Either way: if `git status` shows unstaged changes you did not make, **stop and
surface to the user**. Do not silently work on top of someone else's work.

## Project quick facts

- C++ MATLAB-compatible runtime: parser → AST → (TreeWalker | Bytecode VM).
- Two backends, dual-engine tests via `DualEngineTest` fixture.
- Build presets: `desktop-fast` (default, x86_64 + Highway SIMD),
  `portable`, `apple-m`, `browser` (WASM via emsdk).
- Build dir = `build-<preset>/` per source root; runs of cmake never share
  binaryDir between worktrees.
- Test runner: `build-<preset>/tests/gtest/Release/numkit_gtest.exe`.
- WASM: `scripts/engine-build.sh --wasm` with emsdk env sourced; rebuild the IDE web bundle via `scripts/web-build.sh` (see `scripts/README.md`).

## Commits

- Conversational Russian, code/commits/tests English.
- **Commit messages — English only, subject AND body** (user rule,
  2026-09-01). A commit is a permanent, searchable artifact read by
  tools (`git log --grep`) and strangers. Bug ids, file paths and code
  snippets stay as-is.
- Co-authored trailer required (see prior commits for style).
- `main` is the integration branch. Never force-push.

## Documentation map (`dev-docs/`)

Structure and the rules for using it:

| Directory | Holds | Read when | Write when |
|---|---|---|---|
| `dev-docs/handbook/` | living docs: rules, references, how-tos, protocols | situational must-reads below | a NEW living rule/reference earns a file here + a row in `dev-docs/README.md` |
| `dev-docs/memory/` | reasoning archive: decisions, campaign logs, rationale | **before any major task** — scan for files touching your components | after a major task / significant decision / non-obvious gotcha (see "Project memory" below) |
| `dev-docs/todo/` | open non-defect work (tech-debt, deferred design) — one file per task | when picking up deferred work | when work is deliberately deferred (not a bug → not `bugs/`; not done → not `memory/`); on completion record the outcome in `memory/` and delete the file |

Situational must-reads (in `handbook/`): `library_api.md` (any public
`toolboxes/` function), `callback_pausability.md` (VM callbacks),
`object_model.md` (classdef/object model), `core_architecture.md` (engine
design), plus `src/codegen/DESIGN.md` for the transpiler. `dev-docs/README.md`
is the full map.

Naming: content documents are `lowercase_snake.md` everywhere (`handbook/`,
`memory/`, `todo/`, `bugs/missing.md`); CAPS is reserved for tool-discovered
entry points (`README.md`, `AGENTS.md`, `LICENSE`).

## Public API conventions

Every public function in `src/toolboxes/<ns>/include/numkit/<ns>/**` follows
[dev-docs/handbook/library_api.md](dev-docs/handbook/library_api.md) — the authoritative API
ruleset (argument order, native scalar types vs `const Value &` vs
`Span<const double>`, `FnHandle` callbacks, no `Engine *` in public
signatures, magic-polymorphism → typed overloads, multi-output return
shape). Read it before adding or refactoring any public `src/toolboxes/`
function.

## Test-running policy

**Full suite is expensive (~5 min) — run it ONLY when the user asks.**
For every change, run the MINIMUM set that proves YOUR change is correct:

| Change type | Minimum verification |
|---|---|
| Single function / builtin | `--gtest_filter` for the affected suite(s) only |
| Parser / lexer / compiler | The specific syntax-family tests + any DISABLED guard |
| Registration / bundle wiring | The domain's `*_test` + one CLI smoke (`-e "..."`) |
| Bug fix | Enable the guard, verify IT passes; related suite only |
| Refactor (no behavior change) | The suite(s) whose files you touched |

A green targeted run is sufficient to commit. If the user asks for a
full run (or before a publish), the command is:
`build/desktop-fast/tests/gtest/Release/numkit_gtest.exe`

## Smoke tests

- Hand-runnable `.m` smokes live in `src/toolboxes/<name>/tests/smoke/*_smoke.m`
  (one per public function or related cluster). Run via
  `build/desktop-fast/apps/numkit/Release/numkit_repl.exe <path>`.
- **Every smoke MUST start with `clear` on the very first line**, then the
  body. This ensures no leftover workspace state from a prior run leaks
  into the test. No `import compat.*` — builtins are globally registered
  since the builtin consolidation (verified by the full 710-smoke sweep
  2026-08-30: 699 pass bare, the 11 failures are unrelated and filed).

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
across src/toolboxes/stats, tests/math, src/toolboxes/signal):
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
`build/desktop-fast/apps/numkit/Release/numkit_repl.exe src/toolboxes/<lib>/tests/smoke/<name>_smoke.m`.

Three real bugs in cycles 65-75 were caught only by parity cross-check —
hand-written smokes had passed all three. Don't trust your own
expected values: trust the reference engine.

## Project memory (`dev-docs/memory/`)

Decentralised memory files persist architectural decisions, gotchas and
performance logs across sessions. Before a major task, scan
`dev-docs/memory/` for files touching your components. After completing a
major task, making a significant architectural decision, or discovering a
non-obvious gotcha — document it there (new file per logical domain, e.g.
`linalg_lu_performance.md`; or append to an existing one). Content: the
problem/context, the chosen solution, the rationale, and any measured
numbers.

## Bug catalog (`bugs/`)

Structured one-file-per-bug catalog.
**Every bug you find gets TWO things:**

1. its own `bugs/opened/<namespace>/<fn>.md` with a self-contained repro (numkit
   output vs MATLAB R2025b) so any session can act on it cold; and
2. a matching **`DISABLED_` gtest** in that namespace's `known_bugs_test.cpp`
   (`src/toolboxes/<ns>/tests/` for a toolbox; `src/bundle/tests/` for the
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

See [bugs/README.md](bugs/README.md) for the template + Kind legend. Before closing
a session run `python tools/bugs_audit.py` (protocol: guards ↔ bugs linkage);
before a release add `--guards <numkit_gtest.exe>` (every DISABLED_ guard must fail).
When you fix a bug, remove the test's `DISABLED_` prefix, flip the md status
to ✅ FIXED with the commit hash, and `git mv` the md from
`bugs/opened/<namespace>/` to `bugs/closed/<namespace>/` (structure
preserved).

### Bug Discovery Playbook for Agents

When hunting for bugs or fuzzing a function, systematically test these 5 vectors:

1. **Shape & Domain Edge-Cases**:
   - Empty inputs `[]` (0×0, 0×1, 1×0, 0×3), singletons `1x1`, scalars vs row vs column vectors.
   - N-D arrays ($3\text{D}+$ tensors), non-square matrices, negative/zero dimensions.
   - Special values: `0`, `-0.0`, `eps`, `Inf`, `-Inf`, `NaN`.
2. **Type Polymorphism & Saturation**:
   - Pass integer types (`int8`, `uint8`, `int32`, `uint64`), `logical`, `char`, and `complex` to functions expecting double.
   - Check if integer classes are preserved or correctly promoted according to MATLAB conventions.
3. **Dual-Engine Consistency (TreeWalker vs Bytecode VM)**:
   - Run expressions through `DualEngineTest`. Any divergence in values, error codes, or `nargout` handling between TreeWalker and VM is an immediate bug.
4. **Multiple Outputs (`nargout`)**:
   - Verify 1, 2, 3+ return values (e.g. `[y, idx] = min(...)`, `[b, a] = butter(...)`, `[u, s, v] = svd(...)`).
5. **Numerical Tolerance vs Bug**:
   - Floating-point differences $< 10^{-12}$ (for double) due to SIMD/FMA contraction or summation order are normal numerical variance, **NOT bugs**.
   - Flag as a bug only when results diverge algorithmically ($> 10^{-9}$), outputs have wrong shape/type, or an unexpected error/crash occurs.

### Self-Contained Repro Rule
Every `## Repro` block in `bugs/opened/<ns>/<fn>.md` **MUST be 100% copy-paste runnable**:
- Start with `clear;` — numkit runs the snippet with `--compat` (implicit
  `import compat.*`), so the block stays copy-paste valid in MATLAB too.
- Define all input variables explicitly inline (e.g., `x = [1, 2, 3];`).
- Include the exact function call and comment the expected MATLAB vs NumKit output.
- **No fieldtest/corpus references in the Repro.** A bug found in real-world
  corpus code is distilled to a minimal inline snippet FIRST — the bug file
  must survive `rm -rf fieldtest/corpus` (the corpus is disposable by design;
  it is rebuilt from the external catalog). "Run the corpus script" is not a
  repro.
- A single `Found:` provenance line may mention fieldtest (real-world
  discovery is a prioritization signal) — as metadata only, never as the
  reproducer.

