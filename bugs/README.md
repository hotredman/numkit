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

| Bug | Sev | Status | Notes |
|---|---|---|---|
| [builtin/sort-missingplacement](builtin/sort-missingplacement.md) | P1 | ✅ FIXED | 'MissingPlacement' option was ignored |
| [signal/rceps-cceps-padding](signal/rceps-cceps-padding.md) | P1 | ✅ FIXED 9fcf6872 | cepstrum garbage on non-2ⁿ lengths + rceps 2nd output |
| [signal/spectrogram-ps](signal/spectrogram-ps.md) | P2 | ✅ FIXED 1128db65 | missing 4th output (PSD) |
| [signal/dct-types](signal/dct-types.md) | P2 | 🔴 OPEN | Type 1/3/4 stubbed |
| [signal/cceps-nd-phase](signal/cceps-nd-phase.md) | P1 | 🔴 OPEN | non-2ⁿ phase (rcunwrap) + missing `nd` output |
| [signal/risetime-falltime-outputs](signal/risetime-falltime-outputs.md) | P2 | 🔴 OPEN | only 1 of up to 5 outputs |
| [signal/findpeaks-widthreference](signal/findpeaks-widthreference.md) | P2 | 🔴 OPEN | 'halfheight'/'halfprom' option |
| [signal/pmusic-peig](signal/pmusic-peig.md) | P2 | 🔴 OPEN | functions missing |
| [signal/fillgaps](signal/fillgaps.md) | P2 | 🔴 OPEN | function missing |
| [signal/ellipord-bandstop](signal/ellipord-bandstop.md) | P2 | 🔴 OPEN | bandstop case throws |
| [image/regionprops-perimeter](image/regionprops-perimeter.md) | P1 | 🔴 OPEN | unknown property silently dropped |
| [stats/smoothdata-methods](stats/smoothdata-methods.md) | P2 | 🔴 OPEN | sgolay/lowess/loess throw |
| [stats/isoutlier-gesd](stats/isoutlier-gesd.md) | P2 | 🔴 OPEN | 'gesd' method throws |
| [signal/instfreq-instbw](signal/instfreq-instbw.md) | P1 | 🔴 OPEN | wrong values (negative on a chirp) |
| [stats/anova1-matrix-input](stats/anova1-matrix-input.md) | P2 | 🔴 OPEN | matrix (column-per-group) form throws |
| [stats/mle-output](stats/mle-output.md) | P2 | 🔴 OPEN | missing 2nd output (pci) |
| [stats/distribution-dispatchers](stats/distribution-dispatchers.md) | P2 | 🔴 OPEN | cdf/pdf/icdf/random missing |
| [control/lqr-hinfnorm](control/lqr-hinfnorm.md) | P2 | 🔴 OPEN | functions missing |
| [image/watershed](image/watershed.md) | P2 | 🔴 OPEN | function missing |
| [image/imfindcircles](image/imfindcircles.md) | P2 | 🔴 OPEN | function missing |
| [wavelet/wpdec](wavelet/wpdec.md) | P2 | 🔴 OPEN | wavelet packets missing (needs tree type) |
| [wavelet/wentropy-ddencmp](wavelet/wentropy-ddencmp.md) | P2 | 🔴 OPEN | functions missing (small) |
| [wavelet/dwt-biorthogonal](wavelet/dwt-biorthogonal.md) | P2 | 🔴 OPEN | bior*/rbio* families unsupported |
| [comm/analog-demodulators](comm/analog-demodulators.md) | P2 | 🔴 OPEN | am/fm/pm/ssb/msk demod missing (mods exist) |
| [optim/nonlinear-lsq](optim/nonlinear-lsq.md) | P2 | 🔴 OPEN | lsqcurvefit/lsqnonlin missing |
| [optim/constrained-solvers](optim/constrained-solvers.md) | P2 | 🔴 OPEN | fmincon/linprog/quadprog/fminunc missing |
| [linalg/qr-pivoting](linalg/qr-pivoting.md) | P2 | 🔴 OPEN | column-pivoting [Q,R,P] missing |
| [linalg/eig-left-vectors](linalg/eig-left-vectors.md) | P2 | 🔴 OPEN | 3rd output W (left eigenvectors) missing |
| [builtin/histcounts-autobinning](builtin/histcounts-autobinning.md) | P2 | 🔴 OPEN | automatic binning unsupported |
| [stats/kstest-pvalue](stats/kstest-pvalue.md) | P1 | 🔴 OPEN | p-value/cv wrong (statistic OK) |
| [stats/friedman](stats/friedman.md) | P2 | 🔴 OPEN | function missing |
| [builtin/unique-last](builtin/unique-last.md) | P1 | 🔴 OPEN | 'last' option ignored (ia = first occurrence) |
| [builtin/max-all-linear](builtin/max-all-linear.md) | P2 | 🔴 OPEN | max(A,[],'all','linear') errors |
