# linalg.funm — general matrix function missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
`funm(A, fun)` — evaluate an arbitrary scalar function `fun` *of a matrix*
(not element-wise) — is not registered. numkit ships the specialised matrix
functions `expm`/`sqrtm`/`logm`, but not the general `funm` that `expm`
et al. are special cases of.

## Repro
```matlab
funm([2 0; 0 3], @exp)
% MATLAB: [7.38906 0; 0 20.0855]   (= diag(e^2, e^3))
% numkit: Error — VM: undefined function 'funm'
```

## Root cause
Not implemented. The Schur-Parlett machinery needed by `funm` is partly
present (numkit has Schur decomposition + `expm`/`logm`/`sqrtm`), but the
general dispatcher that applies a user `FnHandle` on the Schur form (with
the Parlett recurrence for the off-diagonal blocks) is not wired.

## Suggested fix
Implement the Schur-Parlett algorithm: `A = Q T Q'`, apply `fun` to the
(clustered) diagonal blocks, fill super-diagonals via the Parlett
recurrence, then `F = Q F_T Q'`. `fun` is a `FnHandle` evaluated on scalars
(and, for repeated eigenvalues, on derivatives — or fall back to a block
Taylor series). Medium-large. Verify on a diagonal matrix (closed form)
and a non-normal 2×2 vs MATLAB.

## References
- new file under `toolboxes/linalg/src/...` (reuse Schur + the expm block code)
- MATLAB `doc funm`
