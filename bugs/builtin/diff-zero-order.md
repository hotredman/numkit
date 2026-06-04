# builtin.diff — accepts n=0 (returns identity); MATLAB errors

- **Status:** 🔴 OPEN
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
`diff` (`libs/builtin/src/language/arrays/matrix.cpp`) only guards `n < 0`
(throws "order n must be non-negative") and treats `n == 0` as an identity
copy. MATLAB requires `N` to be a *positive* integer scalar, so `0` should
error too.

## Suggested fix
Tighten the order check: require `n >= 1` and integer-valued
(`n == floor(n)`, finite). Throw the MATLAB-style message
"Difference order N must be a positive integer scalar." Remove the `n == 0`
identity branch (or keep it only for an internal caller, if any). Small —
audit any internal `diff(x, 0)` callers first. Validate the error vs MATLAB.

## References
- `libs/builtin/src/language/arrays/matrix.cpp` (diff, the `n == 0` branch)
- MATLAB `doc diff`
- Found alongside bugs/builtin/diff-complex.md (FIXED); the n=0 path there
  was only updated to preserve complex parts, not to reject n=0.
