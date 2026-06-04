# signal.resample — wrong output values

- **Status:** 🔴 OPEN
- **Severity:** P1 (wrong result)
- **Kind:** bug
- **Found:** 2026-06 via DEEP-PROBE
- **Note:** part of the deferred MULTIRATE gap (decimate/resample/interp
  delay compensation), recorded here with a concrete repro.

## Symptom
`resample(x, p, q)` returns values that do not track the input — the
antialiasing/polyphase filtering (and edge/delay compensation) is wrong.

## Repro
```matlab
resample([1 2 3 4 5 6], 3, 2)
% numkit: [0.00448 0.01466 -0.07709 0.11110 0.80764 1.51393 2.17249 2.82780 3.49960]
%         (sum 10.87 — ramps up from ~0, lags the signal)
% MATLAB: [1.00061 1.80791 2.16807 3.00182 3.94099 3.96567 5.00303 6.56811 4.24029]
%         (sum 31.70 — tracks the 1..6 ramp at the new rate)
```

## Root cause
The resample implementation's polyphase FIR (Kaiser-window antialiasing
filter design + the group-delay/edge compensation MATLAB applies) is
incorrect, so the resampled samples are mis-scaled and mis-aligned.

## Suggested fix
Re-implement per MATLAB: design the antialiasing FIR with `fir1`/Kaiser
(`firls`-style), upsample by p / downsample by q via `upfirdn`, then
compensate the filter group delay and trim to `ceil(length(x)*p/q)`
samples (MATLAB's exact length + edge handling). Large/risky — this is the
known multirate gap (shared with `decimate` / `interp`). Validate the full
output vector + length vs MATLAB.

## References
- `libs/signal/src/.../resample*`, `upfirdn`
- shipped: `upfirdn`, `fir1`, `kaiser`, `upsample`, `downsample`
- MATLAB `doc resample`
