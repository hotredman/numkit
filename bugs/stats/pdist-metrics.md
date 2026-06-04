# stats.pdist — 'seuclidean'/'spearman' metrics missing + cosine zero-vector

- **Status:** 🔴 OPEN
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
`libs/stats/src/cluster/distance.cpp`: the metric dispatch doesn't implement
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
machinery in libs/stats to lean on.

## References
- `libs/stats/src/cluster/distance.cpp` (pdist metric dispatch)
- shipped: `tiedrank`, `corr`, `std`
- MATLAB `doc pdist`
