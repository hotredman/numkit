# linalg.schur — throws on a non-symmetric matrix (real Schur form deferred)

- **Status:** ✅ FIXED (2026-06-18) — Francis double-shift QR real Schur
- **Severity:** P2 (errors where MATLAB returns a value)
- **Kind:** stub
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
`schur(A)` for a **non-symmetric** `A` throws
"eig: only symmetric matrices supported in this revision (general eig via
Hessenberg + Francis QR is deferred to Phase 2b)". MATLAB returns the real
Schur form `[U,T]` (orthogonal `U`, quasi-upper-triangular `T`).

Notably, **eigenVALUES of a non-symmetric matrix already work**
(`eig([2 1;0 3])` → `[3 2]`), so the eigenvalue iteration exists — it is the
orthogonal-factor accumulation (the Schur *form*) that is gated behind the
symmetric-only `schur` path.

## Repro
```matlab
[U, T] = schur([2 1; 0 3]);
% MATLAB: T upper-triangular, diag(T) = [2 3], U orthogonal, U*T*U' = A
% numkit: Error — "only symmetric matrices supported ... deferred to Phase 2b"
eig([2 1; 0 3])   % numkit == MATLAB == [3 2]  (eigenVALUES already work)
```

## Root cause
The `schur` implementation only covers the symmetric (Jacobi/tridiagonal)
case and explicitly defers the general real Schur (Hessenberg + Francis
double-shift QR with `U` accumulation). The eigenvalue-only path is wired
elsewhere, creating the asymmetry above.

## Fix (2026-06-18)
Implemented `numkit::linalg::schur_general` in `eig.cpp` on top of the existing
`hessReduceInplace` (Householder Hessenberg): **Francis double-shift QR**
(`francisSchur`) chases bulges down the active block, deflates 1×1/2×2 blocks,
and accumulates every reflector into `U` (seeded with the Hessenberg transform).
A final `standardizeSchur2x2` pass (LAPACK `dlanv2` logic) triangularizes 2×2
blocks with REAL eigenvalues and leaves complex-conjugate pairs as standardized
2×2 blocks — also covering the `n==2` input. `schur_reg` now dispatches by
`isSymmetricApprox`: symmetric → `schur_sym` (Jacobi), else → `schur_general`.

The Schur form is not unique, so validation is on the invariants: `A == U·T·Uᵀ`,
`U` orthogonal, `diag`/blocks of `T` = eigenvalues, and upper-triangular `T` for
real eigenvalues. Verified vs MATLAB R2025b (parity `schur_general.json` → OK):
`[1 2;3 4]` → triangular, diag `[-0.37228, 5.37228]`; `[1 2 3;4 5 6;7 8 10]` →
eig `[-0.9057, 0.1981, 16.7077]`, reconstruction/orthogonality ~1e-14; a 4×4
with a complex pair keeps its 2×2 block + reconstructs. Guards:
`matfunc_test.cpp` (`SchurNonsymmetric`), `known_bugs_test.cpp`
(`SchurNonsymmetric`, promoted live); smoke `schur_general_smoke.m`.

This is the kernel `care`/`dare` + general `eig`-vectors / `qz` need
(bugs/control/care-dare, linalg/qz-gsvd) — those can now build on `schur_general`.

## References
- `src/toolboxes/linalg/src/eig.cpp` (`francisSchur`, `standardizeSchur2x2`,
  `schur_general`), `.../include/numkit/linalg/eig.hpp`,
  `src/bundle/src/register/linalg/eig_reg.cpp` (`schur_reg` dispatch).
- `tools/parity/specs/schur_general.json`.
- related: linalg/qz-gsvd.md, control/care-dare.md (same Schur machinery)
- MATLAB `doc schur`
