# wavelet.wentropy — coefficient entropy measure missing

- **Status:** ✅ FIXED (2026-06-19) — closed-form additive entropy
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

> Split note: this entry originally also tracked `ddencmp` (default
> denoising/compression parameters), which needs the finest-detail MAD noise
> estimate. That piece is its own entry now — see bugs/wavelet/ddencmp.md
> (still OPEN).

## Symptom
`wentropy` (wavelet entropy / cost of a coefficient vector) is not
registered.

## Repro
```matlab
wentropy([1 2 3 4], 'shannon')
% MATLAB: -69.6816181963   (-Σ s²·log(s²) over nonzero s)
% numkit: Error — VM: undefined function 'wentropy'
```

## Fix (2026-06-19)
Implemented `numkit::wavelet::wentropy` (`denoise.cpp`, next to
`wthresh`/`wdenoise`). `E = wentropy(X, T[, P])` is the additive entropy
"cost" over the elements of `X`:
- `'shannon'`: `−Σ sᵢ²·log(sᵢ²)` (a `sᵢ=0` term contributes 0).
- `'log energy'`: `Σ log(sᵢ²)` over nonzero `sᵢ`.
- `'threshold'` (P): `#{i : |sᵢ| > P}`.
- `'sure'` (P): `n − 2·#{i : |sᵢ| ≤ P} + Σ min(sᵢ², P²)`.
- `'norm'` (P ≥ 1): `Σ |sᵢ|ᴾ`.

Type is case-insensitive; `'norm'` requires `P ≥ 1` (throws otherwise);
unknown types throw.

Verified vs MATLAB R2025b (parity `wentropy.json` → OK) on
`x = [0.5 −0.3 0.8 0 −0.1 0.2]`: shannon=1.023719175595,
log energy=−12.064573083256, threshold(0.2)=3, sure(0.2)=0.17,
norm(1.5)=1.354477406346; and the repro shannon([1 2 3 4])=−69.6816181963.
Guards: `wentropy_test.cpp` (6 TEST_F: shannon / log-energy / threshold /
sure / norm / errors), `known_bugs_test.cpp` (`WentropyShannon`, promoted
live); smoke `wentropy_smoke.m`.

## References
- `src/toolboxes/wavelet/src/denoise/denoise.cpp` (`wentropy`),
  `.../include/numkit/wavelet/denoise/denoise.hpp`,
  `src/bundle/src/register/wavelet/denoise/denoise_reg.cpp` (`wentropy_reg`).
- `tools/parity/specs/wentropy.json`.
- related: wavelet/ddencmp.md (the default-parameter half, still open)
- MATLAB `doc wentropy`
