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
clear;
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
has `arburg`/`aryule`/`levinson`).

## ⚠️ Attempt + DEFER (2026-06-18)
The obvious reconstruction — **separate forward/reverse `arburg` extrapolation +
a linear blend across the gap** — does NOT match MATLAB. Probed `fillgaps(x,[],4)`
(explicit order 4) on `sin(2π·0.05·n)` with a 3-sample gap: MATLAB returns
`[0.1315, -0.0036, -0.2949]`, but forward/reverse-extrapolate-and-blend gives
`[0.0004, -0.3095, -0.5888]` — and even the forward extrapolation alone differs
from MATLAB's first filled value. So MATLAB's `fillgaps` is **not** "extrapolate
each side then blend": it appears to treat the missing samples as unknowns in a
least-squares system over the AR prediction-error equations spanning the gap
(forward + reverse fits combined), plus the default `'aic'` order selection.
Reconstructing that exactly (the AR-interpolation LSQ formulation + AIC order +
`maxlen` default) needs more than black-box probing — **deferred**. Don't repeat
the extrapolate-and-blend dead-end.

## References
- **Guard:** `DISABLED_Fillgaps`
- new file under `src/toolboxes/signal/src/smoothing/` or `.../measurements/`
- MATLAB `doc fillgaps`
- related shipped: `arburg`, `aryule`, `levinson`
