# comm.mskdemod — missing (pmdemod/fmdemod/amdemod/ssbdemod done)

- **Status:** ✅ FIXED (2026-06-19) — all five demodulators implemented
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Progress (2026-06-19)
**Four of the five demodulators are implemented** (`analog.cpp`):
- `pmdemod(y,fc,fs,phasedev[,ini_phase])` = `angle(hilbert(y)·exp(−j·(2π·fc·t
  + ini_phase))) / phasedev`.
- `fmdemod(y,fc,fs,freqdev[,ini_phase])` = `[0; diff(unwrap(φ))]·fs/(2π·
  freqdev)`, `φ` the down-converted analytic phase.
- `amdemod(y,fc,fs[,ini_phase[,carramp]])` = `2·filtfilt(butter(5,fc·2/fs),
  y·cos(2π·fc·t+ini_phase)) − carramp` (coherent detection).
- `ssbdemod(y,fc,fs[,ini_phase])` = the same coherent detector (no carramp).

pm/fm needed a **correct length-N `hilbert`** — they surfaced and required
bugs/signal/hilbert-nonpow2 (the old hilbert zero-padded to nextPow2, ~8 %
errors). am/ssb use numkit's `butter`+`filtfilt`: `butter`/`filter` are
bit-exact with MATLAB; `filtfilt` matches in the **interior** (mid-signal
to ~3e-10) and differs ~3e-6 near the **edges** (filtfilt edge conditions —
a separate minor item). Parity OK vs MATLAB R2025b (`pmdemod.json`,
`fmdemod.json`, `amdemod.json`, all interior). Guards:
`analog_demod_test.cpp`; smoke `analog_demod_smoke.m`.

**mskdemod (2026-06-19):** coherent **differential** demod — `bit_k =
(Σ_{within symbol k} angle(y[n]·conj(y[n-1])) > 0)`, summing the per-sample
phase increments over the symbol's interior pairs. Because the decision uses
phase *increments* it is invariant to a constant phase rotation and robust
to noise (`ini_phase` only sets the returned final-phase state). numkit's
`mskmod` is bit-exact with MATLAB, and this rule reproduces MATLAB R2025b
`mskdemod` exactly (verified on 20-bit random clean + noisy: full match,
including the last bit, which uses the within-symbol pairs rather than a
boundary-to-boundary difference). Returns `[z, phaseout]`, bits in the input
orientation. **Non-differential** `'nondiff'` path **deferred** — numkit's
`mskmod` has no non-diff path either, so it cannot round-trip; rejected with
a clear error. Guards: `mskdemod_test.cpp`, `known_bugs_test.cpp`
(`MskDemodExists`, promoted live); smoke `mskdemod_smoke.m`; parity
`mskdemod.json` → OK.

## Symptom
The analog **modulators** are all implemented; the **demodulator** inverses
were missing. `pmdemod`/`fmdemod`/`amdemod`/`ssbdemod` are now done; only
`mskdemod` remains.

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

## Fix summary
All five demodulators implemented (`analog.cpp`):
`pmdemod`/`fmdemod` (phase) + `amdemod`/`ssbdemod` (coherent
`2·filtfilt(butter(5,Fc·2/Fs), y·cos)` — note MATLAB uses **zero-phase
filtfilt**, not a causal `filter`, and the `×2` recovers the suppressed
carrier).

## References
- `src/toolboxes/comm/src/modulation/analog.cpp`, `library.cpp`
- shipped pairs for reference: pskmod/pskdemod, qammod/qamdemod
- MATLAB `doc amdemod` etc.
