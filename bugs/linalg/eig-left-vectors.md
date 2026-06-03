# linalg.eig — 3rd output W (left eigenvectors) unsupported

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing output)
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

## References
- `libs/linalg/src/eig.cpp`
- MATLAB `doc eig`
