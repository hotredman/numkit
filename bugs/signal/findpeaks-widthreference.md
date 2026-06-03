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

## Reverse-engineering notes (probed 2026-06-03, c183 — partial)
Confirmed `ref = h/2` for halfheight. Where `h/2` lies between the two
adjacent valleys the answer equals `'halfprom'` (e.g. isolated peaks
`[0 1 2 5 2 1 0]` and `[0 0 4 10 4 0 0]` both give halfprom == halfheight).
MATLAB samples (`'WidthReference'`):
| signal | peaks | halfprom w | halfheight w |
|---|---|---|---|
| `[1 3 2 5 1 6 1]` | 3,5,6 | 0.75, 1.1667, 1.0 | 1.75, 1.4583, 1.20 |
| `[5 5 5 6 9 6 5 5 5]` (pedestal) | 9 | 1.3333 | **6.0** |
| `[0 1 2 5 2 1 0]` | 5 | 1.6667 | 1.6667 |
The blocker remains the pedestal case: when `h/2` is BELOW the signal's
minimum inside the peak's borders (no crossing), MATLAB does NOT return the
full inter-border span (8) but **6.0** — this depends on MATLAB's exact
prominence-base **index** selection (which of the equal-height valley samples
defines the contour), which we could not pin from outputs alone. Need
MATLAB's `findpeaks` source for `getHalfMaxBounds`/the width-bound clamp
before implementing. Deferred as fiddly.

## References
- `libs/signal/src/measurements/findpeaks.cpp`
- MATLAB `doc findpeaks` (WidthReference)
