# signal.cceps — non-power-of-two phase diverges + missing `nd` output

- **Status:** 🔴 OPEN
- **Severity:** P1 (wrong result for non-2ⁿ) + P2 (missing output)
- **Kind:** bug
- **Found:** 2026-06 via signal.* DEEP-PROBE sweep

## Symptom
1. For non-power-of-two lengths the complex cepstrum's **phase** still uses
   a simple per-bin unwrap; MATLAB uses `rcunwrap` (removes the linear-phase
   term). The magnitude term is correct (fixed alongside rceps — exact
   length, see signal/rceps-cceps-padding.md), but the per-sample result
   diverges past the DC bin.
2. The documented **2nd output** `nd` (samples of delay removed) is missing
   — `[xhat, nd] = cceps(x)` throws "Too many output arguments".

## Repro
```matlab
cceps([1 2 3 4 3 2 1])
% numkit: 0.396084 3.668860 1.591387 0.104849 -0.509763 -0.556044 -1.922784
% MATLAB: 0.396084 0.523560 0.383457 -0.240870 -0.164044 0.651886  1.222516
%         ^DC matches; phase-dependent samples differ
[xhat, nd] = cceps((1:8)')
% numkit: Error — Too many output arguments
% MATLAB: nd = 0 (for this input)
```
Power-of-two cceps (e.g. `(1:8)'`) already matches MATLAB bit-for-bit.

## Root cause
`libs/signal/src/transforms/extras.cpp` `cceps()` unwraps phase per-bin
(`while (phase-prev > pi) ...`) without MATLAB's `rcunwrap` linear-phase
removal, and never computes/returns `nd`.

## Suggested fix
Port `rcunwrap`: unwrap the full phase, estimate the linear trend
`nd = round(unwrapped(end)/pi)`, subtract `nd*pi*(0:n-1)/(n/2)` (or the
MATLAB-exact form), then `nd` becomes the 2nd output. Make `cceps_reg`
nargout-aware. Validate the phase on several non-2ⁿ inputs.

## References
- `libs/signal/src/transforms/extras.cpp` (cceps)
- `tools/parity/specs/cceps.json` (documents this gap)
- related fix: `bugs/signal/rceps-cceps-padding.md`
