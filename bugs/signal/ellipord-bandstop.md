# signal.ellipord — bandstop case throws

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing option)
- **Found:** 2026-06 via signal.* DEEP-PROBE sweep (long-standing KNOWN GAP)

## Symptom
`ellipord` rejects the bandstop configuration (passband edges straddling
the stopband). low/high/bandpass work.

## Repro
```matlab
[n, Wn] = ellipord([0.1 0.6], [0.2 0.5], 3, 40)   % bandstop edges
% numkit: Error — ellipord: bandstop case not yet supported
```

## Root cause
`libs/signal/src/filter_design/iir_designs.cpp:596` (`case 3:`) throws;
comment at `:508` notes it needs the digital→analog bandstop transform
branch that the other three response types use.

## Suggested fix
Add the bandstop frequency-transform branch (analog prototype order
estimate with the bandstop pre-warp), mirroring the bandpass path with the
reciprocal mapping. Validate `n` and `Wn` against MATLAB. Moderate.

## References
- `libs/signal/src/filter_design/iir_designs.cpp:508,596`
- MATLAB `doc ellipord`
- sibling `buttord`/`cheb1ord`/`cheb2ord` already handle bandstop
