# `bugs/` — one file per bug

Structured bug catalog. **Every bug gets its own `.md` file** here, with a
self-contained repro (numkit output vs MATLAB R2025b) so any session can
pick it up cold. This complements the flat append-only [BUGS.md](../BUGS.md)
(quick running log) and is distinct from `audit/findings/**` (the parallel
auditor worker's territory — do not write there from the main worker).

## Layout

```
bugs/
  README.md              ← this file (index + conventions)
  <namespace>/<fn>.md    ← one bug (e.g. signal/dct-types.md)
```

Use `<fn>.md` when a function has one open bug; `<fn>-<aspect>.md` when it
has several distinct ones (e.g. `cceps-nd-phase.md`).

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

## Severity legend (matches BUGS.md)

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
gtest in `libs/<lib>/tests/known_bugs_test.cpp` that asserts the
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

1. Find a bug → create `bugs/<ns>/<fn>.md` (status OPEN) with full repro,
   AND add a `DISABLED_` test in `libs/<ns>/tests/known_bugs_test.cpp`.
2. Fix it (4 artefacts) → remove `DISABLED_` (or promote the assertion into
   the function's own test file), flip the md status to ✅ FIXED with the
   commit hash, and update the index row. Keep the md (repro stays useful).

## Index

**Tally (43 entries):** ✅ 8 fixed · 🔴 35 open = **9 bug** + 7 stub +
5 missing-output + **13 missing-fn** + 1 perf (the 13 missing-fns are parity
feature-gaps, not defects — also in PROGRESS.md; perf = correct-but-slow).

### ✅ FIXED (8)

| Kind | Bug | Sev | Notes |
|---|---|---|---|
| bug | [builtin/sort-missingplacement](builtin/sort-missingplacement.md) | P1 | 'MissingPlacement' option was ignored |
| bug | [signal/rceps-cceps-padding](signal/rceps-cceps-padding.md) | P1 | cepstrum garbage on non-2ⁿ + rceps 2nd output (9fcf6872) |
| bug | [signal/besself-digital](signal/besself-digital.md) | P1 | ran digital path → binomial garbage |
| bug | [builtin/max-all-linear](builtin/max-all-linear.md) | P1 | max/min(A,[],'all') was entirely broken |
| bug | [stats/combnk-scalar](stats/combnk-scalar.md) | P3 | scalar v is the 1-element set {v}; K>N → empty 0×K (c179) |
| bug | [stats/anova1-matrix-input](stats/anova1-matrix-input.md) | P2 | matrix columns-as-groups input form (c179) |
| bug | [builtin/unique-last](builtin/unique-last.md) | P1 | 'last' selects last occurrence (sorted; stable+last sub-gap deferred) (c180) |
| missing-output | [signal/spectrogram-ps](signal/spectrogram-ps.md) | P2 | missing 4th output PSD (1128db65) |

### 🔴 OPEN — bug (defect on an implemented function) — 9

| Bug | Sev | Notes |
|---|---|---|
| [signal/instfreq-instbw](signal/instfreq-instbw.md) | P1 | wrong values (negative on a chirp) |
| [signal/impinvar-repeated-poles](signal/impinvar-repeated-poles.md) | P1 | wrong numerator for repeated poles |
| [signal/resample-values](signal/resample-values.md) | P1 | wrong output values (multirate) |
| [signal/cceps-nd-phase](signal/cceps-nd-phase.md) | P1 | non-2ⁿ phase wrong (rcunwrap) + missing `nd` |
| [signal/freqs-scalar-w](signal/freqs-scalar-w.md) | P3 | scalar w should be N points (needs freqint auto-range) |
| [stats/kstest-pvalue](stats/kstest-pvalue.md) | P1 | p-value/cv wrong (kstest + kstest2; stat OK) |
| [stats/dwtest-pvalue](stats/dwtest-pvalue.md) | P2 | DW stat OK, p-value method differs |
| [stats/mahal-singular](stats/mahal-singular.md) | P2 | throws on rank-deficient reference |
| [image/regionprops-perimeter](image/regionprops-perimeter.md) | P1 | unknown property silently dropped |

### 🔴 OPEN — stub (option/branch throws "not supported") — 7

| Bug | Sev | Notes |
|---|---|---|
| [signal/dct-types](signal/dct-types.md) | P2 | Type 1/3/4 throw |
| [signal/findpeaks-widthreference](signal/findpeaks-widthreference.md) | P2 | 'halfheight'/'halfprom' throw |
| [signal/ellipord-bandstop](signal/ellipord-bandstop.md) | P2 | bandstop case throws |
| [stats/smoothdata-methods](stats/smoothdata-methods.md) | P2 | sgolay/lowess/loess throw |
| [stats/isoutlier-gesd](stats/isoutlier-gesd.md) | P2 | 'gesd' method throws |
| [builtin/histcounts-autobinning](builtin/histcounts-autobinning.md) | P2 | automatic binning throws |
| [wavelet/dwt-biorthogonal](wavelet/dwt-biorthogonal.md) | P2 | bior*/rbio* families throw |

### 🔴 OPEN — missing-output (Nth output not emitted) — 5

| Bug | Sev | Notes |
|---|---|---|
| [linalg/qr-pivoting](linalg/qr-pivoting.md) | P2 | column-pivoting [Q,R,P] |
| [linalg/eig-left-vectors](linalg/eig-left-vectors.md) | P2 | 3rd output W (left eigenvectors) |
| [stats/mle-output](stats/mle-output.md) | P2 | 2nd output pci |
| [stats/corr-pvalue](stats/corr-pvalue.md) | P2 | [r,p]=corr p-value |
| [signal/risetime-falltime-outputs](signal/risetime-falltime-outputs.md) | P2 | only 1 of up to 5 outputs |

### 🔴 OPEN — missing-fn (not implemented — PARITY GAP, not a defect) — 13

| Bug | Sev | Notes |
|---|---|---|
| [stats/friedman](stats/friedman.md) | P2 | Friedman ANOVA |
| [stats/distribution-dispatchers](stats/distribution-dispatchers.md) | P2 | cdf/pdf/icdf/random |
| [signal/pmusic-peig](signal/pmusic-peig.md) | P2 | pmusic/peig |
| [signal/fillgaps](signal/fillgaps.md) | P2 | fillgaps |
| [signal/stmcb](signal/stmcb.md) | P2 | stmcb |
| [image/watershed](image/watershed.md) | P2 | watershed |
| [image/imfindcircles](image/imfindcircles.md) | P2 | imfindcircles |
| [wavelet/wpdec](wavelet/wpdec.md) | P2 | wavelet packets (needs tree type) |
| [wavelet/wentropy-ddencmp](wavelet/wentropy-ddencmp.md) | P2 | wentropy / ddencmp |
| [control/lqr-hinfnorm](control/lqr-hinfnorm.md) | P2 | lqr/hinfnorm/dlqr/gram |
| [comm/analog-demodulators](comm/analog-demodulators.md) | P2 | am/fm/pm/ssb/msk demod |
| [optim/nonlinear-lsq](optim/nonlinear-lsq.md) | P2 | lsqcurvefit/lsqnonlin |
| [optim/constrained-solvers](optim/constrained-solvers.md) | P2 | fmincon/linprog/quadprog/fminunc |

### 🔴 OPEN — perf (correct but slower than MATLAB) — 1

| Entry | Slowdown | Notes |
|---|---|---|
| [signal/fft-speed](signal/fft-speed.md) | 1.2×–4.3× | single-threaded vs FFTW; Highway already present, gap is threading + MSVC codegen + wrapper |
