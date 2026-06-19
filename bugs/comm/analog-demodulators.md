# comm.amdemod / ssbdemod / mskdemod — missing (pmdemod/fmdemod done)

- **Status:** 🔴 OPEN (partial — `pmdemod` + `fmdemod` fixed 2026-06-19)
- **Severity:** P2 (missing functions — asymmetry)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Progress (2026-06-19)
**`pmdemod` and `fmdemod` are implemented** (`analog.cpp`), the two phase-
based demodulators (no filtering needed):
- `pmdemod(y,fc,fs,phasedev[,ini_phase])` = `angle(hilbert(y)·exp(−j·(2π·fc·t
  + ini_phase))) / phasedev`.
- `fmdemod(y,fc,fs,freqdev[,ini_phase])` = `[0; diff(unwrap(φ))]·fs/(2π·
  freqdev)` with `φ` the same down-converted analytic phase.

They depend on a **correct length-N `hilbert`** — fixing them surfaced and
required bugs/signal/hilbert-nonpow2 (the old hilbert zero-padded to
nextPow2 and gave ~8 % demod errors). Parity OK vs MATLAB R2025b
(`pmdemod.json` / `fmdemod.json`): pm recovers `cos(2πt)` to ~2e-8; fm
`mf=[0, 0.99774, 0.99189, …]`. Guards: `analog_demod_test.cpp`; smoke
`analog_demod_smoke.m`.

**Still OPEN:** `amdemod`, `ssbdemod` (need the exact MATLAB lowpass — a
5th-order Butterworth at Fc; numkit's `butter`+`filter` must match MATLAB's
default bit-for-bit, not yet verified), and `mskdemod` (coherent/Viterbi).

## Symptom
The analog **modulators** are all implemented; the **demodulator** inverses
were missing. `pmdemod`/`fmdemod` are now done; `amdemod`/`ssbdemod`/
`mskdemod` remain.

## Repro
```matlab
ammod(1,10,100)   % OK        amdemod(1,10,100)   % undefined function 'amdemod'
fmmod(1,10,100,2) % OK        fmdemod(1,10,100,2) % undefined function 'fmdemod'
pmmod(1,10,100,2) % OK        pmdemod(1,10,100,2) % undefined function 'pmdemod'
ssbmod(1,10,100)  % OK        ssbdemod(1,10,100)  % undefined function 'ssbdemod'
mskmod([1 0 1],8) % OK        mskdemod([1 0 1],8) % undefined function 'mskdemod'
```

## Root cause
Only the `*mod` functions were registered (`src/toolboxes/comm/src/modulation/
analog.cpp` + `library.cpp`); the `*demod` inverses were never added.

## Suggested fix (remaining)
- `amdemod`: multiply by carrier, low-pass. **Gotcha:** MATLAB's default is
  a 5th-order Butterworth at `Fc·2/Fs`; a naive `butter(5,…)+filter+×2`
  reconstruction did NOT match MATLAB (off by ~2×) — numkit's `butter`/
  `filter` must be checked against MATLAB's exact default first.
- `ssbdemod`: coherent multiply + the same low-pass.
- `mskdemod`: MSK coherent/Viterbi demod.
Done: `pmdemod`/`fmdemod` (phase-based, no filter — see Progress above).

## References
- `src/toolboxes/comm/src/modulation/analog.cpp`, `library.cpp`
- shipped pairs for reference: pskmod/pskdemod, qammod/qamdemod
- MATLAB `doc amdemod` etc.
