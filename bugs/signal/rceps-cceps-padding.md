# signal.rceps / cceps / icceps — garbage on non-power-of-two lengths

- **Status:** ✅ FIXED (9fcf6872, 2026-06)
- **Severity:** P1 (wrong result)
- **Found:** 2026-06 via signal.* DEEP-PROBE sweep

## Symptom
The cepstrum functions transformed on a buffer zero-padded to `nextPow2(n)`.
But the cepstrum takes `log|X|`, which blows up at the near-zero spectral
bins that padding introduces — so any **non-power-of-two length** returned
garbage. The 2nd output of `rceps` (minimum-phase reconstruction) was also
missing.

## Repro (pre-fix)
```matlab
rceps([1 2 3 4 3 2 1])
% numkit (pre-fix): -258.174 87.005 86.694 86.382 -259.214 86.382 86.694
% MATLAB:            0.396084 0.873038 0.517671 -0.202457 -0.202457 0.517671 0.873038
[y, ym] = rceps([1 2 3 4 3 2 1])
% numkit (pre-fix): Error — Too many output arguments
% MATLAB ym:         1.603952 2.571895 3.739440 3.372840 2.652253 1.415667 0.643953
```
A padded symmetric input also hit `log(0)` at the Nyquist bin (MATLAB
rejects such inputs; numkit produced NaN-scale garbage).

## Root cause
`fftReal()` in `libs/signal/src/transforms/extras.cpp` used `fftRadix2`
with `nextPow2(n)` padding. `log|X|` of the padded near-zero bins ≈ -690,
smeared across every output sample by the inverse transform.

## Fix
Transform at the EXACT length `n` (new `dftExact` helper — radix-2 when `n`
is a power of two, direct O(n²) DFT otherwise, same twiddle/dir convention
as `fftRadix2`). Applied to rceps/cceps/icceps; outputs now keep the input
orientation. Added rceps's `ym` 2nd output (`rcepsMinPhase`, nargout-aware
adapter). Power-of-two parity (the `(1:8)'` specs) preserved bit-for-bit.

**Remaining:** cceps non-2ⁿ **phase** still diverges (needs `rcunwrap`) and
its `nd` output is still missing — see `bugs/signal/cceps-nd-phase.md`.

## References
- `libs/signal/src/transforms/extras.cpp`
- `tools/parity/specs/rceps.json`, `cceps.json`
- `libs/signal/tests/rceps_test.cpp`
