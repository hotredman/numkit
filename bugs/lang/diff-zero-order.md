# lang.diff — accepts n=0 (returns identity); MATLAB errors

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P3 (accepts an input MATLAB rejects; returns a value vs error)
- **Kind:** bug
- **Found:** 2026-06-05 while fixing diff complex support (bug-fix loop, cycle 3)

## Symptom
`diff(X, 0)` returns `X` unchanged (identity) in numkit. MATLAB rejects a
zero (or non-positive / non-integer) difference order.

## Repro
```matlab
diff([1 2 3], 0)
% numkit: [1 2 3]   (identity)
% MATLAB: Error — "Difference order N must be a positive integer scalar."
diff([2+3i 7+1i], 0)
% numkit: [2+3i 7+1i]
% MATLAB: same error
```

## Root cause
`diff` (`src/lang/src/arrays/matrix.cpp`) only guards `n < 0`
(throws "order n must be non-negative") and treats `n == 0` as an identity
copy. MATLAB requires `N` to be a *positive* integer scalar, so `0` should
error too.

## Suggested fix
Tighten the order check: require `n >= 1` and integer-valued
(`n == floor(n)`, finite). Throw the MATLAB-style message
"Difference order N must be a positive integer scalar." Remove the `n == 0`
identity branch (or keep it only for an internal caller, if any). Small —
audit any internal `diff(x, 0)` callers first. Validate the error vs MATLAB.

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 20),
  `src/lang/src/arrays/matrix.cpp`.
- `diff_reg` now rejects a non-scalar / non-finite / fractional / `< 1` order
  with "diff: Difference order N must be a positive integer scalar" (the old
  guard allowed `0` and only checked `nv < 0`). The C++ primitive `diff()` was
  tightened the same way (`n < 1` throws) and the dead `n == 0` identity branch
  removed — verified no internal caller passes `0` (only `diff_reg` calls it).
- Verified vs MATLAB R2025b: `diff(X,0)`, `diff(X,-1)`, `diff(X,1.5)`,
  `diff(X,[1 2])`, `diff(X,Inf)`, `diff(X,NaN)` all error; valid orders
  (`diff(X)`, `diff(X,2)`, `diff(X,n,dim)`, integer + complex) unchanged.
- Live guard: `src/lang/tests/diff_order_test.cpp` (3 TEST_F) + flipped
  `BuiltinKnownBug.DiffZeroOrderErrors` live; stale
  `CumLogicalTest.DiffOrderZeroReturnsCopy` rewritten to
  `DiffOrderZeroErrors`. Parity: `tools/parity/specs/diff.json` (valid orders
  correctness=OK; error cases noted). Smoke:
  `src/lang/tests/smoke/diff_order_smoke.m`.

## References
- `src/lang/src/arrays/matrix.cpp` (diff + diff_reg)
- MATLAB `doc diff`
- Found alongside bugs/lang/diff-complex.md (FIXED); the n=0 path there
  was only updated to preserve complex parts, not to reject n=0.
