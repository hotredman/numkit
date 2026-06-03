# signal.freqs — scalar `w` treated as a frequency, not a point count

- **Status:** 🔴 OPEN
- **Severity:** P3 (edge-case input form)
- **Kind:** bug
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`freqs(b, a, n)` with a SCALAR third argument: MATLAB treats `n` as the
number of auto-spaced frequency points (returns an `n`-element response);
numkit treats the scalar as a single frequency value `w` (returns one
point). The vector-`w` form matches MATLAB exactly.

## Repro
```matlab
h = freqs([1 0], [1 1 1], 2);
% numkit: numel(h) = 1, |h| = 0.5547   (evaluated at w = 2)
% MATLAB: numel(h) = 2  (2 auto-spaced frequency points: |h| = [0.0100 0.1005])
freqs([1 0],[1 1 1],[1 2 3])   % vector w: numkit == MATLAB ([1 0.5547 0.3511])
```

## Root cause
The `freqs` adapter routes a scalar third arg to the "evaluate at these
frequencies" path; MATLAB routes a scalar to the "n auto-points" path
(only a vector means "these frequencies").

## Suggested fix
When the third arg is a scalar, treat it as `n` (number of points) and build
the auto frequency range the way MATLAB does (logspace over the dynamic
range of the response), matching `freqs(b,a,n)`. Low priority — passing a
scalar to `freqs` is unusual; the vector form (the common case) is correct.

## References
- `libs/signal/src/.../freqs*`
- MATLAB `doc freqs`
