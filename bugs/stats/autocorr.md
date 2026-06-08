# stats.autocorr / parcorr / crosscorr — Econometrics correlation fns missing

- **Status:** 🔴 OPEN
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

## Suggested fix
- `autocorr(y)`: `acf(k) = c(k)/c(0)`, `c(k)=mean((y-ȳ)(y_{+k}-ȳ))`;
  default `NumLags = min(20, N-1)`. Can reuse `xcorr(y,'biased')` then
  normalise. Optional outputs: lags, bounds.
- `parcorr(y)`: PACF via Durbin-Levinson recursion on the ACF (numkit
  already has `levinson`/`aryule` machinery to lean on).
- `crosscorr(y1,y2)`: normalise the cross-covariance by `√(c1(0)·c2(0))`.
All small-medium and share one helper. Verify ACF/PACF vs MATLAB on a short
deterministic series (lag-0 must be exactly 1).

## References
- new file under `toolboxes/stats/src/...` (or reuse `toolboxes/signal` xcorr/levinson)
- MATLAB `doc autocorr`, `doc parcorr`, `doc crosscorr` (Econometrics TB)
