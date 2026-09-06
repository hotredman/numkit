# stats.mahal — throws on rank-deficient reference (MATLAB handles it)

- **Status:** ✅ FIXED (e71a5e817, 2026-09-06)
- **Severity:** P2 (over-strict — errors where MATLAB returns a value)
- **Kind:** bug
- **Found:** 2026-06 via DEEP-PROBE (stats coverage)

## Symptom
`mahal(Y, X)` throws "covariance matrix is not positive definite" when the
reference `X` is rank-deficient (collinear); MATLAB still returns distances.
Full-rank `X` works and matches MATLAB exactly.

## Repro
```matlab
clear;
mahal([1 1; 2 2], [0 0; 1 1; 2 2; 3 3])   % X collinear (rank 1)
% numkit: Error — mahal: covariance matrix is not positive definite
% MATLAB: [0.9505075; 0.9505075]
mahal([1 1; 2 2], [0 0; 1 0; 0 1; 2 2; 1 3])   % full rank: numkit == MATLAB
%   -> [0.157746; 2.07324]
```

## Root cause
numkit forms the reference covariance and requires a Cholesky factor
(positive-definite); rank-deficient `X` has a singular covariance → throw.
MATLAB's `mahal` instead QR-factorises the centered reference and solves a
least-squares system, which is defined even when `X` is rank-deficient.

## Fix (2026-09-06)

mahal.m's QR formulation implemented in
`src/toolboxes/stats/src/cluster/distance.cpp`: center X, local
Householder QR (with the dlarfg tail rule), thresholded forward solve
Rᵀz = y−μ, d² = (n−1)·|z|². The throw is gone; FULL-RANK values match
MATLAB bit-comparably (probed 0.157746 / 2.073239 exact).

## ⚠️ Caveat — rank-deficient value is a FP artifact (probed 2026-06-03, reconfirmed 2026-09-06)
The collinear reference `0.9505075439` is NOT a well-defined mathematical
value: for rank-deficient X the Mahalanobis distance is infinite in the
null direction. MATLAB returns a finite number only because `qr` leaves a
tiny non-zero residual on the diagonal (`R(2,2) = -4.29851e-16`,
`RCOND ≈ 1.9e-16`) and `R'\b` divides a tiny numerator by it — MATLAB
itself warns *"Matrix is close to singular ... Results may be inaccurate."*
numkit THRESHOLDS the dust diagonal (z_i = 0 in null directions) — a
stable variant of the same math; its rank-deficient values are finite
but differ from MATLAB's dust-amplified ones, by nature unmatchable
without a bit-identical LAPACK.

## References
- `src/toolboxes/stats/src/cluster/distance.cpp` (mahal)
- Guards: `StatsKnownBug.MahalSingularNoThrow` (live) +
  `MahalTest.RankDeficientNoThrow` (finite + full-rank exactness)
- shipped: `qr`, `cov`
- MATLAB `doc mahal`
