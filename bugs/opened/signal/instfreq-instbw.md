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
clear;
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

## Progress 2026-09-06 (portion 21): 'hilbert' method FIXED exactly; tfmoment reverse-engineered

- The stale "conjugate hilbert" comment was wrong: numkit's hilbert
  MATCHES MATLAB (probed at signal edge and deep interior, phase delta
  +0.1885 identical). The real defect was the leftover NEGATION of the
  phase delta — correct positive frequencies came out negative.
  instfreq is now MATLAB's verbatim `fs/(2π)·diff(unwrap(angle(z)))`
  and matches 'Method','hilbert' to 6 decimals on the chirp probe
  (16.761585 / 24.967185 / 27.511913). 'Method' name-value added;
  'tfmoment' errors honestly. Live guard: InstfreqHilbertMethodExact.
- DEFAULT-method reverse-engineering data (for the pspectrum port):
  [P,F,T]=pspectrum(x,t,'spectrogram') on chirp N=1000,fs=1000 gives
  nF=1024, nT=30, F=linspace(0,fs/2,1024) (dF=500/1023 — an
  INTERPOLATED grid, not a raw FFT grid), T(1)=0.0625 dT=0.031.
  Geometry across N: wlen=round(N/8) & hop≈round(wlen/4)+1 for
  N≤2000 (63/16, 125/31, 250/61); N=12345 switches regime (wlen=193,
  hop=47) — the large-N rule still unknown. instfreq = Σ F·P / Σ P
  over the COLUMNS of the LINEAR power spectrogram (verified exact);
  instbw = sqrt(second central moment)·(sqrt(4π)-related scale) and is
  tfmoment-ONLY (no hilbert method exists). Porting pspectrum's
  window/heuristic/interpolation pipeline is the remaining work.

## Reverse-engineering addendum (portion 23, 2026-09-06)

- pspectrum.m DELEGATES the spectrogram to
  signalanalyzer.internal.ZoomSpectrum — P-CODE, not readable. The .m
  only sets: Npoints=4096 auto grid, kbeta =
  convertLeakageToBeta(Leakage) with doc-stated default Leakage 0.5,
  half-sample time corrections (t1 − 0.5/Fs, t2 + 0.5/Fs).
- Geometry ladder VERIFIED (fs=1000, 14 lengths): regime N<=2000:
  wlen = ceil(N/8) (all 8 probed points), nT = 30 for N>=777 with
  hop = ceil((N−wlen)/29) (verified 777/1000/1500/2000 and the
  sub-30-nT small-N corner 100/200/300/500 with nT = 23/26/28/29 —
  hop formula holds there too, nT rule for the corner still unknown);
  N>=3000 regime switches (wlen non-monotone: 188,157,250,193,157,391
  at N=3000..50000; nT ~ 2^k-ish 63,129,129,260,524,524).
- Manual kaiser(wlen,beta) STFT (zero-padded edge windows, centered at
  wlen/2 + k*hop, nfft 4096, power interpolated onto linspace(0,fs/2,
  1024)) does NOT reproduce ifr within 1.2 Hz for ANY beta in 5..12 —
  structural unknowns remain (exact centering/padding or reassign
  internals). STOPPED fitting per clean_code: an opaque-pipeline
  match-the-probe implementation would be worse than this deferral.
  Path forward if ever needed: binary-probe ZoomSpectrum outputs
  (single-impulse/single-tone inputs) to pin its window alignment.

## Suggested fix
Reconcile with MATLAB's definition: default `instfreq(x,fs)` is the
first conditional spectral moment over the `pspectrum`/spectrogram TFD
(NOT a raw Hilbert phase derivative). Decide which definition to implement,
fix the sign/scaling, validate against a linear chirp (should rise linearly
from f0 to f1). Same TFD underlies `instbw` (2nd central moment). Medium.

## References
- **Guard:** `InstfreqHilbertMethodExact` (live, the fixed half) + `DISABLED_InstfreqTfmomentDefault` (the pspectrum-pending half)
- `src/toolboxes/signal/src/.../instfreq*`, `instbw*`
- MATLAB `doc instfreq` (note: default is the spectral-moment method)
