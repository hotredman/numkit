# signal.stmcb — function missing

- **Status:** ✅ FIXED (2026-06-18) — impulse-response form `stmcb(h, nb, na, niter)`
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

## Fix (2026-06-18)
Implemented `numkit::signal::stmcb(h, nb, na, niter)` in
`src/toolboxes/signal/src/spectral_analysis/signal_modeling.cpp` (alongside
`prony`), registered under `parametric`. Algorithm (validated by reproducing it
in MATLAB before porting): initialise `A` via `prony`, then `niter` (default 5)
Steiglitz–McBride iterations — prefilter the unit impulse and `h` by `1/A`
(`e`, `g`), build the Toeplitz LS `[E | -G]·[b; a_tail] ≈ g` (E = `e` shifted
0..nb, G = `g` shifted 1..na), solve the normal equations (reuses `solveSPD`),
update `[b, a]`.

Verified vs MATLAB R2025b (parity `stmcb.json` → OK):
`stmcb([1 .5 .25 .125 .0625],1,1)` → `a=[1 -0.5]`, `b≈[1 0]`; a 2nd-order system
`B=[1 0.3] / A=[1 -0.6 0.2]` recovered exactly from its impulse response
(`b=[1 0.3]`, `a=[1 -0.6 0.2]`). Guard: `lpc_parametric_test.cpp` (`Stmcb`,
DualEngine TW+VM); smoke `stmcb_smoke.m`.

**Deferred:** the two-signal form `stmcb(y, x, nb, na)` and the explicit `ai`
initial estimate (5th arg) are rejected with a clear error — the impulse-response
form (the common use) is implemented.

## References
- `src/toolboxes/signal/src/spectral_analysis/signal_modeling.cpp` (`stmcb`),
  `.../include/numkit/signal/spectral_analysis/signal_modeling.hpp`,
  `src/bundle/src/register/signal/spectral_analysis/signal_modeling_reg.cpp`.
- `tools/parity/specs/stmcb.json`.
- shipped: `prony`, `levinson`, `lpc`, `filter`
- MATLAB `doc stmcb`
