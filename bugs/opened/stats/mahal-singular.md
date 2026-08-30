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

## Suggested fix
Switch `mahal` to the QR-based formulation (center X, `[Q,R]=qr(Xc,0)`,
solve `R'\ (Yc')` per row, sum of squares × (n-1)) — matches MATLAB for
FULL-RANK X and stops throwing.

## ⚠️ Caveat — rank-deficient value is a FP artifact (probed 2026-06-03, c181)
The collinear reference `0.9505075439` is NOT a well-defined mathematical
value: for rank-deficient X the Mahalanobis distance is infinite in the
null direction. MATLAB returns a finite number only because `qr` leaves a
tiny non-zero residual on the diagonal (`R(2,2) = -4.29851e-16`,
`RCOND ≈ 1.9e-16`) and `R'\b` divides a tiny numerator by it — MATLAB
itself warns *"Matrix is close to singular ... Results may be inaccurate."*
Reproducing `0.9505075439` to 1e-5 requires numkit's QR to emit the SAME
~1e-16 residual, which is QR-implementation-dependent and fragile.
**Decision:** deferred — switching to QR is safe and removes the throw, but
the specific rank-deficient value can't be robustly matched. Revisit only if
a user needs the finite (unstable) value rather than just "doesn't throw".
DISABLED guard `DISABLED_MahalSingular` stays.

## References
- `src/toolboxes/stats/src/.../mahal*`
- shipped: `qr`, `cov`
- MATLAB `doc mahal`
