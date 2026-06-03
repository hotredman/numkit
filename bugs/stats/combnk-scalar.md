# stats.combnk — scalar first arg expanded to 1:N instead of a 1-element set

- **Status:** 🔴 OPEN
- **Severity:** P3 (edge-case input)
- **Kind:** bug
- **Found:** 2026-06 via DEEP-PROBE (stats coverage)

## Symptom
`combnk(N, k)` with a SCALAR `N`: numkit treats it as `combnk(1:N, k)`;
MATLAB treats the scalar as a 1-element set `[N]`. The vector form
(`combnk(v, k)`) matches MATLAB.

## Repro
```matlab
size(combnk(5, 2), 1)
% numkit: 10   (wrongly enumerated combinations of 1:5)
% MATLAB: 0    (choosing 2 from the 1-element set {5} -> empty)
size(combnk(1:4, 2), 1)   % vector form: numkit == MATLAB == 6
```

## Root cause
`combnk` expands a scalar argument to `1:N` (nchoosek-style) instead of
treating it as a length-1 vector.

## Suggested fix
Remove the scalar→`1:N` expansion: a scalar `v` is the set `{v}`, so
`combnk(v, k)` is empty for `k > 1` and `{v}` for `k == 1`. Trivial, but
low value (scalar input to combnk is unusual). Note: `nchoosek(N, k)` with
a scalar N IS the count — `combnk` is the (set) enumerator; don't conflate.

## References
- `libs/stats/src/.../combnk*`
- MATLAB `doc combnk`
