# linalg.schur — throws on a non-symmetric matrix (real Schur form deferred)

- **Status:** 🔴 OPEN
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

## Suggested fix
Wire `schur` to the general real Schur: reduce to upper Hessenberg, run the
Francis double-shift QR accumulating the orthogonal transformations into
`U`, leaving 1×1 / 2×2 blocks on the diagonal of `T`. This is the same
kernel `qz`/general `eig`-vectors need (see linalg/qz-gsvd.md). Medium-large
(couples to the deferred Phase-2b work). Verify `U*T*U'==A`, `U` orthogonal,
`diag(T)` = eigenvalues vs MATLAB.

## References
- `libs/linalg/src/...` (schur kernel; symmetric-only branch)
- related: linalg/qz-gsvd.md (same Hessenberg/QZ machinery)
- MATLAB `doc schur`
