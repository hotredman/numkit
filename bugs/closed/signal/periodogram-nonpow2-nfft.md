# signal.periodogram — garbage spectrum for non-power-of-two nfft

- **Status:** ✅ FIXED (2026-06-18) — non-pow2 nfft routes through the Bluestein fft
- **Severity:** P2 (wrong result for an explicit non-pow2 nfft)
- **Kind:** bug
- **Found:** 2026-06-18 (while fixing bugs/signal/obw-value-outputs)

## Symptom
`periodogram(x, win, nfft, fs)` with an explicit **non-power-of-two** `nfft`
returns a garbage spectrum: the energy lands in the wrong bins and the PSD no
longer obeys Parseval. The default `nfft` (`max(256, 2^nextpow2(N))`) and any
power-of-two `nfft` are correct — only a non-pow2 `nfft` is affected. `pwelch`
and the other helpers built on the same routine share the latent gap.

## Repro
```matlab
fs = 1000; t = (0:fs-1)/fs; x = sin(2*pi*100*t) + 0.5*sin(2*pi*200*t);
[P, F] = periodogram(x, [], 1000, fs);   % nfft = 1000 (non-pow2)
[~, ix] = max(P); F(ix)         % numkit: ~256 (wrong)   MATLAB: 100
sum(P) * (F(2)-F(1))            % numkit: ~21.5 (wrong)  MATLAB: 0.625 (= mean(x^2))
```

## Root cause
`periodogram` (`src/toolboxes/signal/src/spectral_analysis/periodogram_pwelch.cpp`)
runs the transform via `fftRadix2`, which requires a power-of-two length. With a
non-pow2 `nfft` it produces garbage — there is no Bluestein/mixed-radix fallback
on this path. The general `fft` (`transforms/fft.cpp`) already has a Bluestein
path for arbitrary `N`; `periodogram` should route non-pow2 `nfft` through it.

## Fix (2026-06-18)
`periodogram` now gates on the transform length: a power-of-two `nfft` keeps the
fast `fftRadix2` path (unchanged — zero risk for the default and all existing
callers), while a non-pow2 `nfft` routes the windowed, zero-padded signal through
the general `fft` (Bluestein, `O(N log N)`) before the existing one-sided scaling.
`pwelch`/`cpsd`/`mscohere`/`tfestimate`/`computePsd` share the helper and inherit
the fix. (`obw` keeps its own inline general-fft PSD; not refactored to avoid
churn.)

Verified vs MATLAB R2025b (parity `periodogram_nonpow2.json` → OK):
`periodogram(sin(2π·100t)+0.5·sin(2π·200t), [], 1000, 1000)` → 501 one-sided bins
on `[0,500]`, `P(f=100)=0.5`, `P(f=200)=0.125`, `Σ P·df = 0.625 = mean(x²)`
(Parseval). Previously the peak landed at ~256 Hz with `Σ P·df ≈ 21.5`. Guard:
`known_bugs_test.cpp` (`PeriodogramNonPow2Nfft`, promoted live); smoke
`periodogram_nonpow2_smoke.m`.

## References
- `src/toolboxes/signal/src/spectral_analysis/periodogram_pwelch.cpp`
  (`periodogram` — pow2 gate + Bluestein route).
- `src/toolboxes/signal/src/transforms/fft.cpp` (general `fft` with Bluestein).
- `tools/parity/specs/periodogram_nonpow2.json`.
- related: `bugs/signal/obw-value-outputs.md` (worked around this inline).
