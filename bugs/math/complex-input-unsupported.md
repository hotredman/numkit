# (cross-cutting) complex inputs unsupported across several functions

- **Status:** ✅ FIXED (2026-06-05) — all nine members now accept complex
- **Severity:** P2 (errors where MATLAB returns a value)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (complex-input sweep)

## Symptom
A group of element-wise / filtering / interpolation functions reject complex
input — either "Not a double array" (they read `doubleData()` with no complex
branch) or an explicit "complex inputs are not supported" stub. MATLAB
accepts complex for all of them.

| Function | numkit | MATLAB |
|---|---|---|
| `conv([1 1i],[1 1])` | ✅ FIXED 2026-06-05 → `[1  1+1i  1i]` | `[1  1+1i  1i]` |
| `filter([1 1],1,[1i 1i])` | ✅ FIXED 2026-06-05 → `[1i 2i]` | `[1i  2i]` |
| `trapz([1+1i 2+2i 3+3i])` | ✅ FIXED 2026-06-05 → `4+4i` | `4+4i` |
| `cumtrapz(...)` | ✅ FIXED 2026-06-05 → `[0 1.5+1.5i 4+4i]` | `[…  4+4i]` |
| `gradient([1+1i 3+3i 5+5i])` | ✅ FIXED 2026-06-05 → `[2+2i …]` | `[2+2i …]` |
| `movmean([1+1i 2+2i 3+3i],2)` | ✅ FIXED 2026-06-05 | `[…]` |
| `detrend([1+1i 2+2i 3+3i])` | ✅ FIXED 2026-06-05 → `~0` | `~0` |
| `interp1([1 2 3],[1+1i 2+2i 3+3i],2.5)` | ✅ FIXED 2026-06-05 → `2.5+2.5i` | `2.5+2.5i` |
| `median([1+1i 2+2i 3+3i])` | ✅ FIXED 2026-06-05 → `2+2i` | `2+2i` (sort by abs, then angle) |

`diff` is worse (silently wrong, not an error) — tracked separately in
bugs/lang/diff-complex.md. `cumsum`/`cumprod` and `norm` are the same
pattern — see bugs/lang/cumsum-complex.md, bugs/linalg/norm-complex.md.
(`sum`/`prod`/`mean`/`var`/`sort`/`max`/`dot`/`conv2`? handle complex fine.)

## Root cause
Each function's kernel reads `x.doubleData()` (or asserts real) with no
`ValueType::COMPLEX` path; the `median` one bails out deliberately.

## Suggested fix
Add complex branches per function (each moderate; the pattern is identical to
the cumsum/norm fixes). For `median`, MATLAB's complex ordering is "sort by
`abs`, ties by `angle`" — same comparator `sort`/`max` already use. These can
be closed incrementally; this entry is the tracking umbrella.

## References
- conv/filter: `src/toolboxes/signal/src/convolution/`, `.../digital_filtering/`
- trapz/cumtrapz/gradient/interp1: `src/math/src/`; movmean/detrend/median:
  `src/toolboxes/stats/src/`
- MATLAB docs for each

## Progress (incremental — umbrella stays OPEN until all rows close)
- ✅ **trapz** — 2026-06-05 (bug-fix loop, cycle 7). Added a `ValueType::COMPLEX`
  branch in `trapzImpl` (`src/math/src/integration/integration.cpp`):
  trapezoidal sum over `Complex` storage; the integration variable x stays real.
  All trapz paths (Y / X,Y / dim / matrix) route through `trapzImpl`, so one
  branch covers them. Live guard `tests/builtin/trapz_complex_test.cpp`,
  parity `tools/parity/specs/trapz.json` (OK), smoke
  `tests/builtin/smoke/trapz_complex_smoke.m`.
- ✅ **cumtrapz** — 2026-06-05 (bug-fix loop, cycle 8). Added Complex
  counterparts `cumtrapzVectorC`/`cumtrapzMatrixColsC`/`cumtrapzMatrixRowsC`
  and routed `cumtrapz`/`cumtrapzDim`/the 3-arg reg path to them when y is
  complex (x coordinate stays real). Removed the four "complex not supported"
  throws. Live guard `tests/builtin/cumtrapz_complex_test.cpp`, parity
  `tools/parity/specs/cumtrapz.json` (OK), smoke
  `tests/builtin/smoke/cumtrapz_complex_smoke.m`. (Also updated the stale
  CalculusTest.CumtrapzComplexThrows → CumtrapzComplexOk.)
- ✅ **median** — 2026-06-05 (bug-fix loop, cycle 9). Orders a complex slice by
  abs (ties by angle — the same comparator sort/max use); odd n → middle, even
  n → mean of the two middle. New helpers `complexMedianFromSlice` /
  `complexMedianAlongDim` / `medianComplex` in
  `src/toolboxes/stats/src/descriptive/descriptive.cpp`, routed from both `median()` and
  the `median_reg` 'all' path (removed two throws). Live guard
  `src/toolboxes/stats/tests/median_complex_test.cpp`, parity `tools/parity/specs/median.json`
  (OK), smoke `src/toolboxes/stats/tests/smoke/median_complex_smoke.m`. (Also updated the
  stale ReductionDimTest.MedianComplexThrows → MedianComplexOk.)
- ✅ **interp1** — 2026-06-05 (bug-fix loop, cycle 10). At the top of
  `interp1Dispatch` (`src/math/src/interp/interp.cpp`), a complex y is
  split into real + imag DOUBLE arrays, each interpolated by the existing real
  path (so EVERY method — linear/nearest/previous/next/spline/pchip/makima/
  cubic — and the matrix-column path are covered), then recombined; the
  extrapolation policy carries through (out-of-range → NaN+NaNi). Live guard
  `tests/builtin/interp1_complex_test.cpp`, parity
  `tools/parity/specs/interp1.json` (OK), smoke
  `tests/builtin/smoke/interp1_complex_smoke.m`.
- ✅ **gradient** (complex) — 2026-06-05 (bug-fix loop, cycle 11). gradient real
  + imaginary parts separately and recombine, for the vector + matrix
  single-output and 2-output forms (`gradient`/`gradient2` in
  `src/math/src/integration/integration.cpp`). Live guard
  `tests/builtin/gradient_complex_test.cpp`, parity
  `tools/parity/specs/gradient.json` (OK), smoke
  `tests/builtin/smoke/gradient_complex_smoke.m`. (N-D `gradient` is still
  rank-limited — that's a separate bug, bugs/math/gradient-3d.md, still OPEN;
  a complex N-D input routes into the real path and errors the same way.)
- ✅ **movmean** — 2026-06-05 (bug-fix loop, cycle 12). Split-real/imag at the
  top of `movmean_impl` (`src/toolboxes/stats/src/moving/moving.cpp`): each part is
  moving-meaned by the existing real driver (window / asymmetric [kb kf] /
  Endpoints / dim all carry through), then recombined. Live guard
  `src/toolboxes/stats/tests/movmean_complex_test.cpp`, parity
  `tools/parity/specs/movmean.json` (OK), smoke
  `src/toolboxes/stats/tests/smoke/movmean_complex_smoke.m`. (omitnan with a
  partial-NaN complex element — one part finite, the other NaN — is a rare
  edge that the independent-part split handles approximately; full NaN+NaNi
  matches.)
- ✅ **detrend** — 2026-06-05 (bug-fix loop, cycle 13). Refactored
  `detrend_reg` (`src/toolboxes/stats/src/descriptive/descriptive_extras.cpp`) to parse
  order/breakpoints once, then dispatch through a `runReal` lambda; a complex
  input detrends the real + imaginary parts separately and recombines.
  ('constant' subtracts the complex mean; 'linear'/order-N and breakpoints all
  carry through.) Live guard `src/toolboxes/stats/tests/detrend_complex_test.cpp`,
  parity `tools/parity/specs/detrend.json` (OK), smoke
  `src/toolboxes/stats/tests/smoke/detrend_complex_smoke.m`.
- ✅ **conv** — 2026-06-05 (bug-fix loop, cycle 14). conv is BILINEAR (it
  multiplies the two sequences), so the real/imag split does NOT apply — a
  genuine complex multiply-accumulate `full[n] = sum_k a[k]·b[n-k]` (direct;
  correctness over an FFT path), then the same 'full'/'same'/'valid' trim.
  Handles complex×complex and complex×real
  (`src/toolboxes/signal/src/convolution/convolution.cpp`). Live guard
  `src/toolboxes/signal/tests/conv_complex_test.cpp`, parity `tools/parity/specs/conv.json`
  (OK), smoke `src/toolboxes/signal/tests/smoke/conv_complex_smoke.m`.
- ✅ **filter** — 2026-06-05 (bug-fix loop, cycle 15). filter is BILINEAR (the
  recursive a-part mixes terms), so the Direct Form II transposed recurrence
  runs over Complex (NOT a split): a complex `applyFilterDf2tComplex` core +
  complex branches in both `filter()` and the `filter_reg` zi/[y,zf] path
  (`src/toolboxes/signal/src/digital_filtering/filter.cpp`). Covers FIR/IIR, complex b/a
  taps, complex x, zi initial conditions, the [y,zf] final state, and the
  matrix-per-column form. Live guard `src/toolboxes/signal/tests/filter_complex_test.cpp`,
  parity `tools/parity/specs/filter.json` (OK), smoke
  `src/toolboxes/signal/tests/smoke/filter_complex_smoke.m`.

## ✅ Umbrella fully closed (2026-06-05)
All nine members accept complex now — trapz, cumtrapz, median, interp1,
gradient, movmean, detrend, conv, filter — each with its own parity spec,
live gtest and smoke. The linear ops use a real/imag split; conv/filter use a
genuine complex multiply-accumulate (they are bilinear). The umbrella DISABLED
guard was retired (every member has its own live guard). Related complex gaps
remain tracked separately: N-D gradient (gradient-3d.md), complex MATRIX linear
algebra (linalg/complex-matrix-unsupported.md), and the sqrt/acosh/atanh array
promotion (complex-promotion-arrays.md, also FIXED).
