# wavelet.wentropy / wavelet.ddencmp — denoising helpers missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`wentropy` (wavelet entropy) and `ddencmp` (default denoising/compression
parameters) are not registered.

## Repro
```matlab
wentropy([1 2 3 4], 'shannon')
% numkit: Error — VM: undefined function 'wentropy'
% MATLAB: -sum(s.^2 .* log(s.^2)) over nonzero s  ->  scalar
[thr, sorh, keepapp] = ddencmp('den', 'wv', [1 2 3 4 5])
% numkit: Error — VM: undefined function 'ddencmp'
% MATLAB: thr (universal threshold), sorh='s', keepapp=1
```

## Root cause
Not implemented.

## Suggested fix
- `wentropy(x, T)`: closed-form entropy over the coefficients —
  `'shannon'` = `-Σ s²·log(s²)`, `'log energy'` = `Σ log(s²)`,
  `'threshold'`/`'sure'`/`'norm'` variants. Small, self-contained.
- `ddencmp(opt, 'wv'|'wp', x)`: returns the universal threshold
  `thr = sqrt(2·log(n))·σ̂` (σ̂ from the finest-detail MAD/0.6745), default
  `sorh`, and `keepapp`. Small; reuses a 1-level `dwt`.
Both are small and good first wavelet wins.

## References
- new file(s) under `libs/wavelet/src/...`
- shipped: `dwt`, `wthresh`, `wdenoise`
- MATLAB `doc wentropy`, `doc ddencmp`
