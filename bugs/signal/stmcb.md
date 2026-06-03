# signal.stmcb — function missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`stmcb` (Steiglitz–McBride iteration — IIR model from impulse/output) is not
registered.

## Repro
```matlab
[b, a] = stmcb([1 0.5 0.25 0.125 0.0625], 1, 1)
% numkit: Error — VM: undefined function 'stmcb'
% MATLAB: a = [1 -0.5]   (recovers the 1/(1-0.5 z^-1) system)
```

## Root cause
Not implemented.

## Suggested fix
Steiglitz–McBride: initialise `a` (e.g. via `prony`), then iterate —
prefilter input/output by `1/a`, solve the linear LS for `[b, a]`, repeat
(default ~5 iterations). numkit already has `prony`, `filter`, and LS
solves. Medium. Outputs `[b, a]`. Validate vs MATLAB on a known IIR
impulse response.

## References
- new file under `libs/signal/src/...`
- shipped: `prony`, `levinson`, `lpc`, `filter`
- MATLAB `doc stmcb`
