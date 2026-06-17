# `bugs/` — one file per bug

Structured bug catalog. **Every bug gets its own `.md` file** here, with a
self-contained repro (numkit output vs MATLAB R2025b) so any session can
pick it up cold. This is the sole bug tracker (the old flat BUGS.md was retired —
its open items migrated here). The full MATLAB parity-gap inventory (missing / partial
functions) lives in [PARITY_GAPS.md](PARITY_GAPS.md).

## Layout

```
bugs/
  README.md              ← this file (index + conventions)
  <namespace>/<fn>.md    ← one bug (e.g. signal/dct-types.md)
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

1. Find a bug → create `bugs/<ns>/<fn>.md` (status OPEN) with full repro,
   AND add a `DISABLED_` test in `src/toolboxes/<ns>/tests/known_bugs_test.cpp`.
2. Fix it (4 artefacts) → remove `DISABLED_` (or promote the assertion into
   the function's own test file), flip the md status to ✅ FIXED with the
   commit hash, and update the index row. Keep the md (repro stays useful).

## Index

**Tally (108 entries):** ✅ 59 fixed · 🔴 49 open = **11 bug** + 5 stub +
2 missing-output + **30 missing-fn** + 1 perf (the 30 missing-fns are parity
feature-gaps, not defects — also in PROGRESS.md; perf = correct-but-slow).

> **Full parity-gap inventory:** the 30 missing-fn rows below are the *curated /
> notable* gaps. The complete list of **839 missing + 25 partial** MATLAB
> functions (migrated out of PROGRESS.md, grouped by namespace) lives in
> [PARITY_GAPS.md](PARITY_GAPS.md). Those are parity gaps, **not defects** —
> they are NOT counted in the tally above.

### ✅ FIXED (59)

| Kind | Bug | Sev | Notes |
|---|---|---|---|
| stub | [math/maxmin-complex](math/maxmin-complex.md) | P2 | binary max(A,B)/min(A,B) + clamp accept complex - by modulus then angle(z), NaN-component omitted (omitnan default); no all-real fallback (max(complex(-3,0),1)=-3). Prior VM-dispatch-blocker note was a misdiagnosis: default omitnan routes to maxOmitNanBinary, not max (2026-06-17) |
| bug | [math/complex-zero-imag-narrowing](math/complex-zero-imag-narrowing.md) | P2 | MATLAB narrows an all-zero-imaginary complex RESULT back to real (isreal(2+0i)=1); complex() stays forced. narrowComplex (value layer) applied across arithmetic/unary/matmul/fused/indexing/reductions(sum,prod,mean,var,std,cumsum,cumprod,diff,median)/linalg(dot,kron,cross,diag)/reorder; reshape/transpose/sort/cat/unique correctly preserve. In-place indexed-assign deliberately NOT narrowed (eager scan is O(n^2) in fill loops, ~250x; narrows on next op). Both backends (2026-06-17) |
| bug | [image/adapthisteq-mapping](image/adapthisteq-mapping.md) | P2 | CLAHE was ~54% too bright; ported MATLAB clip (ceil/round + step-redistribute) + map (rayleigh vmax) + integer-weight region interpolation over even-tile padding → matches MATLAB to ±1 level (was a regression; tonemap re-matches too) (2026-06-14) |
| bug | [lang/int-cast-rtne](lang/int-cast-rtne.md) | P2 | int32/int64/uint32 SIMD cast rounded ties-to-even; now half-away-from-zero (MATLAB) via trunc(v+copysign(0.5,v)) (2026-06-14) |
| stub | [stats/jackknife](stats/jackknife.md) | P2 | jackknife(fn,X) now loops leave-one-out inline via callFunctionHandle (like bootstrp_reg) instead of a dead stub; vector reshaped to column observations (2026-06-14) |
| bug | [linalg/cross-integer-class](linalg/cross-integer-class.md) | P2 | cross preserves the integer class of integer operands with per-operation saturation (cross(int8([100 100 0]),int8([0 100 100]))=[127 -127 127], not -128); int+double→int; different-int/int+logical lenient→double (2026-06-05) |
| bug | [runtime/accumarray-integer-vals](runtime/accumarray-integer-vals.md) | P2 | accumarray accepts integer/logical vals (was: throw "vals must be DOUBLE") — sum/prod/mean→double, max/min preserve int class; logical+max/min & custom-handle class are lenient niches (2026-06-05) |
| bug | [signal/deconv-integer-input](signal/deconv-integer-input.md) | P2 | deconv accepts integer/logical input (was: throw "Not a double array") — promotes to double (reuses convPromoteToDouble); q+r always double. na>nb int is numkit-lenient (MATLAB errors there) (2026-06-05) |
| bug | [signal/conv-integer-input](signal/conv-integer-input.md) | P2 | conv accepts integer/logical input (was: throw "Not a double array") — promotes to double; result always double (every shape), never the int class (2026-06-05) |
| bug | [linalg/kron-integer-class](linalg/kron-integer-class.md) | P2 | kron preserves the integer class of integer operands (saturating) — same-int→that class, int+scalar-double→int (round+saturate), double*double unchanged (2026-06-05) |
| bug | [lang/concat-integer-types](lang/concat-integer-types.md) | P2 | CORE (user-approved): cat/[;]/[,]/vertcat/horzcat preserve integer class — first-int wins, others cast (round+saturate); concat in double then narrow (2026-06-05) |
| bug | [lang/str2double-complex](lang/str2double-complex.md) | P3 | str2double parses complex strings ('1+2i'/'2i'/'i'/'1+2j'/'1e-3+2i'); COMPLEX output when any element complex, real path unchanged (2026-06-05) |
| bug | [math/psi-zero-pole](math/psi-zero-pole.md) | P3 | psi(0) returns -Inf (digamma pole), was NaN; finite values + negative-domain unchanged, matches MATLAB (2026-06-05) |
| bug | [math/polyder-product](math/polyder-product.md) | P2 | polyder(a,b) single-output = derivative of the PRODUCT a*b (was the quotient numerator); 2-output quotient form unchanged (2026-06-05) |
| bug | [math/gamma-negative-integer-poles](math/gamma-negative-integer-poles.md) | P3 | gamma returns +Inf at non-positive integer poles (was NaN via std::tgamma); gamma(-Inf)=Inf, matches MATLAB (2026-06-05) |
| bug | [math/maxmin-char-double](math/maxmin-char-double.md) | P2 | max/min of a char array return double (the code point), not char — MATLAB does not preserve char for max/min (mode does); flipped the stale char-return test (2026-06-05) |
| bug | [lang/sprintf-complex](lang/sprintf-complex.md) | P2 | sprintf/fprintf use the real part of a complex argument for numeric conversions (was: throw); imaginary discarded, as MATLAB (2026-06-05) |
| bug | [stats/movfun-order-stats](stats/movfun-order-stats.md) | P3 | movmax/movmin/movmedian accept integer/logical — movmax/movmin preserve class, movmedian rounds int half-away & logical→double (completes mov* sweep) (2026-06-05) |
| bug | [stats/movfun-typeclass](stats/movfun-typeclass.md) | P3 | movsum/movprod/movmean accept integer/logical — promote to double (char still errors, as MATLAB) (2026-06-05) |
| bug | [lang/cummax-cummin-integer](lang/cummax-cummin-integer.md) | P3 | cummax/cummin accept integer — preserve int class (promote→cummax/cummin→doubleToIntegerExact; exact, order stats) (2026-06-05) |
| bug | [math/setops-typeclass](math/setops-typeclass.md) | P2 | ismember/intersect/setdiff/union accept char/logical/integer — values preserve class, ia/ib & ismember loc stay double (2026-06-05) |
| bug | [math/unique-typeclass](math/unique-typeclass.md) | P2 | unique accepts char/logical/integer — preserves class on values, ia/ic stay double (promote→unique→narrowUniqueClass) (2026-06-05) |
| bug | [lang/sort-char](lang/sort-char.md) | P2 | sort accepts char — sorts by code point, preserves char class, index double (charizeSortResult narrow; shape-preserving, unlike toChar) (2026-06-05) |
| bug | [lang/sort-logical](lang/sort-logical.md) | P2 | sort accepts logical — values preserve logical class, index stays double (mirrors integer path) (2026-06-05) |
| bug | [math/trapz-logical](math/trapz-logical.md) | P2 | trapz accepts logical X/Y — promote→double at trapz_reg entry (class not preserved; matches cumtrapz) (2026-06-05) |
| bug | [lang/cumulative-logical](lang/cumulative-logical.md) | P2 | cumsum/cumprod/cummax/cummin accept logical — cumsum/cumprod→double, cummax/cummin→logical (class preserved) (2026-06-05) |
| bug | [signal/impinvar-repeated-poles](signal/impinvar-repeated-poles.md) | P1 | repeated-pole impinvar numerator — multiplicity partial fractions + Eulerian impulse-invariant z-kernel + centroid/Newton pole refine (clean-room) (2026-06-05) |
| stub | [signal/ellipord-bandstop](signal/ellipord-bandstop.md) | P2 | ellipord bandstop order/Wn — reciprocal bandpass→LP map WA=(WS·(WP1-WP2))/(WS²-WP1·WP2) (clean-room) (2026-06-05) |
| bug | [stats/distribution-array-params](stats/distribution-array-params.md) | P2 | *pdf/*cdf/*inv broadcast ARRAY params across all 16 distribution families — continuous + discrete (c29-38) |
| missing-output | [stats/corr-pvalue](stats/corr-pvalue.md) | P2 | [r,p]=corr 2nd output: Pearson p=2·tcdf(-\|t\|,n-2); Kendall/Spearman EXACT permutation p (small n); matrix diag=1 (2026-06-05) |
| missing-output | [stats/mle-output](stats/mle-output.md) | P2 | [phat,pci]=mle(...) confidence intervals (normal/exp/poisson/lognormal, Alpha) via *fit CIs (2026-06-05) |
| missing-output | [linalg/eig-left-vectors](linalg/eig-left-vectors.md) | P2 | [V,D,W]=eig(A) left eigenvectors W (W'*A=D*W', unit-norm, eig(A') reordered) (2026-06-05) |
| missing-output | [linalg/qr-pivoting](linalg/qr-pivoting.md) | P2 | column-pivoting [Q,R,P]=qr(A) — A*P=Q*R, decreasing-norm order, 'vector'/econ P (2026-06-05) |
| stub | [stats/isoutlier-gesd](stats/isoutlier-gesd.md) | P2 | isoutlier 'gesd' (Rosner generalized ESD) + MaxNumOutliers/ThresholdFactor (2026-06-05) |
| bug | [stats/kstest-pvalue](stats/kstest-pvalue.md) | P1 | kstest/kstest2 exact KS p-value (Marsaglia + Birnbaum-Tingey) + cv (2026-06-05) |
| bug | [stats/dwtest-pvalue](stats/dwtest-pvalue.md) | P2 | exact Durbin-Watson p-value via Imhof CF inversion + Tail option (2026-06-05) |
| bug | [runtime/cellfun-inputforms](runtime/cellfun-inputforms.md) | P2 | cellfun multi-cell zip + legacy string-name forms ('isempty'/'size'/'isclass'/…) (2026-06-05) |
| bug | [lang/diff-zero-order](lang/diff-zero-order.md) | P3 | diff order N must be a positive integer scalar — 0/neg/frac/non-scalar now error (was identity at 0) (2026-06-05) |
| bug | [math/gradient-3d](math/gradient-3d.md) | P2 | gradient supports N-D (3-D+) arrays — one gradient per dim up to nargout (2026-06-05) |
| bug | [stats/pdist-metrics](stats/pdist-metrics.md) | P2 | pdist/pdist2 gain 'seuclidean' + 'spearman'; cosine/correlation → NaN (not 1) on zero-norm/const row (2026-06-05) |
| bug | [lang/find-count-direction](lang/find-count-direction.md) | P1 | find(X,k[,'first'/'last']) now honours count + direction (single + multi-output) (2026-06-05) |
| bug | [math/complex-input-unsupported](math/complex-input-unsupported.md) | P2 | complex now accepted by trapz/cumtrapz/median/interp1/gradient/movmean/detrend/conv/filter (umbrella closed) (2026-06-05) |
| bug | [math/log-complex-promotion-arrays](math/log-complex-promotion-arrays.md) | P2 | log/log10/log2/log1p promote whole real arrays to complex out of domain (log1p: x<-1) (2026-06-05) |
| bug | [linalg/norm-complex](linalg/norm-complex.md) | P2 | norm() of a complex array by \|z\| (vector 1/2/Inf/p + matrix 1/Inf/'fro'; spectral deferred) (2026-06-05) |
| bug | [lang/diff-complex](lang/diff-complex.md) | P1 | diff() now differences real + imaginary parts (n-th order + dim) (2026-06-05) |
| bug | [lang/cumsum-complex](lang/cumsum-complex.md) | P2 | cumsum/cumprod accumulate complex element-wise (dim + reverse) (2026-06-05) |
| bug | [math/acos-asin-complex](math/acos-asin-complex.md) | P2 | acos/asin go complex for \|x\|>1 (via acosh for the correct branch; array promotes) (2026-06-05) |
| bug | [math/complex-promotion-arrays](math/complex-promotion-arrays.md) | P2 | sqrt/acosh/atanh promote whole real arrays to complex (+ atanh x<-1 branch sign) (2026-06-05) |
| bug | [lang/sort-missingplacement](lang/sort-missingplacement.md) | P1 | 'MissingPlacement' option was ignored |
| bug | [signal/rceps-cceps-padding](signal/rceps-cceps-padding.md) | P1 | cepstrum garbage on non-2ⁿ + rceps 2nd output (9fcf6872) |
| bug | [signal/besself-digital](signal/besself-digital.md) | P1 | ran digital path → binomial garbage |
| bug | [math/max-all-linear](math/max-all-linear.md) | P1 | max/min(A,[],'all') was entirely broken |
| bug | [stats/combnk-scalar](stats/combnk-scalar.md) | P3 | scalar v is the 1-element set {v}; K>N → empty 0×K (c179) |
| bug | [stats/anova1-matrix-input](stats/anova1-matrix-input.md) | P2 | matrix columns-as-groups input form (c179) |
| bug | [math/unique-last](math/unique-last.md) | P1 | 'last' selects last occurrence (sorted; stable+last sub-gap deferred) (c180) |
| stub | [signal/dct-types](signal/dct-types.md) | P2 | dct/idct Type 1/3/4 implemented (c181) |
| missing-output (+bug) | [signal/risetime-falltime-outputs](signal/risetime-falltime-outputs.md) | P1 | [R,LT,UT,LL,UL] outputs + sharp-edge value fix 0.224→0.198 (c182) |
| missing-output | [signal/spectrogram-ps](signal/spectrogram-ps.md) | P2 | missing 4th output PSD (1128db65) |
| bug | [io/writelines](io/writelines.md) | P2 | writelines string-array writes one line per element (was: only first) (2026-06-08) |

### 🔴 OPEN — bug (defect on an implemented function) — 11

| Bug | Sev | Notes |
|---|---|---|
| [math/interpn-nan](math/interpn-nan.md) | P2 | interpn 1-D grid-vector query returns NaN (2-D/3-D dispatch OK; 4+-D unimplemented) — migrated from old BUGS.md #31 |
| [linalg/complex-matrix-unsupported](linalg/complex-matrix-unsupported.md) | P2 | entire linalg suite (eig/svd/qr/lu/chol/det/inv/trace/…) rejects complex matrices |
| [signal/obw-value-outputs](signal/obw-value-outputs.md) | P1 | wrong 99% bandwidth value + missing [bw,flo,fhi,power] |
| [image/imresize-interp](image/imresize-interp.md) | P2 | bilinear/bicubic diverge (grid + boundary + antialias) — deferred-G |
| [runtime/func2str-anonymous](runtime/func2str-anonymous.md) | P2 | anon handle returns '@__anon_N' not the source text |
| [signal/instfreq-instbw](signal/instfreq-instbw.md) | P1 | wrong values (negative on a chirp) |
| [signal/resample-values](signal/resample-values.md) | P1 | wrong output values (multirate) |
| [signal/cceps-nd-phase](signal/cceps-nd-phase.md) | P1 | non-2ⁿ phase wrong (rcunwrap) + missing `nd` |
| [signal/freqs-scalar-w](signal/freqs-scalar-w.md) | P3 | scalar w should be N points (needs freqint auto-range) |
| [stats/mahal-singular](stats/mahal-singular.md) | P2 | throws on rank-deficient reference |
| [image/regionprops-perimeter](image/regionprops-perimeter.md) | P1 | unknown property silently dropped |

### 🔴 OPEN — stub (option/branch throws "not supported") — 5

| Bug | Sev | Notes |
|---|---|---|
| [linalg/schur-nonsymmetric](linalg/schur-nonsymmetric.md) | P2 | schur(A) throws on non-symmetric A (real Schur form deferred; eig values work) |
| [signal/findpeaks-widthreference](signal/findpeaks-widthreference.md) | P2 | 'halfheight'/'halfprom' throw |
| [stats/smoothdata-methods](stats/smoothdata-methods.md) | P2 | sgolay/lowess/loess throw |
| [math/histcounts-autobinning](math/histcounts-autobinning.md) | P2 | automatic binning throws |
| [wavelet/dwt-biorthogonal](wavelet/dwt-biorthogonal.md) | P2 | bior*/rbio* families throw |

### 🔴 OPEN — missing-output (Nth output not emitted) — 2

| Bug | Sev | Notes |
|---|---|---|
| [signal/periodogram-pxxc](signal/periodogram-pxxc.md) | P2 | ConfidenceLevel / pxxc CI 3rd output |
| [signal/spectrogram-fc-tc](signal/spectrogram-fc-tc.md) | P2 | 5th/6th outputs fc, tc (centroids) |

### 🔴 OPEN — missing-fn (not implemented — PARITY GAP, not a defect) — 30

*(Curated/notable subset — the full 839-missing + 25-partial inventory is in
[PARITY_GAPS.md](PARITY_GAPS.md).)*

| Bug | Sev | Notes |
|---|---|---|
| [stats/friedman](stats/friedman.md) | P2 | Friedman ANOVA |
| [stats/distribution-dispatchers](stats/distribution-dispatchers.md) | P2 | cdf/pdf/icdf/random |
| [stats/autocorr](stats/autocorr.md) | P2 | autocorr/parcorr/crosscorr (Econometrics ACF/PACF/CCF) |
| [signal/pmusic-peig](signal/pmusic-peig.md) | P2 | pmusic/peig |
| [signal/fillgaps](signal/fillgaps.md) | P2 | fillgaps |
| [signal/stmcb](signal/stmcb.md) | P2 | stmcb |
| [image/watershed](image/watershed.md) | P2 | watershed |
| [image/imfindcircles](image/imfindcircles.md) | P2 | imfindcircles |
| [image/corner](image/corner.md) | P2 | corner-point detection (cornermetric exists) |
| [wavelet/wpdec](wavelet/wpdec.md) | P2 | wavelet packets (needs tree type) |
| [wavelet/wentropy-ddencmp](wavelet/wentropy-ddencmp.md) | P2 | wentropy / ddencmp |
| [wavelet/wenergy-upcoef](wavelet/wenergy-upcoef.md) | P2 | wenergy (energy %) / upcoef (coeff reconstruction) |
| [wavelet/cwt](wavelet/cwt.md) | P2 | continuous wavelet transform (Morse filter bank) — large |
| [wavelet/wavedec2-family](wavelet/wavedec2-family.md) | P2 | wavedec2/detcoef2/appcoef2 (2-D multilevel) |
| [wavelet/centfrq-scal2frq](wavelet/centfrq-scal2frq.md) | P2 | centfrq / scal2frq (scale↔frequency) |
| [control/lqr-hinfnorm](control/lqr-hinfnorm.md) | P2 | lqr/hinfnorm/dlqr/gram |
| [control/care-dare](control/care-dare.md) | P2 | algebraic Riccati solvers (care/dare) |
| [control/minreal](control/minreal.md) | P2 | minimal realization (pole/zero cancellation) |
| [control/initial](control/initial.md) | P2 | initial-condition response |
| [control/allmargin](control/allmargin.md) | P2 | all gain/phase/delay margins struct |
| [control/covar](control/covar.md) | P2 | output covariance from white noise |
| [comm/analog-demodulators](comm/analog-demodulators.md) | P2 | am/fm/pm/ssb/msk demod |
| [comm/syndtable](comm/syndtable.md) | P2 | syndrome decoding table (coset leaders) |
| [math/numerical-integration-nd](math/numerical-integration-nd.md) | P2 | quadgk/integral2/integral3/quad2d |
| [ode/ode-stiff](ode/ode-stiff.md) | P2 | ode15s/ode23s/ode23t/ode23tb/ode113 (stiff/multistep) |
| [linalg/funm](linalg/funm.md) | P2 | general matrix function funm(A,fun) |
| [linalg/qz-gsvd](linalg/qz-gsvd.md) | P2 | qz (generalized Schur) / gsvd (generalized SVD) |
| [optim/fsolve](optim/fsolve.md) | P2 | nonlinear system solver fsolve |
| [optim/nonlinear-lsq](optim/nonlinear-lsq.md) | P2 | lsqcurvefit/lsqnonlin |
| [optim/constrained-solvers](optim/constrained-solvers.md) | P2 | fmincon/linprog/quadprog/fminunc |

### 🔴 OPEN — perf (correct but slower than MATLAB) — 1

| Entry | Slowdown | Notes |
|---|---|---|
| [signal/fft-speed](signal/fft-speed.md) | 1.2×–4.3× | single-threaded vs FFTW; Highway already present, gap is threading + MSVC codegen + wrapper |
