# signal.cceps — non-power-of-two phase diverges + missing `nd` output

- **Status:** ✅ FIXED (2026-06-18) — rcunwrap phase + nd output; also fixed a time-reversal (fft dir)
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
`src/toolboxes/signal/src/transforms/extras.cpp` `cceps()` unwraps phase per-bin
(`while (phase-prev > pi) ...`) without MATLAB's `rcunwrap` linear-phase
removal, and never computes/returns `nd`.

## Fix (2026-06-18)
Ported MATLAB's `rcunwrap` into `cceps()`: after the standard per-bin unwrap,
remove the integer-lag linear-phase term — `nh = fix((n+1)/2)`,
`nd = round(unwrapped(nh+1)/pi)`, `phase -= pi*nd*(0:n-1)/nh` — and return `nd`
as the 2nd output (`cceps_reg` is now nargout-aware; `cceps()` gained an
optional `double *ndOut`). A second bug surfaced once the magnitudes were
correct: the FORWARD fft used `dir=+1` (same as the inverse pass), which
**time-reversed** the cepstrum (and flipped `nd`'s sign). Changed the forward
pass to `dir=-1` (true forward); `icceps` had the identical reversal and got the
same fix so the round-trip stays consistent.

Verified vs MATLAB R2025b (parity OK, `tools/parity/specs/cceps.json`):
`cceps([1 2 3 4 3 2 1])` = `[0.3961 0.5236 0.3835 -0.2409 -0.1640 0.6519
1.2225]`, `cceps((1:8)')` bit-identical, `nd` = 1 for `(1:8)'` and −3 for the
7-tap palindrome. (NB the bug-doc repro above claimed `nd=0` for `(1:8)'`;
MATLAB R2025b actually returns `1` — numkit matches the real MATLAB.)
Guards: `src/toolboxes/signal/tests/cceps_test.cpp`
(`NonPowerOfTwoPhaseMatchesMatlab`, `NdSecondOutput`, `IccepsRoundTripPreservesShape`)
+ `known_bugs_test.cpp` (`CcepsPhaseAndNd`, promoted live);
smoke `cceps_phase_smoke.m`.

## References
- `src/toolboxes/signal/src/transforms/extras.cpp` (`cceps` rcunwrap + fft dir;
  `icceps` fft dir), `src/bundle/src/register/signal/transforms/extras_reg.cpp`
  (`cceps_reg` nargout), `src/toolboxes/signal/include/numkit/signal/transforms/extras.hpp`.
- `tools/parity/specs/cceps.json`.
- related fix: `bugs/signal/rceps-cceps-padding.md`.
