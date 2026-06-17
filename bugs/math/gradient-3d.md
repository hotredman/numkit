# math.gradient — N-D (3-D) arrays unsupported

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (errors where MATLAB returns a value)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (N-D / multi-output sweep)

## Symptom
`gradient` only accepts 1-D vectors and 2-D matrices; a 3-D (or higher) array
throws "only 1D vector and 2D matrix inputs are supported". MATLAB computes
the gradient along every dimension — `[px,py,pz] = gradient(A)` for a 3-D `A`
(and the single-output form returns the first-dimension gradient `px`).

## Repro
```matlab
A = reshape(1:8, 2, 2, 2);
gradient(A)
% numkit: Error — gradient: only 1D vector and 2D matrix inputs are supported
% MATLAB: 2x2x2, g(1,1,1) = 2     (x-gradient)
[gx, gy, gz] = gradient(A)
% numkit: Error — gradient: 2-output form requires a 2D matrix input
% MATLAB: gz(1,1,1) = 4
```

## Root cause
`gradient` (`src/math/src/integration/integration.cpp`) caps the input rank at 2; there is no
loop over a third (or N-th) dimension and the multi-output form only emits
2 gradients.

## Suggested fix
Generalise to N-D: emit one gradient array per dimension (central differences
interior, one-sided at the ends), honouring optional per-dim spacing args
`gradient(A, hx, hy, hz, ...)`. Output count follows `nargout` (single output
= first-dim gradient). Moderate. Common for volume / field data.

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 18),
  `src/math/src/integration/integration.cpp`.
- New generic strided kernel `gradientAlongDim(src, dst, shape, dim, h)` —
  central differences interior, one-sided ends, uniform spacing, along any
  0-based dimension of a column-major N-D array (generalises the old
  `gradientAlongCols`/`gradientAlongRows`).
- New `gradientND(...)` emits `nout` gradients with the MATLAB output→dim map
  `{dim2(x), dim1(y), dim3, dim4, ...}` (the first two swapped, rest natural);
  single output = the dim-2 (x) gradient. Spacing: a single arg broadcasts to
  every dim, otherwise `gradient(A,h1,h2,h3,...)` maps per output. Complex F is
  gradiented part-wise (real/imag) and recombined. Requesting more outputs than
  dimensions throws `numkit:gradient:nargout`.
- `gradient_reg` routes 3-D+ inputs through `gradientND`; vector and 2-D matrix
  paths are byte-for-byte unchanged. The single-output C++ `gradient()` also
  handles N-D now (the x gradient).
- Verified vs MATLAB R2025b: `gradient(reshape(1:8,2,2,2))` g(1,1,1)=2;
  `[gx,gy,gz]` (1,1,1)=(2,1,4); 3×3×3 central bx(1,2,1)=3, bz(1,1,2)=9;
  spacing (2,3,4)→(1, 1/3, 1); single-spacing broadcast; non-cube 2×3×2; 4-D
  d4=8; complex 3-D zx(1,1,1)=2−2i.
- Live guard: `tests/math/gradient_nd_test.cpp` (11 TEST_F) + flipped
  `BuiltinKnownBug.Gradient3D` live; two stale throw-tests rewritten
  (`CalculusTest.Gradient3DInput`, `GradientComplexTest.NDComplexOk`). Parity:
  `tools/parity/specs/gradient_nd.json` (correctness=OK). Smoke:
  `tests/math/smoke/gradient_nd_smoke.m`.
- Deferred sub-gap: coordinate-vector spacing per dim (`gradient(A, xvec, ...)`)
  is still scalar-spacing only — the pre-existing 2-D path didn't support it
  either, so no regression. `del2` remains 1-D/2-D (separate).

## References
- `src/math/src/integration/integration.cpp` (gradient)
- MATLAB `doc gradient` (N-D)
