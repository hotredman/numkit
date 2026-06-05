# (cross-cutting) complex inputs unsupported across several functions

- **Status:** 🔴 OPEN
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
| `conv([1 1i],[1 1])` | Not a double array | `[1  1+1i  1i]` |
| `filter([1 1],1,[1i 1i])` | Not a double array | `[1i  2i]` |
| `trapz([1+1i 2+2i 3+3i])` | ✅ FIXED 2026-06-05 → `4+4i` | `4+4i` |
| `cumtrapz(...)` | ✅ FIXED 2026-06-05 → `[0 1.5+1.5i 4+4i]` | `[…  4+4i]` |
| `gradient([1+1i 3+3i 5+5i])` | ✅ FIXED 2026-06-05 → `[2+2i …]` | `[2+2i …]` |
| `movmean([1+1i 2+2i 3+3i],2)` | Not a double array | `[…]` |
| `detrend([1+1i 2+2i 3+3i])` | Not a double array | `~0` |
| `interp1([1 2 3],[1+1i 2+2i 3+3i],2.5)` | ✅ FIXED 2026-06-05 → `2.5+2.5i` | `2.5+2.5i` |
| `median([1+1i 2+2i 3+3i])` | ✅ FIXED 2026-06-05 → `2+2i` | `2+2i` (sort by abs, then angle) |

`diff` is worse (silently wrong, not an error) — tracked separately in
bugs/builtin/diff-complex.md. `cumsum`/`cumprod` and `norm` are the same
pattern — see bugs/builtin/cumsum-complex.md, bugs/linalg/norm-complex.md.
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
- conv/filter: `libs/signal/src/convolution/`, `.../digital_filtering/`
- trapz/cumtrapz/gradient/movmean/detrend/interp1/median: `libs/builtin/src/`,
  `libs/stats/src/` (movmean/detrend)
- MATLAB docs for each

## Progress (incremental — umbrella stays OPEN until all rows close)
- ✅ **trapz** — 2026-06-05 (bug-fix loop, cycle 7). Added a `ValueType::COMPLEX`
  branch in `trapzImpl` (`libs/builtin/src/math/integration/integration.cpp`):
  trapezoidal sum over `Complex` storage; the integration variable x stays real.
  All trapz paths (Y / X,Y / dim / matrix) route through `trapzImpl`, so one
  branch covers them. Live guard `libs/builtin/tests/trapz_complex_test.cpp`,
  parity `tools/parity/specs/trapz.json` (OK), smoke
  `libs/builtin/tests/smoke/trapz_complex_smoke.m`.
- ✅ **cumtrapz** — 2026-06-05 (bug-fix loop, cycle 8). Added Complex
  counterparts `cumtrapzVectorC`/`cumtrapzMatrixColsC`/`cumtrapzMatrixRowsC`
  and routed `cumtrapz`/`cumtrapzDim`/the 3-arg reg path to them when y is
  complex (x coordinate stays real). Removed the four "complex not supported"
  throws. Live guard `libs/builtin/tests/cumtrapz_complex_test.cpp`, parity
  `tools/parity/specs/cumtrapz.json` (OK), smoke
  `libs/builtin/tests/smoke/cumtrapz_complex_smoke.m`. (Also updated the stale
  CalculusTest.CumtrapzComplexThrows → CumtrapzComplexOk.)
- ✅ **median** — 2026-06-05 (bug-fix loop, cycle 9). Orders a complex slice by
  abs (ties by angle — the same comparator sort/max use); odd n → middle, even
  n → mean of the two middle. New helpers `complexMedianFromSlice` /
  `complexMedianAlongDim` / `medianComplex` in
  `libs/stats/src/descriptive/descriptive.cpp`, routed from both `median()` and
  the `median_reg` 'all' path (removed two throws). Live guard
  `libs/stats/tests/median_complex_test.cpp`, parity `tools/parity/specs/median.json`
  (OK), smoke `libs/stats/tests/smoke/median_complex_smoke.m`. (Also updated the
  stale ReductionDimTest.MedianComplexThrows → MedianComplexOk.)
- ✅ **interp1** — 2026-06-05 (bug-fix loop, cycle 10). At the top of
  `interp1Dispatch` (`libs/builtin/src/math/interp/interp.cpp`), a complex y is
  split into real + imag DOUBLE arrays, each interpolated by the existing real
  path (so EVERY method — linear/nearest/previous/next/spline/pchip/makima/
  cubic — and the matrix-column path are covered), then recombined; the
  extrapolation policy carries through (out-of-range → NaN+NaNi). Live guard
  `libs/builtin/tests/interp1_complex_test.cpp`, parity
  `tools/parity/specs/interp1.json` (OK), smoke
  `libs/builtin/tests/smoke/interp1_complex_smoke.m`.
- ✅ **gradient** (complex) — 2026-06-05 (bug-fix loop, cycle 11). gradient real
  + imaginary parts separately and recombine, for the vector + matrix
  single-output and 2-output forms (`gradient`/`gradient2` in
  `libs/builtin/src/math/integration/integration.cpp`). Live guard
  `libs/builtin/tests/gradient_complex_test.cpp`, parity
  `tools/parity/specs/gradient.json` (OK), smoke
  `libs/builtin/tests/smoke/gradient_complex_smoke.m`. (N-D `gradient` is still
  rank-limited — that's a separate bug, bugs/builtin/gradient-3d.md, still OPEN;
  a complex N-D input routes into the real path and errors the same way.)
- ⏳ Remaining: conv, filter, movmean, detrend.
