# signal.pmusic / signal.peig — functions missing

- **Status:** ✅ FIXED (2026-06-18) — pmusic + peig (peak-frequency parity)
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via signal.* DEEP-PROBE sweep

## Symptom
`pmusic` (MUSIC pseudospectrum) and `peig` (eigenvector pseudospectrum) are
not registered.

## Repro
```matlab
[p, f] = pmusic([1 2 1 3 2 4 1 2 1 3], 4)
% numkit: Error — VM: undefined function 'pmusic'
[p, f] = peig([1 2 1 3 2 4 1 2 1 3], 4)
% numkit: Error — VM: undefined function 'peig'
```

## Root cause
Not implemented.

## Fix (2026-06-18)
Implemented `numkit::signal::pmusic` + `peig` in
`src/toolboxes/signal/src/spectral_analysis/pseudospectrum.cpp` (shared core),
registered under `spectral_analysis`. Build `R = X'·X` (order `2p` via
`corrmtx`), eigendecompose with a self-contained Jacobi solver (the toolbox
pattern, as in `stats/pca`), take the smallest `p` eigenvectors as the noise
subspace, then evaluate `P(ω) = 1/Σ_noise |e(ω)'·v_k|²` (pmusic) or the
`1/λ_k`-weighted variant (peig) over a one-sided `nfft/2+1` grid on `[0, fs/2]`.

**Peak-frequency parity only.** The algorithm was validated by reproducing it
in MATLAB before porting; numkit's detected peak frequencies match MATLAB
exactly. The *absolute* pseudospectrum is deliberately NOT bit-matched — its
peaks are `1/(near-zero)`, hypersensitive to the eigendecomposition (numkit
Jacobi vs MATLAB LAPACK), and the overall scale is arbitrary for a frequency
estimator (a non-peak bin differed ~2% in probing). The parity spec + gtest
therefore assert the **peak locations** + grid shape, not the raw values.

Verified vs MATLAB R2025b (parity `pmusic_peig.json` → OK): two tones
`cos(2π·0.1n)+cos(2π·0.25n)`, `p=4` → both pmusic and peig peak at `0.6381` and
`1.5708` rad/sample; 129 one-sided bins on `[0, π]`. Guard:
`lpc_parametric_test.cpp` (`PmusicPeig`, DualEngine TW+VM); smoke
`pmusic_peig_smoke.m`.

## References
- `src/toolboxes/signal/src/spectral_analysis/pseudospectrum.cpp`
  (`pmusic`/`peig` + shared core),
  `.../include/numkit/signal/spectral_analysis/signal_modeling.hpp`,
  `src/bundle/src/register/signal/spectral_analysis/pseudospectrum_reg.cpp`.
- `tools/parity/specs/pmusic_peig.json`.
- MATLAB `doc pmusic`, `doc peig`
- related shipped: `pwelch`, `periodogram`, `corrmtx`
