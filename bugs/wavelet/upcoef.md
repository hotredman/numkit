# wavelet.upcoef — direct coefficient reconstruction missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep (split from wavelet/wenergy on 2026-06-19)

## Symptom
`upcoef` (direct reconstruction of approximation/detail coefficients up N
levels) is not registered. (Split from the original wenergy/upcoef entry;
`wenergy` is now fixed — see wavelet/wenergy.md.)

## Repro
```matlab
y = upcoef('a', 5, 'db1', 2);
% MATLAB: y = [2.5 2.5 2.5 2.5]  (a single approximation coefficient 5,
%   reconstructed up 2 levels with the db1 synthesis filter)
% numkit: Error — VM: undefined function 'upcoef'
```

## Root cause
Not implemented. `upcoef(O, X, wname, N)` takes a coefficient band `X`
(`O='a'` approximation or `O='d'` detail) and reconstructs it up `N` levels
through the synthesis filter bank — i.e. repeated dyadic upsample +
convolve with the reconstruction lowpass `Lo_R` (for an approximation) and,
on the first step for a detail, `Hi_R`.

## Suggested fix
Reuse the idwt synthesis path: starting from `X`, iterate `N` times
`upsample(·,2)`-then-`conv` with `Lo_R` (`O='a'`) — for `O='d'`, the first
step convolves with `Hi_R` then the remaining `N−1` with `Lo_R`. Optional
length argument trims/centres the output (`wkeep`). Verify
`upcoef('a',5,'db1',2) == [2.5 2.5 2.5 2.5]` and a detail case vs MATLAB.

## References
- `src/toolboxes/wavelet/src/...` (reuse `wfilters`/`dwt` synthesis, `dyadup`,
  `wkeep`)
- guard: `known_bugs_test.cpp` (`DISABLED_Upcoef`)
- split from wavelet/wenergy.md (the energy half, now fixed)
- MATLAB `doc upcoef`
