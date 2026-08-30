# signal.instfreq / signal.instbw — wrong values (broken)

- **Status:** 🔴 OPEN
- **Severity:** P1 (wrong result)
- **Kind:** bug
- **Found:** 2026-06 via DEEP-PROBE (also long-noted as "instfreq BROKEN")

## Symptom
`instfreq` returns negative / out-of-range values that do not track the
true instantaneous frequency. `instbw` (instantaneous bandwidth) is
similarly off.

## Repro
```matlab
fs = 1000; t = (0:1/fs:1-1/fs)';
x = chirp(t, 10, 1, 40);          % frequency sweeps 10 -> 40 Hz
ifr = instfreq(x, fs);
% numkit: ifr(1) = -66.58,  ifr(end) = -126.5     (negative garbage)
% MATLAB: ifr(1) =  13.96,  ifr(end) =   38.46    (tracks the sweep)

fs=100; tt=(0:1/fs:1-1/fs)'; y=cos(2*pi*10*tt)+0.5*cos(2*pi*25*tt);
mean(instbw(y,fs))
% numkit: 4.9074    MATLAB: 31.4126
```
(`snr`/`sinad`/`thd`/`sfdr` were checked alongside and are CORRECT — a real
3rd-harmonic gives thd = -20 dB on both engines; only pure-tone noise-floor
values differ at float level, which is expected.)

## Root cause
Unknown — the analytic-signal phase-derivative path appears wrong (sign /
scaling / unwrap). `src/toolboxes/signal/src/.../instfreq*` (time_frequency or
measurements). The default `instfreq` method is the spectrogram-based
first conditional spectral moment; numkit may be using the Hilbert
phase-derivative incorrectly, or with the wrong fs scaling.

## Recheck after the hilbert non-pow2 fix (2026-06-19)
The chirp repro length is 1000 (non-pow2), so I re-ran it after fixing
bugs/signal/hilbert-nonpow2. The values **changed** (ifr(1) went −66.58 →
−19.12) but are **still negative / wrong** — so numkit's `instfreq` IS on
the Hilbert phase-derivative path (it moved with the hilbert fix), but it
has its own sign/scaling defect AND, more fundamentally, MATLAB's
**default** `instfreq(x,fs)` is the spectral-moment TFD method, not the
Hilbert derivative. So this is a **method mismatch**, not the hilbert bug:
even a perfectly-signed Hilbert derivative won't match MATLAB's default.
**Real fix = reimplement the default as the first conditional spectral
moment over the spectrogram/pspectrum TFD** (then `instbw` = 2nd central
moment). Matching MATLAB's TFD defaults (window/overlap) to parity tol is
the work. Substantial — not the quick win the hilbert recheck hoped for.

## Suggested fix
Reconcile with MATLAB's definition: default `instfreq(x,fs)` is the
first conditional spectral moment over the `pspectrum`/spectrogram TFD
(NOT a raw Hilbert phase derivative). Decide which definition to implement,
fix the sign/scaling, validate against a linear chirp (should rise linearly
from f0 to f1). Same TFD underlies `instbw` (2nd central moment). Medium.

## References
- `src/toolboxes/signal/src/.../instfreq*`, `instbw*`
- MATLAB `doc instfreq` (note: default is the spectral-moment method)
