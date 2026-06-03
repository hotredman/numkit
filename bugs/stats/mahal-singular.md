# stats.mahal — throws on rank-deficient reference (MATLAB handles it)

- **Status:** 🔴 OPEN
- **Severity:** P2 (over-strict — errors where MATLAB returns a value)
- **Kind:** bug
- **Found:** 2026-06 via DEEP-PROBE (stats coverage)

## Symptom
`mahal(Y, X)` throws "covariance matrix is not positive definite" when the
reference `X` is rank-deficient (collinear); MATLAB still returns distances.
Full-rank `X` works and matches MATLAB exactly.

## Repro
```matlab
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

## Suggested fix
Switch `mahal` to the QR-based formulation (center X, `[Q,R]=qr(Xc,0)`,
solve `R'\ (Yc')` per row, sum of squares × (n-1)) — matches MATLAB and is
defined for rank-deficient X. Moderate. Validate both full-rank and
collinear cases vs MATLAB.

## References
- `libs/stats/src/.../mahal*`
- shipped: `qr`, `cov`
- MATLAB `doc mahal`
