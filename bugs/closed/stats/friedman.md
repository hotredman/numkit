# stats.friedman — function missing

- **Status:** ✅ FIXED (2026-06-18) — reps=1 implemented; reps>1 deferred
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

## Fix (2026-06-18)
Implemented `numkit::stats::friedman(x, reps)` in
`src/toolboxes/stats/src/anova/anova.cpp`, registered under `anova`. Ranks the
`k` treatments within each of the `n` blocks (mid-ranks for ties), forms the
tie-corrected statistic `Q = [12/(n·k·(k+1))·Σ Rⱼ² − 3·n·(k+1)] / C`,
`C = 1 − Σ(t³−t)/(n·(k³−k))`, `p = 1 − chi2cdf(Q, k−1)`. Tie correction probed +
confirmed on a ties case.

Returns **`[p, Q, df]`** — the statistic + df, NOT MATLAB's display
`(tbl, stats)` — consistent with how `kruskalwallis` is shaped here; the primary
`p` matches MATLAB exactly.

Verified vs MATLAB R2025b (parity `friedman.json` → OK):
`friedman([1 2 3;2 3 4;3 4 5;1 3 5],1)=0.0183156` (Q=8, no ties),
`friedman([7 9 8;6 5 7;9 7 6;8 8 9;5 6 5],1)=0.8464817` (ties). Guards:
`friedman_test.cpp`; smoke `friedman_smoke.m`.

**Deferred — `reps > 1`.** A replicated two-way layout's ranking does NOT reduce
to averaging-then-rank (probed: numkit's averaging gave 0.7165 vs MATLAB's
0.4869 for a reps=2 case), so `reps > 1` is **rejected with a clear error**
rather than returning a wrong p. Reverse-engineering MATLAB's replicated-layout
ranking is a separate change.

## References
- `src/toolboxes/stats/src/anova/anova.cpp` (`friedman`),
  `.../include/numkit/stats/anova/anova.hpp`,
  `src/bundle/src/register/stats/anova/anova_reg.cpp` (`friedman_reg`, reps gate).
- `tools/parity/specs/friedman.json`.
- shipped: `tiedrank`, `chi2cdf`, `kruskalwallis`, `anova1`
- MATLAB `doc friedman`
