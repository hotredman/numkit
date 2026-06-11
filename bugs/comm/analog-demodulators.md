# comm.amdemod / fmdemod / pmdemod / ssbdemod / mskdemod — missing (mods exist)

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing functions — asymmetry)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
All five analog **modulators** are implemented but **none** of their
matching **demodulators** are — you can modulate but not demodulate.

## Repro
```matlab
ammod(1,10,100)   % OK        amdemod(1,10,100)   % undefined function 'amdemod'
fmmod(1,10,100,2) % OK        fmdemod(1,10,100,2) % undefined function 'fmdemod'
pmmod(1,10,100,2) % OK        pmdemod(1,10,100,2) % undefined function 'pmdemod'
ssbmod(1,10,100)  % OK        ssbdemod(1,10,100)  % undefined function 'ssbdemod'
mskmod([1 0 1],8) % OK        mskdemod([1 0 1],8) % undefined function 'mskdemod'
```

## Root cause
Only the `*mod` functions were registered (`toolboxes/comm/src/modulation/
analog.cpp` + `library.cpp`); the `*demod` inverses were never added.

## Suggested fix
Each demod is the documented inverse of its mod:
- `amdemod`: multiply by carrier, low-pass (or envelope for non-suppressed).
- `fmdemod`: differentiate the phase of the analytic signal / discriminator.
- `pmdemod`: phase of the analytic signal minus carrier.
- `ssbdemod`: coherent multiply + low-pass.
- `mskdemod`: MSK coherent/Viterbi demod.
The forward `*mod` paths already exist to mirror. Medium overall; do as a
cluster (round-trip mod→demod gtests). Validate vs MATLAB.

## References
- `toolboxes/comm/src/modulation/analog.cpp`, `library.cpp`
- shipped pairs for reference: pskmod/pskdemod, qammod/qamdemod
- MATLAB `doc amdemod` etc.
