# stats.friedman — function missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`friedman` (Friedman's nonparametric two-way ANOVA by ranks) is not
registered.

## Repro
```matlab
p = friedman([1 2 3; 2 3 4; 3 4 5; 1 3 5], 1)
% numkit: Error — VM: undefined function 'friedman'
% MATLAB: p = 0.018315639
```

## Root cause
Not implemented.

## Suggested fix
Friedman's test: rank within each row (block) across the `k` columns
(treatments), sum ranks per column `R_j`, compute the statistic
`Q = 12/(n·k·(k+1))·Σ R_j² − 3·n·(k+1)` (with tie correction), p-value from
`chi2cdf(Q, k−1)` (MATLAB also returns the ANOVA table + stats struct).
`reps` (>1 replicates per cell) averages within cells first. Reuses
`tiedrank` + `chi2cdf` (both present). Moderate. Validate the statistic +
p-value (and the table) vs MATLAB; pairs with `kruskalwallis` (already
shipped).

## References
- new file under `src/toolboxes/stats/src/...`
- shipped: `tiedrank`, `chi2cdf`, `kruskalwallis`, `anova1`
- MATLAB `doc friedman`
