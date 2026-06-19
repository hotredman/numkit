# signal.hilbert — wrong analytic signal for non-power-of-two lengths

- **Status:** ✅ FIXED (2026-06-19) — transform at length N (general FFT)
- **Severity:** P1 (wrong values on a core primitive; many dependents)
- **Kind:** bug
- **Found:** 2026-06-19 while implementing comm pmdemod/fmdemod

## Symptom
`hilbert(x)` returns an incorrect analytic signal whenever `length(x)` is
**not a power of two**: the magnitude is wrong and the constant-envelope
property is lost. Power-of-two lengths were correct, so the defect hid
behind the usual pow2 test inputs.

## Repro
```matlab
x = (1:6)';
imag(hilbert(x))
% MATLAB: [ 2.3094  -1.1547  -1.1547  -1.1547  -1.1547   2.3094]
% numkit: [-1.0000  -1.7249  -1.8284  -1.1036  -1.0000   3.2249]  (wrong)

n = (0:99)'; x = cos(2*pi*5*n/100);
max(abs(abs(hilbert(x)) - 1))
% MATLAB: ~1e-15 (constant envelope = 1)
% numkit: ~0.25  (|z| ≈ 0.75, not 1 — corrupted)
```

## Root cause
`hilbertBuf` (`transforms/hilbert.cpp`) padded the signal to
`fftLen = nextPow2(N)`, computed the analytic signal on the **padded**
length, then sliced the first `N` samples. Zero-padding changes the
spectrum, so for non-pow2 `N` the doubled-positive-frequency mask and the
inverse transform operate on the wrong-length DFT — the result is not the
length-`N` Hilbert transform. (For pow2 `N`, `fftLen == N`, so the padding
was a no-op and the result was correct — which is why every pow2 test
passed.)

## Fix (2026-06-19)
Compute the transform at the **signal length N**:
- pow2 `N`: unchanged fast path (`fftRadix2` at length `N`).
- non-pow2 `N`: the general (mixed-radix / Bluestein) `fft`/`ifft` at length
  `N` (the same `signal::fft` periodogram uses for non-pow2 nfft).

The analytic mask is applied at length `N` and handles even/odd parity:
double bins `1 … ⌈N/2⌉−1`, keep DC (and the Nyquist bin `N/2` when `N` is
even), zero `⌊N/2⌋+1 … N−1`. (For pow2 `N` this is identical to the old
mask.)

Verified vs MATLAB R2025b: `hilbert([1:6]')` imag `[2.3094, −1.1547,
−1.1547, −1.1547, −1.1547, 2.3094]`; `hilbert([1:7]')`, `[1:100]'` ramp
(incl. the edge-effect magnitudes, `|z(1)|=128.524726`) match exactly;
constant-envelope tone at L=100 → `|hilbert|=1`. **Fixes the downstream
non-pow2 users too** — `envelope`, `ssbmod`, `instfreq`, `vibration`,
`spectral_metrics`, and unblocks comm `pmdemod`/`fmdemod` (they were giving
~8% errors before). Full suite 12294 pass / 0 fail (no pow2-path
regressions). Guards: `hilbert_test.cpp` (`NonPowerOfTwoLength`,
`NonPowerOfTwoConstantEnvelope`); parity `hilbert_nonpow2.json`.

## References
- `src/toolboxes/signal/src/transforms/hilbert.cpp` (`hilbertBuf`).
- `tools/parity/specs/hilbert_nonpow2.json`.
- unblocks: bugs/comm/analog-demodulators.md (pmdemod/fmdemod)
- MATLAB `doc hilbert`
