# linalg.qz / gsvd — generalized decompositions missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
The two generalized matrix decompositions are not registered: `qz` (the
generalized Schur decomposition of a matrix pair `(A,B)`) and `gsvd` (the
generalized singular value decomposition). numkit has `eig`/`svd`/`qr` for
the ordinary cases (and `eig(A,B)` generalized eigenvalues), but not these
factorizations.

## Repro
```matlab
[AA,BB,Q,Z] = qz([1 2;3 4], [1 0;0 1]);   % Q*A*Z = AA, Q*B*Z = BB
% MATLAB: 2x2 factors; numkit: Error — VM: undefined function 'qz'
S = gsvd([1 2;3 4], [1 0;0 1]);
% MATLAB: generalized singular values [0.365966  5.46499] (ascending)
% numkit: Error — VM: undefined function 'gsvd'
```

## Root cause
Not implemented. `qz` needs the generalized real Schur (Hessenberg-triangular
reduction + the QZ step) — related to the deferred general real Schur
(see linalg/schur-nonsymmetric.md). `gsvd` is typically built on `qz` or a
pair of QR + CS decompositions.

## Suggested fix
- `qz(A,B)`: reduce `(A,B)` to Hessenberg-triangular, run the QZ iteration to
  (quasi-)triangular `(AA,BB)`, accumulate `Q,Z`. Large; couples to the
  general-Schur work.
- `gsvd(A,B)`: via the CS decomposition of `qr([A;B])`, or LAPACK `*ggsvd`
  if available. Large.
Defer unless requested. Verify reconstruction `Q*A*Z==AA` and the
generalized singular values vs MATLAB when implementing.

## References
- new file(s) under `libs/linalg/src/...`; cf. `eig(A,B)`/`svd`/`qr`
- MATLAB `doc qz`, `doc gsvd`
