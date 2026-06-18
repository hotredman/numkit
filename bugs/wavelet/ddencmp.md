# wavelet.ddencmp — default denoising / compression parameters missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE (split from wavelet/wentropy on 2026-06-19)

## Symptom
`ddencmp` (default denoising/compression thresholds) is not registered.
(Split from the original wentropy/ddencmp entry; `wentropy` is now fixed —
see wavelet/wentropy.md.)

## Repro
```matlab
[thr, sorh, keepapp] = ddencmp('den', 'wv', [1 2 3 4 5]);
% MATLAB: thr = universal threshold sqrt(2·log(n))·σ̂, sorh = 's', keepapp = 1
```

## Root cause
Not implemented.

## Suggested fix
`ddencmp(opt, 'wv'|'wp', x)` returns the default parameters for denoising
(`opt = 'den'`) or compression (`opt = 'cmp'`):
- **σ̂** (noise std) from the finest-detail coefficients: a 1-level `dwt`
  of `x` (reuse the shipped `dwt`/`wnoisest`), then `σ̂ = median(|cD₁|) /
  0.6745` (MAD estimate).
- **thr** = `sqrt(2·log(n))·σ̂` (universal / VisuShrink threshold) for
  `'den'`; for `'cmp'` MATLAB uses a different default (`median(|cD₁|)`
  scaled) — probe the exact form.
- **sorh** = `'s'` (soft) for `'wv'`, `'h'` for `'wp'`; **keepapp** = 1.

Returns `[thr, sorh, keepapp]`. Small — reuses a 1-level `dwt` + `wnoisest`.
Verify `thr`/`sorh`/`keepapp` vs MATLAB on a few signals (and the `'cmp'`
branch separately).

## References
- `src/toolboxes/wavelet/src/...` (reuse `dwt`/`wnoisest`)
- guard: `known_bugs_test.cpp` (`DISABLED_Ddencmp`)
- split from wavelet/wentropy.md (the entropy half, now fixed)
- MATLAB `doc ddencmp`
