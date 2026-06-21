# control.initial — initial-condition response missing

- **Status:** ✅ FIXED (2026-06-19) — zero-input simulate from x0
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
`initial(sys, x0)` — the time response of a state-space system to an
initial state `x0` with zero input — is not registered. numkit ships
`step`/`impulse`/`lsim` but not the initial-condition response.

## Repro
```matlab
[y, t] = initial(ss(-2, 0, 1, 0), 1);
% MATLAB: y(1) = 1, y(end) = 0.00301995172040398  (y = e^{-2t})
% numkit: Error — VM: undefined function 'initial'
```

## Fix (2026-06-19)
Implemented `numkit::control::initial_response` (`response.cpp`) — the
existing ZOH state propagator (`simulate`) run with `u ≡ 0` and `x(0) = x0`,
output `y = C·x`. `initial(sys, x0[, tArg])`: `tArg` follows the same
semantics as `step`/`impulse` (Empty → auto grid from poles; scalar →
final time; vector → explicit grid). Returns `[y, t, x]` by nargout (`x` =
state trajectory). Wired via `initial_reg` (`a[1]` = x0, `a[2]` = optional
time arg).

Verified vs MATLAB R2025b (parity `initial.json` → OK on the **explicit
grid**, machine precision): 1st-order `A=−2, x0=1` → `y=e^{−2t}`,
`y(end at t=3)=e^{−6}=0.002478752177`; 2-state `A=[0 1;−2 −3], x0=[1;0]` →
`y(t=1)=0.6004235991`, `y(t=5)=0.013430494068`. The **auto-grid** horizon
matches MATLAB to ~1e-7 (numkit `tEnd=2.901255` vs MATLAB `2.901257`,
`y(end)=0.0030199651` vs `0.0030199517`, abs diff 1.3e-8 — within the
repro's 1e-6 tol): the auto-time grid is a heuristic, same caveat as
`step`/`impulse` auto-time; the explicit-`t` form is exact.

Guards: `initial_test.cpp` (6 TEST_F: explicit-grid exact / auto-grid /
2-state / state-trajectory / bad-x0-throws), `known_bugs_test.cpp`
(`Initial`, promoted live — passes the repro's auto-grid assertion at 1e-6);
smoke `initial_smoke.m`.

## References
- `src/toolboxes/control/src/response/response.cpp` (`initial_response`,
  reuses `simulate`/`readTimeArg`/`toSSiso`),
  `.../include/numkit/control/response/response.hpp`,
  `src/bundle/src/register/control/response/response_reg.cpp` (`initial_reg`).
- `tools/parity/specs/initial.json`.
- MATLAB `doc initial`
