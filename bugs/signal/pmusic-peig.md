# signal.pmusic / signal.peig — functions missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
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

## Suggested fix
Both estimate a pseudospectrum from the signal/data correlation matrix:
build the (modified-)covariance/correlation matrix, eigen-decompose, split
into signal (top `p`) and noise subspaces, then
`P(f) = 1 / Σ_noise |e(f)' v_k|²` (pmusic) or the eigenvalue-weighted
variant (peig). Needs a symmetric eigensolver (available in core linalg).
Moderate; share one core with `pmusic`/`peig`/`rootmusic`/`espd`.

## References
- new file under `libs/signal/src/spectral_analysis/`
- MATLAB `doc pmusic`, `doc peig`
- related shipped: `pwelch`, `periodogram`, `corrmtx`
