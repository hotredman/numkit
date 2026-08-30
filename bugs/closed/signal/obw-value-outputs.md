# signal.obw — wrong occupied-bandwidth value + missing outputs

- **Status:** ✅ FIXED (2026-06-18) — value matches MATLAB + [bw,flo,fhi,power] emitted
- **Severity:** P1 (wrong value) + P2 (missing outputs)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (spectral-measurement sweep)

## Symptom
1. **Value** — `obw(x, fs)` (the 99% occupied bandwidth) diverges from MATLAB.
2. **Outputs** — MATLAB `obw` returns up to four outputs
   `[bw, flo, fhi, power]` (bandwidth + lower/upper band edges + power in the
   band); numkit emits only `bw` and throws "Too many output arguments" for
   the rest.

## Repro
```matlab
fs = 1000; t = (0:fs-1)/fs;
x = sin(2*pi*100*t) + 0.5*sin(2*pi*200*t);   % deterministic two-tone

obw(x, fs)
% numkit: 108.7721
% MATLAB: 100.9688

[bw, flo, fhi, p] = obw(x, fs)
% numkit: Error — Too many output arguments
% MATLAB: bw=100.9688, flo=99.5062, fhi=200.4750, p=0.6188
```

## Root cause
Two parts. The single-output value already differs (~8%), so numkit's
occupied-bandwidth integration (PSD estimate and/or the 99%-power band
search) does not match MATLAB's. Separately, the `obw_reg` adapter emits
only `outs[0]` — the band edges and in-band power are not threaded to
`nargout`.

## Fix (2026-06-18)
Black-box probing of MATLAB R2025b pinned the exact algorithm (it's a standard
cumulative-power band — no MATLAB source read). Three bugs compounded:
1. **nfft** — numkit's PSD used the default `max(256, 2^nextpow2(N))` (=1024 for
   N=1000, zero-padded); MATLAB uses **nfft = N** (no padding). This was the
   dominant error (108.77 vs 100.97).
2. **cumulative rule** — numkit used the trapezoid; MATLAB uses the **rectangle
   rule** `cum[k] = Σ_{j≤k} P[j]·df`.
3. **band-edge frequency** — MATLAB places a band edge at the bin's **upper edge
   `F + df/2`** (not the bin centre). `bw = fhi − flo` is invariant to the shift,
   but `flo`/`fhi` need it.
The window is **rectangular** (NOT Kaiser — the original "Kaiser" guess above was
wrong). The band edges are the `0.5%` / `99.5%` cumulative-power crossings;
`power = p · total`.

numkit's `periodogram` helper is radix-2 only, so it can't take `nfft = N` for a
non-power-of-two `N` (that path produced a garbage spectrum). `obw` now computes
the length-N DFT via the general `fft` (Bluestein for non-pow2 N) and forms the
one-sided PSD inline: `Pxx[k] = (2 if folded)·|X[k]|² / (fs·N)`, which obeys
`Σ Pxx·df = mean(x²)`. The compute fn now returns `(bw, flo, fhi, power)` and
`obw_reg` emits them by `nargout`.

Verified vs MATLAB R2025b (parity `obw.json` → OK) on the two-tone
`sin(2π·100t)+0.5·sin(2π·200t)`, fs=1000: `bw=100.96875`, `flo=99.50625`,
`fhi=200.475`, `power=0.61875`; and a second fs=8000 signal matched to ~1e-12.
Guards: `spectral_measurements_test.cpp` (`OccupiedBandwidth`, DualEngine TW+VM) +
`known_bugs_test.cpp` (`ObwValueAndOutputs`, promoted live); smoke `obw_smoke.m`.

NB this is a periodogram bug too: `periodogram(x, [], nfft, fs)` with a non-pow2
`nfft` returns a garbage spectrum (radix-2 only) — a separate latent gap, not
exercised by the common power-of-two / default path.

## References
- `src/toolboxes/signal/src/spectral_analysis/spectral_metrics.cpp` (`obw`),
  `.../spectral_metrics.hpp`,
  `src/bundle/src/register/signal/spectral_analysis/spectral_metrics_reg.cpp`
  (`obw_reg` nargout).
- `tools/parity/specs/obw.json`.
- MATLAB `doc obw`
