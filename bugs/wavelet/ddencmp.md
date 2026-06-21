# wavelet.ddencmp — default denoising / compression parameters missing

- **Status:** ✅ FIXED (2026-06-19) — MAD noise estimate + universal threshold
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
% MATLAB: thr = 1.880854323469, sorh = 's', keepapp = 1
```

## Fix (2026-06-19)
Implemented `numkit::wavelet::ddencmp` (`denoise.cpp`, next to
`wentropy`/`wthresh`). Estimates the noise level from the finest-detail
coefficients of a **1-level db1 DWT** — `σ̂ = median(|cD₁|)/0.6745` (MAD) —
and returns `[thr, sorh, keepapp]`:
- **`opt='den'`** (denoise): `thr = sqrt(2·log(n))·σ̂` (universal /
  VisuShrink threshold), `sorh='s'`.
- **`opt='cmp'`** (compress): `thr = median(|cD₁|)`, `sorh='h'`.

`keepapp = 1` in both. numkit's `dwt(x,'db1')` matches MATLAB's finest
detail bit-for-bit (incl. the odd-length boundary), so the thresholds match
exactly. Only `type='wv'` is supported; `'wp'` (wavelet packet — different
threshold + a 4th `crit` output) throws a clear error (documented gap).

Verified vs MATLAB R2025b (parity `ddencmp.json` → OK): den/wv
`[1 2 3 8 3 2 1 2]` → thr=2.137919772574, sorh='s', keepapp=1; cmp/wv same
→ thr=0.707106781187, sorh='h'; den/wv odd-length `[1 2 3 4 5]` →
thr=1.880854323469. Guards: `ddencmp_test.cpp` (4 TEST_F: denoise /
odd-length / compress / wp-throws), `known_bugs_test.cpp` (`Ddencmp`,
promoted live); smoke `ddencmp_smoke.m`. **Closes the wentropy/ddencmp
cluster.**

## References
- `src/toolboxes/wavelet/src/denoise/denoise.cpp` (`ddencmp`),
  `.../include/numkit/wavelet/denoise/denoise.hpp` (`DdencmpResult`),
  `src/bundle/src/register/wavelet/denoise/denoise_reg.cpp` (`ddencmp_reg`).
- `tools/parity/specs/ddencmp.json`.
- shipped + reused: `dwt` (db1), `median_abs`
- related: wavelet/wentropy.md (the entropy half, also fixed)
- deferred: `'wp'` (wavelet-packet) defaults + the `crit` output
- MATLAB `doc ddencmp`
