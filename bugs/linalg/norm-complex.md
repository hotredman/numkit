# linalg.norm — complex input throws (vecnorm already handles it)

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (errors where MATLAB returns a value)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (complex-input sweep)

## Symptom
`norm` throws "Not a double array" on a complex vector/matrix (the default
2-norm, the p-norm, and `'fro'`). MATLAB norms a complex array via element
magnitudes. Notably `vecnorm` on the same input works — only `norm` is broken.

## Repro
```matlab
norm([3+4i 0])
% numkit: Error — Not a double array
% MATLAB: 5
norm([3+4i 0], 'fro')
% numkit: Error — Not a double array
% MATLAB: 5
vecnorm([3+4i 0])      % numkit == MATLAB == 5  (already correct)
```

## Root cause
`norm_value` and `norm_fro` (`toolboxes/linalg/src/norms.cpp:35` / `:111`) read
`x.doubleData()` unconditionally. `vecnorm` in the same file uses a `getAbs`
helper that branches on `A.isComplex()` (→ `std::abs(complexElem)`); `norm`'s
scalar paths never got that branch.

## Suggested fix
Give `norm_value`/`norm_fro` the same complex handling as `vecnorm`: when the
input is COMPLEX, use element magnitudes for the vector p-norm / Frobenius
norm, and for the matrix 2-norm route the complex matrix through the SVD
(numkit's svd already accepts complex). Small — reuse the `vecnorm` `getAbs`
pattern. Validate vs MATLAB on complex vectors (1/2/Inf/'fro') and matrices.

## References
- `toolboxes/linalg/src/norms.cpp` (norm_value, norm_fro, norm_inf, vecnorm getAbs)
- MATLAB `doc norm`

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 2).
- Added a complex-aware `absLin(x, i)` magnitude helper (mirrors `vecnorm`'s
  `getAbs`) and routed `norm_value` (vector 1/2/p + matrix 1-norm),
  `norm_inf` (vector + matrix), and `norm_fro` through it. Complex inputs now
  norm by element magnitude `|z|`, matching MATLAB.
- **Deferred:** the complex *matrix* 2-norm (spectral / largest singular
  value) needs a complex SVD, which is still unimplemented — it now throws a
  clear `numkit:norm:complexSpectral` error instead of the cryptic "Not a
  double array". Tracked under bugs/linalg/complex-matrix-unsupported.md.
- Live guard: `toolboxes/linalg/tests/norm_complex_test.cpp` (6 cases incl. a
  real-input regression + the spectral-throws guard). Parity:
  `tools/parity/specs/norm.json` extended (correctness=OK). Smoke:
  `toolboxes/linalg/tests/smoke/norm_complex_smoke.m`.
