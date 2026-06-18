# signal.periodogram — garbage spectrum for non-power-of-two nfft

- **Status:** 🔴 OPEN
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

## Suggested fix
When `nfft` is not a power of two, compute the length-`nfft` DFT via the general
`fft` (Bluestein) instead of `fftRadix2`, then apply the existing one-sided
scaling. (This is exactly what `obw` now does inline to dodge the bug — see
bugs/signal/obw-value-outputs.) `pwelch`/`cpsd`/`mscohere`/`tfestimate` and
`computePsd` share the helper, so fixing it in one place closes the gap for all.
Validate vs MATLAB on a non-pow2 nfft (e.g. 1000): dominant-tone bin + Parseval
`sum(P)·df == mean(x²)`.

## References
- `src/toolboxes/signal/src/spectral_analysis/periodogram_pwelch.cpp`
  (`periodogram`, uses `fftRadix2`).
- `src/toolboxes/signal/src/transforms/fft.cpp` (general `fft` with Bluestein).
- related: `bugs/signal/obw-value-outputs.md` (worked around this by calling the
  general `fft` directly).
