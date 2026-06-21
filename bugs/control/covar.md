# control.covar — output covariance from white-noise input missing

- **Status:** ✅ FIXED (2026-06-19) — lyap/dlyap gramian + C·Q·Cᵀ
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
`covar(sys, W)` — the steady-state output covariance of a system driven by
white noise of intensity `W` — is not registered.

## Repro
```matlab
P = covar(ss(-1, 1, 1, 0), 1);
% MATLAB: P = 0.5
% numkit: Error — VM: undefined function 'covar'
```

## Fix (2026-06-19)
Implemented `numkit::control::covar` (`state.cpp`, reuses `pullABC` +
`lyap`/`dlyap`). `[P, Q] = covar(sys, W)`:
- **state covariance** `Q` solves the gramian Lyapunov equation with the
  noise-shaped input — continuous `A Q + Q Aᵀ + B W Bᵀ = 0` (via `lyap`),
  discrete `A Q Aᵀ − Q + B W Bᵀ = 0` (via `dlyap`);
- **output covariance** `P = C Q Cᵀ` (+ `D W Dᵀ` for discrete; `∞` for
  continuous if `D ≠ 0` — white noise through a direct feedthrough has
  unbounded variance).

`W` is a scalar intensity (`W·I`) or a full m×m matrix. Returns `[P, Q]`
by nargout.

Verified vs MATLAB R2025b (parity `covar.json` → OK): `1/(s+1)`,W=1 →
P=0.5 (closed form `B²W/(2|a|)·C²`); 2-state `[-1 0;0 -2]`,`[1;1]`,`[1 1]`
→ P=1.41667, Q=`[0.5 0.3333; 0.3333 0.25]`; linear in W (W=4 → P=2);
discrete `[0.5 0;0 0.3]`,Ts=0.1,W=2 → P=9.570351; continuous D=0.5 → Inf.
Guards: `covar_test.cpp` (5 TEST_F: closed-form / scaling / 2-state P+Q /
discrete / feedthrough-Inf), `known_bugs_test.cpp` (`Covar`, promoted
live); smoke `covar_smoke.m`.

## References
- `src/toolboxes/control/src/state/state.cpp` (`covar`),
  `.../include/numkit/control/state/state.hpp` (`CovarResult`),
  `src/bundle/src/register/control/state/state_reg.cpp` (`covar_reg`).
- `tools/parity/specs/covar.json`.
- shipped + reused: `lyap`/`dlyap`/`gram`/`pullABC`
- MATLAB `doc covar`
