# signal.periodogram — confidence-interval output (pxxc) unsupported

- **Status:** ✅ FIXED (2026-06-18) — 'ConfidenceLevel' parsed + chi-square pxxc 3rd output
- **Severity:** P2 (missing output / option)
- **Kind:** missing-output
- **Found:** 2026-06-04 via DEEP-PROBE (multi-output sweep)

## Symptom
`[pxx, f, pxxc] = periodogram(..., 'ConfidenceLevel', p)` throws. MATLAB
returns `pxxc`, the two-column confidence interval for the PSD estimate.
(pwelch has the same gap — see deferred note.)

## Repro
```matlab
[pxx, f, pxxc] = periodogram([1 2 3 4 5 6 7 8], [], [], 1, 'ConfidenceLevel', 0.95)
% numkit: Error — Cannot convert double to scalar
% MATLAB: pxxc is an (nf × 2) matrix of [lower, upper] PSD bounds
```

## Root cause
The `'ConfidenceLevel'` name-value pair is not parsed (the adapter treats the
value as a positional numeric → "Cannot convert double to scalar"), and the
`pxxc` 3rd output is not computed.

## Fix (2026-06-18)
Black-box probing of MATLAB R2025b pinned the exact rule (no MATLAB source
read — IP-clean, the CI is a standard textbook chi-square method): each PSD
bin is chi-square distributed with `v` degrees of freedom, and **MATLAB uses
integer `v`, not a window EDOF** — `v = 2` for the general (folded) interior
bins, `v = 1` for the purely-real DC bin and (when `nfft` is even) the Nyquist
bin. A complex signal's bins are all `v = 2`. The interval is
`pxxc = pxx .* v ./ chi2inv([1-α/2, α/2], v)`, `α = 1 - ConfidenceLevel`, with
the closed forms `chi2inv(q,2) = -2·ln(1-q)` and `chi2inv(q,1) = (√2·erfinv(q))²`
(`erfinv` from the math layer — no cross-toolbox dep on stats). Default level
0.95 when a 3rd output is requested without the name-value.

New compute fn `numkit::signal::periodogramConf(Pxx, conf, realInput, nfftEven,
mr)` (nf×2 `[lower, upper]`); `periodogram_reg` now scans for the
`'ConfidenceLevel'` pair (case-insensitive), tolerates `[]` placeholders +
string flags in the positional slots, and emits `pxxc` as `outs[2]`.

Verified vs MATLAB R2025b (parity `tools/parity/specs/periodogram_pxxc.json`
→ correctness=OK): `[pxx,f,pxxc]=periodogram((1:8)',rectwin(8),8,1,
'ConfidenceLevel',0.95)` gives lower `[32.246 7.404 2.169 1.270 0.398]`, upper
`[164957.8 1078.8 316.0 185.1 2036.5]`, DC/Nyquist ratios 0.19905, interior
0.27108. Guards: `spectral_test.cpp` (`PeriodogramConfidenceIntervalValues`,
`PeriodogramConfidenceDefaultLevel`) + `known_bugs_test.cpp` (`PeriodogramPxxc`,
promoted live); smoke `periodogram_pxxc_smoke.m`.

`pwelch`/`cpsd` CI (deferred-gap R) is still open — those need the averaged
EDOF `v = 2·k` for `k` segments, not the single-segment rule above.

## References
- `src/toolboxes/signal/src/spectral_analysis/periodogram_pwelch.cpp`
  (`periodogramConf`), `.../periodogram_pwelch.hpp`,
  `src/bundle/src/register/signal/spectral_analysis/periodogram_pwelch_reg.cpp`
  (`periodogram_reg` option parse + 3rd output).
- `tools/parity/specs/periodogram_pxxc.json`.
- MATLAB `doc periodogram` (ConfidenceLevel / pxxc).
