# stats.autocorr / parcorr / crosscorr — Econometrics correlation fns missing

- **Status:** ✅ FIXED (2026-06-18) — autocorr + crosscorr + parcorr all implemented
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
The sample-correlation trio from the Econometrics Toolbox is not
registered: `autocorr` (sample autocorrelation function, ACF), `parcorr`
(partial autocorrelation, PACF), and `crosscorr` (sample cross-correlation).
These are distinct from the Signal-Toolbox `xcorr` — they normalise to a
correlation (lag-0 = 1), default `NumLags = min(20, N-1)`, and return
confidence bounds.

## Repro
```matlab
ac = autocorr([1 2 3 2 1 2 3 2 1]);   % default NumLags
% MATLAB: numel(ac)=9, ac(1)=1, ac(2)=0.0202020202020202
% numkit: Error — VM: undefined function 'autocorr'
parcorr([1 2 3 2 1 2 3 2 1])          % MATLAB: pc(1)=1 ; undefined in numkit
crosscorr([1 2 3 4],[4 3 2 1],'NumLags',2)
% MATLAB: numel=5, zero-lag (index 3) = -1 ; undefined in numkit
```

## Root cause
Not implemented. numkit has `xcorr` (raw/biased/unbiased cross-correlation)
but not the correlation-normalised ACF/PACF/CCF with the econometrics
conventions (lag-0 normalisation, default lag count, ±1.96/√N bounds).

## Fix (2026-06-18) — autocorr + crosscorr
Implemented in `src/toolboxes/stats/src/descriptive/sample_corr.cpp`
(`numkit::stats::autocorr` / `crosscorr`), registered under `descriptive`:
- `autocorr(y, NumLags, NumSTD)`: biased `acf(k) = c(k)/c(0)`,
  `c(k) = (1/N) Σ (y_t-ȳ)(y_{t+k}-ȳ)`; default `NumLags = min(20, N-1)`;
  bounds `±NumSTD/√N`, **NumSTD default 2** (MATLAB R2025b uses 2, not the
  1.96 originally noted here). Returns `[acf, lags, bounds]`.
- `crosscorr(y1, y2, NumLags, NumSTD)`: `xcf(k) = [(1/N) Σ (y1_t-ȳ1)
  (y2_{t+k}-ȳ2)] / √(c1(0)·c2(0))`, `k = -NumLags..NumLags`.

Verified vs MATLAB R2025b (parity `autocorr.json` + `crosscorr.json` → OK):
`autocorr([1 2 3 2 1 2 3 2 1])` acf(2)=0.020202, acf(3)=−0.800505,
bounds=±0.666667; `crosscorr([1 2 3 4],[4 3 2 1],'NumLags',2)` =
`[0.3 −0.25 −1 −0.25 0.3]`. Guards: `sample_corr_test.cpp`; smoke
`sample_corr_smoke.m`.

## Fix (2026-06-18) — parcorr
Implemented `numkit::stats::parcorr` in the same file. **Matches MATLAB's default
`parcorr` Method (OLS), NOT Durbin-Levinson**: for each lag `k` it fits the AR(k)
model `y_t = c + Σ_{j=1..k} φ_j·y_{t-j}` by least squares on `t = k+1..N` and
returns `PACF(k) = φ_k` (lag-0 = 1; bounds `±NumSTD/√N`). Solved via the normal
equations + a small Gaussian-elimination solver — reproduces MATLAB's QR to full
precision on **well-conditioned** lags (`N-k ≥ k+1`). Deeper, rank-deficient lags
are numerically unstable in both engines (MATLAB warns "rank deficient"); numkit
leaves a near-singular lag at 0 rather than emitting QR-implementation-specific
noise — these lags aren't statistically meaningful anyway.

Verified vs MATLAB R2025b (parity `parcorr.json` → OK) on a well-conditioned
N=16 series: `pacf = [1, 0.55, 0.241249, 1.000776, 0.442451, 0.677598]` (note
PACF can exceed 1 under OLS), `bounds = ±0.5`. Guard:
`sample_corr_test.cpp` (`ParcorrOLS`); smoke `sample_corr_smoke.m`.
(MATLAB's `'Method','yule-walker'` option — Durbin-Levinson on the ACF — is a
separate spelling not wired here; the default OLS is what `parcorr(y)` uses.)

## References
- `src/toolboxes/stats/src/descriptive/sample_corr.cpp`,
  `.../include/numkit/stats/descriptive/descriptive.hpp`,
  `src/bundle/src/register/stats/descriptive/sample_corr_reg.cpp`.
- `tools/parity/specs/autocorr.json`, `tools/parity/specs/crosscorr.json`.
- for parcorr: numkit ships `qr` / `\` (least-squares) + `levinson`/`aryule`.
- MATLAB `doc autocorr`, `doc parcorr`, `doc crosscorr` (Econometrics TB)
