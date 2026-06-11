# signal.ellipord — bandstop case throws

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (missing option)
- **Kind:** stub
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
`toolboxes/signal/src/filter_design/iir_designs.cpp:596` (`case 3:`) throws;
comment at `:508` notes it needs the digital→analog bandstop transform
branch that the other three response types use.

## Suggested fix
Add the bandstop frequency-transform branch (analog prototype order
estimate with the bandstop pre-warp), mirroring the bandpass path with the
reciprocal mapping. Validate `n` and `Wn` against MATLAB. Moderate.

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 39),
  `toolboxes/signal/src/filter_design/iir_designs.cpp` (`ellipord` `case 3`).
- The bandstop branch now computes the analog passband-edge ratio as the
  **reciprocal of the bandpass→lowpass map**: `WA[i] = WS[i]·(WP1-WP2) /
  (WS[i]² - WP1·WP2)` for each prewarped stopband edge, then the worst-case
  edge is selected by `findElliporderImpl` (`WAmin = min|WA|`, `k = 1/WAmin`,
  `N = ceil(K(k²)·K(1-k1²)/(K(1-k²)·K(k1²)))`). This mirrors the bandstop
  transform already used by numkit's `buttord`/`cheb1ord`/`cheb2ord`
  (`normaliseOrd`, `Ω_LP = Bw·Ω/(Ω²-Ω0²)`) — clean-room, derived from the
  standard elliptic-order / frequency-transform identities, not MATLAB source.
  `Wn` returns the original passband edges (digital `wp`, analog `WP`).
- Verified vs MATLAB R2025b: `ellipord([0.1 0.6],[0.2 0.5],3,40)` → n=4,
  Wn=[0.1 0.6]; `([0.15 0.55],[0.25 0.45],1,60)` → n=5;
  `([0.2 0.7],[0.3 0.6],2,50)` → n=4; analog `([100 600],[200 500],3,40,'s')`
  → n=5, Wn=[100 600]. low/high/bandpass unchanged.
- Live guard: `toolboxes/signal/tests/ellipord_test.cpp`
  (`BandstopDigital`, `BandstopAnalog`) + flipped
  `SignalKnownBug.EllipordBandstop` live. Parity:
  `tools/parity/specs/signal_ellipord.json` extended (correctness=OK). Smoke:
  `toolboxes/signal/tests/smoke/ellipord_smoke.m`.

## References
- `toolboxes/signal/src/filter_design/iir_designs.cpp` (`ellipord` case 3)
- MATLAB `doc ellipord`
- sibling `buttord`/`cheb1ord`/`cheb2ord` already handle bandstop (`normaliseOrd`)
