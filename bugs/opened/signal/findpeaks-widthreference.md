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
clear; import compat.*;
findpeaks([1 3 2 5 1 6 1], 'WidthReference', 'halfheight')
% numkit: Error — findpeaks: option 'WidthReference' not supported
% MATLAB: (returns peaks; width measured at half the absolute peak height)
```

## Root cause
`src/toolboxes/signal/src/measurements/findpeaks.cpp` `peakWidth()` hard-codes the
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

## Reverse-engineering notes (probed 2026-06-05, c40 — still blocked)
Confirmed: the CROSSING case is standard and matchable — e.g. strictly
descending base `[3 4 5 6 9 6 5 4 3]` gives halfheight w=5.0 by ordinary
linear interpolation at `h/2=4.5` (crossings at 2.5 and 7.5). numkit's
existing `peakWidth` crossing+interp logic reproduces this once `ref=h/2`.
The current prominence bounds (`peakProminence` lb/rb) span the WHOLE
interval to the next-higher sample / array end, so the no-crossing fallback
returns that span (8 for the pedestal) — NOT MATLAB's 6.0.
Asymmetric-pedestal probe (all halfheight, base flat = 5, peak = 9, ref=4.5,
no crossing) to pin the no-crossing bound rule:
| signal | peak@ | MATLAB w |
|---|---|---|
| `[5 5 5 6 9 6 5 5 5]` | 5 | 6 |
| `[5 5 6 9 6 5 5 5]`   | 4 | 5 |
| `[5 5 5 6 9 6 5 5]`   | 5 | 6 |
| `[5 6 9 6 5 5 5]`     | 3 | 4 |
| `[5 6 9 6 5]`         | 3 | 4 |
These widths {6,5,6,4,4} do NOT fit any single closed-form bound rule tried
(middle-of-flat, innermost-of-flat, farthest, min-half-width-doubled all
contradict at least one case — e.g. `[5 5 5 6 9 6 5 5 5]`→6 needs bounds at
the flat midpoints 2/8, but `[5 6 9 6 5 5 5]`→4 needs the right bound at the
innermost flat sample 5, not its midpoint 6). So the no-crossing width is
governed by MATLAB's internal prominence base-INDEX bookkeeping and is not
recoverable from black-box outputs. STILL blocked on MATLAB source; the
crossing case alone is matchable but a partial (crossing-only) implementation
would diverge on any high-pedestal peak, so not shipped.

## References
- `src/toolboxes/signal/src/measurements/findpeaks.cpp`
- MATLAB `doc findpeaks` (WidthReference)
