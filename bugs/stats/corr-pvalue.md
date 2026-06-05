# stats.corr — missing 2nd output (p-value) for all correlation types

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (missing output)
- **Kind:** missing-output
- **Found:** 2026-06 via DEEP-PROBE (stats coverage)

## Symptom
`[r, p] = corr(x, y, ...)` throws "Too many output arguments" — numkit
returns only the correlation `r`; the p-value `p` (test of H0: no
correlation) is missing for Pearson, Spearman, AND Kendall.

## Repro
```matlab
x = [1 2 3 4 5]'; y = [2 1 4 3 6]';
[r, p] = corr(x, y, 'type', 'Kendall')   % numkit: Error — Too many output args
% MATLAB: r = 0.6,       p = 0.233333
[r, p] = corr(x, y, 'type', 'Spearman')  % MATLAB: r = 0.8,   p = 0.133333
[r, p] = corr(x, y)                       % MATLAB: r = 0.821995, p = 0.0877066
```
The `r` values are all correct (1-output form works).

## Root cause
`corr_reg` emits only `outs[0]`; the p-value isn't computed.

## Suggested fix
- **Pearson**: `t = r·sqrt((n-2)/(1-r²))`, `p = 2·tcdf(-|t|, n-2)` — simple.
- **Spearman / Kendall**: MATLAB uses the EXACT permutation distribution for
  small n and an asymptotic approximation for large n — the small-n exact
  part is the fiddly bit (moderate). So this isn't a one-liner overall: the
  Pearson p is trivial but matching MATLAB's Spearman/Kendall p needs the
  exact small-n tables/approximation. Thread `nargout` and emit `p` for all
  three; validate vs MATLAB across n.

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 28),
  `libs/stats/src/descriptive/descriptive_extras.cpp` (`corr_reg`).
- `corr_reg` is now `nargout`-aware. For `nargout >= 2` it builds a p-value
  matrix the same shape as `r` (element-wise from `(r_ij, n, type)`), and for
  the auto-correlation form `corr(X)` the diagonal is forced to 1 (matching
  MATLAB). New helpers added next to `corrDispatch`:
  - **Pearson** — `corrPearsonP`: `t = r·sqrt((n-2)/(1-r²))`,
    `p = 2·tcdf(-|t|, n-2)`. Exact for all n. `|r|>=1 → p=0`, `n<3 → NaN`.
  - **Kendall** — `corrKendallExactP`: the EXACT permutation (Mahonian
    inversions) distribution via the generating function
    `∏_{k=1}^{n-1}(1+q+…+q^k)/(k+1)` evaluated by DP; two-sided
    `p = 2·min(P(D≤Dobs), P(D≥Dobs))` where `Dobs = round(Dmax·(1-tau)/2)`.
    No-ties up to n=100, normal approx (`erfc(|z|/√2)`) beyond.
  - **Spearman** — `corrSpearmanExactP`: EXACT permutation enumeration for
    n≤10 (`Dobs = round((1-rho)·n(n²-1)/6)`), t-approximation
    `2·tcdf(-|t|, n-2)` for n>10.
- Verified vs MATLAB R2025b: Pearson `p=0.087706647` (n=5); Spearman
  `p=0.133333333` (n=5), `p=0.012301587` (n=7); Kendall `p=0.233333333`
  (n=5), `p=0.014136905` (n=8); matrix `corr(X)` diagonal p = 1,
  off-diagonal = the pairwise p. `r` and the 1-output form are unchanged.
- **Known divergence (documented):** for large-n Spearman, MATLAB uses the
  AS 89 algorithm; numkit uses the Student-t approximation. The two agree to
  ~3 digits but are not identical, so only the small-n EXACT Spearman case is
  validated in parity. Ties in Kendall fall back to the normal approximation.
- Live guard: `libs/stats/tests/corr_pvalue_test.cpp` (5 TEST_F) + flipped
  `StatsKnownBug.CorrPValue` live. Parity: `tools/parity/specs/corr.json`
  extended with 7 p-value fingerprints (correctness=OK). Smoke:
  `libs/stats/tests/smoke/corr_pvalue_smoke.m`.

## References
- `libs/stats/src/descriptive/descriptive_extras.cpp` (`corr_reg`)
- shipped: `tcdf`, `tiedrank`
- MATLAB `doc corr`
