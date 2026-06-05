# linalg.eig — 3rd output W (left eigenvectors) unsupported

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (missing output)
- **Kind:** missing-output
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`[V,D,W] = eig(A)` throws "Too many output arguments". numkit returns only
`[V,D]` (right eigenvectors + eigenvalues); the left eigenvectors `W`
(satisfying `W'*A = D*W'`) are missing.

## Repro
```matlab
[V,D,W] = eig([2 1; 1 3]);
% numkit: Error — Too many output arguments
% MATLAB: W'*A - D*W' ≈ 0  (left eigenvectors, normalized to unit length)
[V,D,W] = eig([4 -2; 1 1]);
% MATLAB: D = diag([3 2]),  W(1,1) = 0.707107
```

## Root cause
The `eig` adapter (`libs/linalg/src/eig.cpp`) emits only `outs[0..1]`.

## Suggested fix
Left eigenvectors are the right eigenvectors of `A'` (conjugated). Compute
`eig(A')`, then **reorder** them to match the eigenvalue order of `D` and
**normalize** (MATLAB: unit 2-norm, and a sign/phase convention). The
reorder + normalization to match MATLAB exactly is the fiddly part —
moderate, not trivial. For symmetric A, `W == V`. Validate `W'*A = D*W'`
and the per-column normalization vs MATLAB.

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 26), `libs/linalg/src/eig.cpp`
  (`leftEigenvectors`).
- New helper: `eig(Mᵀ)` → reorder its columns so each matches D's k-th
  eigenvalue (nearest-value greedy match) → normalize each to unit 2-norm. The
  `eig_reg` 2-output block emits it as `outs[2]` for `nargout >= 3`. Only
  real-eigenvalue inputs are supported (the general eig path itself throws on
  complex eigenvalues).
- Verified vs MATLAB R2025b (sign/order-agnostic): `W'*A - D*W' ≈ 0` on every
  case; symmetric A → `W == V`; unit-norm columns; `sum(abs(W))` matches MATLAB
  exactly (2×2 `[4 -2;1 1]` → 2.755854; 3×3 `[2 0 0;1 3 0;0 1 4]` → 4.080880);
  `W'*V` is diagonal (left/right eigenvectors of different eigenvalues are
  orthogonal).
- **Sign/order note:** numkit's `eig` eigenvalue ORDER differs from MATLAB for
  *non-symmetric* matrices, and the per-column SIGN of eigenvectors follows
  numkit's own convention. Both are *pre-existing* (`[V,D]` already differs).
  `W` is consistent with numkit's own `D` (the relation holds), so validation
  is sign/order-agnostic — same playbook as bugs/linalg/qr-pivoting.md.
- Live guard: `libs/linalg/tests/eig_left_vectors_test.cpp` (6 TEST_F) + flipped
  `LinalgKnownBug.EigLeftVectors` live. Parity: `tools/parity/specs/eig.json`
  extended (correctness=OK). Smoke:
  `libs/linalg/tests/smoke/eig_left_vectors_smoke.m`.

## References
- `libs/linalg/src/eig.cpp` (eig_reg, leftEigenvectors)
- MATLAB `doc eig`
