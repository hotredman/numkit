# `bugs/` — one file per bug

Structured bug catalog. **Every bug gets its own `.md` file** here, with a
self-contained repro (numkit output vs MATLAB R2025b) so any session can
pick it up cold. This is the sole bug tracker (the old flat BUGS.md was retired —
its open items migrated here). The full MATLAB parity-gap inventory (missing / partial
functions) lives in [missing.md](missing.md).

## Layout

```
bugs/
  README.md              ← this file (index + conventions)
  opened/<namespace>/<fn>.md  ← OPEN bugs (active work)
  closed/<namespace>/<fn>.md  ← FIXED bugs, same structure, archived

Mirror trees: a bug lives its whole life under its namespace, moving
`opened/ → closed/` when fixed. A namespace directory exists (in either
tree) only while it has a bug — remove empties, recreate on demand.
```

Use `<fn>.md` when a function has one open bug; `<fn>-<aspect>.md` when it
has several distinct ones (e.g. `cceps-nd-phase.md`).

`<namespace>` is the toolbox or **source layer** the function lives in — i.e.
where its code (and its `known_bugs_test.cpp`) is found. Domain toolboxes keep
their own name (`signal/`, `image/`, `stats/`, `linalg/`, `control/`, `comm/`,
`optim/`, `wavelet/`, `ode/`, `io/`). The former `builtin` mega-library was
dissolved by the layering refactor and its defects are split across the three
source layers it became:

- **`math/`** — `src/math/` (trig, exp/log, special, poly, interp, integration,
  discrete/setops, reductions, complex): acos/asin, gamma, psi, log, sqrt,
  trapz, gradient, interp1/interpn, unique, histcounts, ismember/union, max/min…
- **`lang/`** — `src/lang/` (arrays, strings, format, types): cat, sort, find,
  diff, cumsum/cumprod/cummax/cummin, sprintf, str2double, integer casts…
- **`runtime/`** — `src/runtime/` (language-runtime builtins): accumarray,
  cellfun, func2str.
- **`core/`** — `src/core/` (interpreter: lexer, parser, compiler, VM,
  TreeWalker): parser crashes, stack-safety defects…

Each base layer keeps its gtests in its own module tree --
`src/math/tests/`, `src/lang/tests/`, `src/runtime/tests/`. The cross-cutting
batches and the base-layer `known_bugs_test.cpp` live in `src/bundle/tests/`.

## File template

```markdown
# <namespace>.<fn> — <one-line title>

- **Status:** 🔴 OPEN  |  ✅ FIXED (<commit>, YYYY-MM-DD)
- **Severity:** P1 wrong result · P2 missing feature · P3 minor/style
- **Found:** YYYY-MM-DD via <how>

## Symptom
What is wrong, in one or two sentences.

## Repro
​```matlab
<exact call>
% numkit: <output>
% MATLAB: <output>
​```

## Root cause
If known.

## Suggested fix
Approach + scope estimate; note any deferral reason (objects, core change,
large algorithm).

## References
Source files, related commits, related specs/tests.
```

## Severity legend

- **P0** crash / data loss
- **P1** wrong result (silently incorrect output)
- **P2** missing feature / option / output relative to MATLAB
- **P3** test-only / style

## Kind legend

Distinguishes a true defect from a parity feature-gap — so the count of
real bugs isn't inflated by unimplemented functions:

- **bug** — an IMPLEMENTED function produces a wrong/divergent result,
  crashes, or silently ignores a documented option. A genuine defect.
- **stub** — the function exists but a documented option/branch throws
  "not supported in this revision".
- **missing-output** — the function exists but a documented Nth output is
  missing ("Too many output arguments").
- **missing-fn** — the function is not implemented at all. This is a
  **parity feature-gap, not a defect** — also tracked in `PROGRESS.md`.
- **perf** — the function is CORRECT but significantly slower than MATLAB.
  Use a `**Slowdown:**` line (e.g. "1.2×–4.3× vs MATLAB") instead of a P0–P3
  severity, and reference a **benchmark** (`benchmarks/*.cpp`) rather than a
  `DISABLED_` gtest — timing assertions are too flaky for gtest. Always
  include the measured numbers + the bottleneck analysis.

  **When to flag as `perf`** (numkit is single-threaded; MATLAB is often
  multithreaded + MKL/FFTW, so a 1.5–3× gap on parallelisable ops is normal,
  not a bug):
  - **< 1.5×** — don't flag (noise / inherent).
  - **1.5×–3×** — flag only if the cause is FIXABLE (quadratic algorithm,
    redundant copies/allocs, a SIMD path that exists for sibling functions).
    If the only cause is "MATLAB threads, we don't", note it as *inherent*,
    low priority.
  - **≥ 3×** — flag (`perf` with measured numbers).
  - **≥ 10× OR worse big-O** (e.g. O(n²) where MATLAB is O(n log n)) —
    high priority; flag at ANY ratio.
  - An **algorithmic** inefficiency (worse big-O, allocs inside a loop) is a
    perf bug at ANY ratio — it scales and is fixable.

  Measure at a representative size (≥ ~10³–10⁴ elements), median of many
  iterations; ignore tiny arrays (wrapper overhead dominates both engines).
  Slowdown sub-scale: **S1** ≥10× or worse-big-O · **S2** 3–10× · **S3**
  1.5–3× with a fixable cause.

Add `- **Kind:** <kind>` to each file (right after Severity).

## Every bug also gets a test

**Found a bug → add a test.** Each OPEN bug has a matching `DISABLED_`
gtest in `src/toolboxes/<lib>/tests/known_bugs_test.cpp` that asserts the
MATLAB-correct behaviour. Disabled means it does NOT run in the normal
suite (the green baseline stays green), but it is visible
(`YOU HAVE N DISABLED TESTS`) and **fails when force-run**
(`--gtest_also_run_disabled_tests`), proving it captures the bug. When the
bug is fixed, just remove the `DISABLED_` prefix — the test becomes a live
regression guard with zero extra work.

Run all known-bug tests (to watch them fail until fixed):
```
numkit_gtest.exe --gtest_also_run_disabled_tests --gtest_filter='*KnownBug*'
```

## Lifecycle

1. Find a bug → create `bugs/opened/<ns>/<fn>.md` (status OPEN) with full repro,
   AND add a `DISABLED_` test in `src/toolboxes/<ns>/tests/known_bugs_test.cpp`.
2. Fix it (4 artefacts) → remove `DISABLED_` (or promote the assertion into
   the function's own test file), flip the md status to ✅ FIXED with the
   commit hash, and **`git mv` the md from `opened/<ns>/` to `closed/<ns>/`**
   (structure preserved). The repro stays useful in the archive.

## Bug Discovery Playbook

When probing or fuzzing functions, systematically test these 5 dimensions:

1. **Shape & Domain Edge-Cases**:
   - Empty matrices `[]` (0×0, 0×1, 1×0, 0×3), singletons `1x1`, row vs column vectors.
   - N-D arrays ($3\text{D}+$ tensors), non-square matrices, negative or zero dimensions.
   - Special floating-point values: `0`, `-0.0`, `eps`, `Inf`, `-Inf`, `NaN`.
2. **Type Polymorphism & Saturation**:
   - Pass integer types (`int8`, `uint8`, `int32`, `uint64`), `logical`, `char`, `string`, and `complex` to functions expecting numeric arguments.
   - Check if integer classes are preserved or correctly promoted according to MATLAB conventions.
3. **Dual-Engine Consistency (TreeWalker vs Bytecode VM)**:
   - Run expressions through `DualEngineTest`. Any divergence in values, error codes, or `nargout` handling between TreeWalker and VM is an immediate bug.
4. **Multiple Outputs (`nargout`)**:
   - Verify 1, 2, 3+ return values (e.g. `[y, idx] = min(...)`, `[b, a] = butter(...)`, `[u, s, v] = svd(...)`).
5. **Numerical Tolerance vs Bug**:
   - Floating-point differences $< 10^{-12}$ (for double) due to SIMD/FMA contraction or summation order are normal numerical variance, **NOT bugs**.
   - Flag as a bug only when results diverge algorithmically ($> 10^{-9}$), outputs have wrong shape/type, or an unexpected error/crash occurs.

## Self-Contained Repro Rule
Every `## Repro` block in `bugs/opened/<ns>/<fn>.md` **MUST be 100% copy-paste runnable**:
- Start with `clear;` — numkit runs the snippet with `--compat` (implicit
  `import compat.*`), so the block stays copy-paste valid in MATLAB too.
- Define all input variables explicitly inline (e.g., `x = [1, 2, 3];`).
- Include the exact function call and comment the expected MATLAB vs NumKit output.

## Catalog & Status

The filesystem (`bugs/opened/` and `bugs/closed/`) is the single source of truth:
- **Active bugs** are stored in `bugs/opened/<namespace>/<fn>.md`.
- **Resolved bugs** are archived in `bugs/closed/<namespace>/<fn>.md`.
- **Parity gaps / missing MATLAB functions** (not defects) are tracked in [missing.md](missing.md).

To view the live status and full list of open bugs at any time, run:

```bash
python tools/bugs_tally.py
```
