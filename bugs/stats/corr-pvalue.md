# stats.corr — missing 2nd output (p-value) for all correlation types

- **Status:** 🔴 OPEN
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

## References
- `libs/stats/src/.../corr*`
- shipped: `tcdf`, `tiedrank`
- MATLAB `doc corr`
