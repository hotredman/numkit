# stats.smoothdata — 'sgolay' / 'lowess' / 'loess' methods throw

- **Status:** ✅ FIXED (2026-06-18) — sgolay + lowess + loess all implemented
- **Severity:** P2 (missing option)
- **Kind:** stub
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`smoothdata` supports `movmean`/`movmedian`/`gaussian` but throws on the
regression-based methods `sgolay`, `lowess`, `loess`.

## Repro
```matlab
smoothdata([1 5 2 8 3 9 4], 'sgolay')
% numkit: Error — smoothdata: method 'sgolay' not supported
%         (supported: 'movmean', 'movmedian', 'gaussian')
smoothdata([1 5 2 8 3 9 4], 'lowess')
% numkit: Error — smoothdata: method 'lowess' not supported
```

## Root cause
The method dispatch in `smoothdata` only wires three methods.

## Fix (2026-06-18) — sgolay
`smoothdata(x, 'sgolay', window)` now works (degree-2 Savitzky-Golay). Because
`smoothdata` lives in **stats** and `sgolayfilt` in **signal** (peer toolboxes,
no cross-dep), the SG filter is implemented **inline** in
`src/toolboxes/stats/src/moving/moving.cpp`: build the projection matrix
`B = A·(A'A)⁻¹·A'` (Vandermonde `A` of `[-m..m]`, degree 2), apply the centre row
to interior samples and the boundary rows to the first/last `m` samples — the
exact behaviour of MATLAB `sgolay`/`sgolayfilt` (verified by reconstructing it in
MATLAB before porting). Works on vectors + matrices along `dim`; short slices
fall back to passthrough.

**Explicit odd window matches MATLAB exactly** (parity `smoothdata_sgolay.json`
→ OK): `smoothdata([1 5 2 8 3 9 4 7 2 6 1 8],'sgolay',5)` =
`[1.1143 3.7429 5.0857 4.4 …]`, `w=7` = `[1.3333 … 6.4762]`. Guard:
`known_bugs_test.cpp` (`SmoothdataSgolay`, promoted live); smoke
`smoothdata_sgolay_smoke.m`.

⚠️ The **auto default window is approximate**, NOT matched: MATLAB's default is a
**data-dependent heuristic** (probed: for the same `N=12` the default window was
2 or 6 depending on the data values), which numkit's shared `round(0.1·N)` default
doesn't replicate — the explicit-window form is the matched contract. (The
existing `movmean`/`movmedian`/`gaussian` defaults share this approximation.)

## Fix (2026-06-18) — lowess / loess
`'lowess'` (local linear) and `'loess'` (local quadratic) now work too, inline in
the same file (`localRegressSlice`/`smoothLocalRegDim`). For each sample: take the
`F` nearest points (window shifted to stay in range), tricube-weight by the
in-window distance (the window's two outermost samples get weight 0), fit a
degree-1/2 polynomial by weighted least squares, and evaluate it at the query
point. Reconstructed + confirmed against MATLAB before porting. The `loess`
interior-identity quirk is explained: tricube zeroes the window edges, leaving 3
non-zero-weight points, and a quadratic through 3 points interpolates the centre
→ the interior sample is returned unchanged.

Explicit window matches MATLAB R2025b exactly (parity `smoothdata_lowess.json`
→ OK): lowess w5 = `[1.7113 3.0349 4.5768 …]`, loess w5 =
`[1.5509 2.9192 2 8 …]`. Guard: `known_bugs_test.cpp` (`SmoothdataLowessLoess`);
smoke extended. (Same data-dependent default-window approximation as sgolay.)

## References
- `src/toolboxes/stats/src/moving/moving.cpp` (`buildSGMatrix`, `smoothSGDim`,
  `smoothdata` dispatch).
- `tools/parity/specs/smoothdata_sgolay.json`.
- MATLAB `doc smoothdata`
