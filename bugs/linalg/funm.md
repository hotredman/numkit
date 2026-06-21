# linalg.funm — general matrix function missing

- **Status:** ✅ FIXED (2026-06-19) — eigendecomposition path (embedded .m)
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

## Fix (2026-06-19)
Implemented via **eigendecomposition** rather than full Schur-Parlett —
`F = V * diag(fun(diag(D))) / V` where `[V, D] = eig(A)`, with `real(F)`
forced when `A` is real. Done as an embedded `.m` builtin
(`kFunmMSource` in `linalg_library.cpp`, registered via
`engine.registerBuiltinMSource`), reusing the existing `eig` builtin
(which already returns `[V, D]` for general matrices). This matches MATLAB
funm **exactly** for diagonalizable matrices with real eigenvalues:

```matlab
funm([2 0; 0 3], @exp)  % diag(7.38906, 20.0855)         ✓ exact
funm([1 2; 3 4], @exp)  % F(1,1)=51.96895620, ...        ✓ exact (= expm)
funm([1 2; 3 4], @sin)  % F(1,1)=-0.46558149             ✓ exact
funm([2 1; 1 2], @sqrt) % == sqrtm([2 1; 1 2])           ✓ exact
```

Note: MATLAB's own `funm` calls `feval(fun, x, k)` with a derivative-order
`k` on its generic path, so `funm(A, @sqrt)` and anonymous funcs *error* in
MATLAB — numkit's element-wise-on-eigenvalues form is strictly more lenient
there.

### Deferred branch (documented limitation)
**Complex-eigenvalue** and **defective** (repeated-eigenvalue, non-diagonalizable)
matrices error: numkit's `eig` `[V, D]` form needs Francis QR iteration for
the complex case (separate deferred core limitation — see
`bugs/linalg/complex-eig*`). MATLAB falls back to Schur-Parlett there
(`funm([4 1; 0 4], @exp) = [54.5982 54.5982; 0 54.5982]`); numkit currently
raises `eig: [V, D] form ... requires Francis QR iteration`. The full
Schur-Parlett rewrite (Parlett recurrence on the off-diagonal blocks) is the
follow-up that would close this branch.

## References
- `src/bundle/src/register/linalg/linalg_library.cpp` (`kFunmMSource` +
  registration in `LinalgLibrary::install`)
- `tools/parity/specs/funm.json`, `src/toolboxes/linalg/tests/funm_test.cpp`,
  `src/toolboxes/linalg/tests/known_bugs_test.cpp` (`Funm`, promoted live),
  smoke `src/toolboxes/linalg/tests/smoke/funm_smoke.m`
- reused: the `eig` builtin (`[V, D]` for general matrices)
- MATLAB `doc funm`
