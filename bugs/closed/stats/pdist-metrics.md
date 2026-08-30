# stats.pdist — 'seuclidean'/'spearman' metrics missing + cosine zero-vector

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (missing metrics) + P3 (cosine edge)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (option-value sweep)

## Symptom
1. **Missing metrics** — `pdist(X, 'seuclidean')` (standardized Euclidean)
   and `pdist(X, 'spearman')` (Spearman rank correlation distance) throw
   "unknown metric". MATLAB ships both. (euclidean, squaredeuclidean,
   cityblock, chebychev, minkowski, cosine, correlation, hamming, jaccard
   all work.)
2. **Cosine zero-vector** — `pdist` with `'cosine'` and a zero-norm row
   returns 1 instead of NaN. The cosine distance `1 - a·b/(‖a‖‖b‖)` is
   undefined when a norm is 0; MATLAB returns NaN.

## Repro
```matlab
A = [1 2 3; 4 5 7; 1 0 2];
pdist(A, 'seuclidean')
% numkit: Error — pdist: unknown metric 'seuclidean'
% MATLAB: [2.5897 0.8800 3.2433]
pdist(A, 'spearman')
% numkit: Error — pdist: unknown metric 'spearman'
% MATLAB: [0 ...] (rank-correlation distances)

pdist([0 0; 3 4], 'cosine')
% numkit: 1
% MATLAB: NaN   (zero-norm row -> undefined)
```

## Root cause
`src/toolboxes/stats/src/cluster/distance.cpp`: the metric dispatch doesn't implement
'seuclidean' (per-column variance scaling) or 'spearman' (tiedrank rows, then
correlation distance). For 'cosine', a zero denominator is collapsed to 1
instead of propagating NaN.

## Suggested fix
- 'seuclidean': scale each coordinate by `1/std(X(:,j))` (sample std over the
  rows) then euclidean. Optional explicit scale vector arg.
- 'spearman': `tiedrank` each column-wise observation, then the correlation
  distance on the ranks.
- 'cosine': when either row norm is 0, emit NaN (don't clamp to 1).
Moderate (two metrics + the edge). pdist already has `tiedrank` and `corr`
machinery in src/toolboxes/stats to lean on.

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 17), `src/toolboxes/stats/src/cluster/distance.cpp`.
- **'seuclidean'** added to both `pdist` and `pdist2`: distance
  `√Σ((xₖ−yₖ)/Sₖ)²` where the default scale `Sₖ` is the per-column **sample
  std** (n−1) of the data set (`std(X)` — the *first* arg for `pdist2`,
  matching MATLAB). An explicit scale vector is supported via the existing
  `C_opt` slot (`pdist(X,'seuclidean',[1 2 3])`).
- **'spearman'** added to both: each row is `tiedrank`-transformed (average
  ranks for ties) and the **correlation distance** is taken on the ranks. A
  constant rank-row (zero variance) yields NaN, as MATLAB does.
- **'cosine'** and **'correlation'** now return **NaN** (not 1) when a row has
  zero norm / zero variance. The md only named cosine, but MATLAB returns NaN
  for `'correlation'` on a constant row too (verified R2025b) — both fixed and
  tested.
- Verified vs MATLAB R2025b: `pdist(A,'seuclidean')=[2.58974 0.88002 3.24327]`,
  explicit-scale `[3.60940 …]`, `pdist(A,'spearman')=[~0 0.5 0.5]`, ties
  `pdist([1 1 2;3 2 2],'spearman')=1.5`, `isnan(pdist([0 0;3 4],'cosine'))`,
  `isnan(pdist([1 1;3 4],'correlation'))`, `pdist2(…,'seuclidean')(1,1)=1.00692`,
  `pdist2(…,'spearman')` NaN column.
- Live guard: `src/toolboxes/stats/tests/pdist_metrics_test.cpp` (8 TEST_F) + the
  catalog guard `StatsKnownBug.PdistMetrics` (flipped from DISABLED). Parity:
  `tools/parity/specs/pdist_metrics.json` (correctness=OK). Smoke:
  `src/toolboxes/stats/tests/smoke/pdist_metrics_smoke.m`.

## References
- `src/toolboxes/stats/src/cluster/distance.cpp` (pdist metric dispatch)
- shipped: `tiedrank`, `corr`, `std`
- MATLAB `doc pdist`
