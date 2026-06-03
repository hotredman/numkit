# signal.findpeaks — 'WidthReference' option unsupported

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing option)
- **Kind:** stub
- **Found:** 2026-06 via signal.* DEEP-PROBE sweep (previously known gap)

## Symptom
`findpeaks(..., 'WidthReference', 'halfheight'|'halfprom')` throws. numkit
always measures the width `w` at the half-prominence reference; the
`'halfheight'` reference (and explicit `'halfprom'`) are rejected.

## Repro
```matlab
findpeaks([1 3 2 5 1 6 1], 'WidthReference', 'halfheight')
% numkit: Error — findpeaks: option 'WidthReference' not supported
% MATLAB: (returns peaks; width measured at half the absolute peak height)
```

## Root cause
`libs/signal/src/measurements/findpeaks.cpp` `peakWidth()` hard-codes the
reference `ref = h - prom/2` (halfprom) and the option parser rejects
`WidthReference` (around line 303).

## Suggested fix
Add the option. `'halfprom'` (default) keeps `ref = h - prom/2`;
`'halfheight'` uses `ref = h/2`. **Caution — fiddly:** MATLAB's halfheight
width interacts with the prominence-interval bounds in a non-obvious way
(on a pedestal `[5 5 5 6 9 6 5 5 5]` MATLAB returns width 6.0, not the naive
full-interval 8). Reverse-engineer MATLAB's exact bound/clamp behavior and
validate across pedestal + edge cases before claiming parity. Moderate.

## References
- `libs/signal/src/measurements/findpeaks.cpp`
- MATLAB `doc findpeaks` (WidthReference)
