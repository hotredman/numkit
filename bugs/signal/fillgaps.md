# signal.fillgaps — function missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via signal.* DEEP-PROBE sweep

## Symptom
`fillgaps` (autoregressive interpolation of missing `NaN` samples) is not
registered.

## Repro
```matlab
fillgaps([1 2 NaN 4 5])
% numkit: Error — VM: undefined function 'fillgaps'
% MATLAB: 1 2 3 4 5  (AR-model interpolation of the gap)
```

## Root cause
Not implemented.

## Suggested fix
MATLAB `fillgaps(x)` / `fillgaps(x, maxlen)` / `fillgaps(x, maxlen, order)`
fits a forward+backward autoregressive model over the surrounding samples
and predicts the missing values. Needs Levinson/AR machinery (numkit already
has `arburg`/`aryule`/`levinson`). Moderate; validate the AR order defaults
and the bidirectional blend against MATLAB.

## References
- new file under `toolboxes/signal/src/smoothing/` or `.../measurements/`
- MATLAB `doc fillgaps`
- related shipped: `arburg`, `aryule`, `levinson`
