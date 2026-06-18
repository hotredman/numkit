# stats.smoothdata — 'sgolay' / 'lowess' / 'loess' methods throw

- **Status:** 🔴 OPEN (lowess/loess) — `sgolay` ✅ FIXED (2026-06-18)
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

## Still open — lowess / loess
`'lowess'` (local linear) and `'loess'` (local quadratic) regression smoothers
still throw. They need a tricube-weighted moving local-regression; MATLAB's
`loess` also shows an interior-identity quirk on short windows that needs
understanding before a parity match. Separate change.

## References
- `src/toolboxes/stats/src/moving/moving.cpp` (`buildSGMatrix`, `smoothSGDim`,
  `smoothdata` dispatch).
- `tools/parity/specs/smoothdata_sgolay.json`.
- MATLAB `doc smoothdata`
