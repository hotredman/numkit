# signal.obw — wrong occupied-bandwidth value + missing outputs

- **Status:** 🔴 OPEN
- **Severity:** P1 (wrong value) + P2 (missing outputs)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (spectral-measurement sweep)

## Symptom
1. **Value** — `obw(x, fs)` (the 99% occupied bandwidth) diverges from MATLAB.
2. **Outputs** — MATLAB `obw` returns up to four outputs
   `[bw, flo, fhi, power]` (bandwidth + lower/upper band edges + power in the
   band); numkit emits only `bw` and throws "Too many output arguments" for
   the rest.

## Repro
```matlab
fs = 1000; t = (0:fs-1)/fs;
x = sin(2*pi*100*t) + 0.5*sin(2*pi*200*t);   % deterministic two-tone

obw(x, fs)
% numkit: 108.7721
% MATLAB: 100.9688

[bw, flo, fhi, p] = obw(x, fs)
% numkit: Error — Too many output arguments
% MATLAB: bw=100.9688, flo=99.5062, fhi=200.4750, p=0.6188
```

## Root cause
Two parts. The single-output value already differs (~8%), so numkit's
occupied-bandwidth integration (PSD estimate and/or the 99%-power band
search) does not match MATLAB's. Separately, the `obw_reg` adapter emits
only `outs[0]` — the band edges and in-band power are not threaded to
`nargout`.

## Suggested fix
- Reconcile the PSD estimate + the 99% cumulative-power band search against
  MATLAB (`doc obw`: default 99% via the periodogram PSD with a Kaiser
  window; band edges where the cumulative power reaches 0.5% and 99.5%).
- Thread `nargout` and emit `flo`/`fhi`/`power` from the same integration.
  Validate `bw`, `flo`, `fhi`, `power` against MATLAB on a deterministic
  multi-tone signal. Moderate — get the value right first, then wire outputs.

## References
- `src/toolboxes/signal/src/.../obw*` (+ the PSD helper it uses)
- MATLAB `doc obw`
