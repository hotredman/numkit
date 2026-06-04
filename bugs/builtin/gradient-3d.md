# builtin.gradient — N-D (3-D) arrays unsupported

- **Status:** 🔴 OPEN
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
`gradient` (`libs/builtin/src/...`) caps the input rank at 2; there is no
loop over a third (or N-th) dimension and the multi-output form only emits
2 gradients.

## Suggested fix
Generalise to N-D: emit one gradient array per dimension (central differences
interior, one-sided at the ends), honouring optional per-dim spacing args
`gradient(A, hx, hy, hz, ...)`. Output count follows `nargout` (single output
= first-dim gradient). Moderate. Common for volume / field data.

## References
- `libs/builtin/src/...` (gradient)
- MATLAB `doc gradient` (N-D)
