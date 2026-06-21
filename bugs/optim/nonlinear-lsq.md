# optim.lsqcurvefit / optim.lsqnonlin — missing

- **Status:** ✅ FIXED (2026-06-19) — embedded-.m Levenberg-Marquardt (reuses fsolve core)
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
The nonlinear least-squares solvers `lsqcurvefit` and `lsqnonlin` are not
registered.

## Repro
```matlab
lsqcurvefit(@(p,xd) p(1)*exp(p(2)*xd), [1 -1], [0 1 2], [1 0.5 0.2])
% numkit: Error — VM: undefined function 'lsqcurvefit'
lsqnonlin(@(p) [p(1)-1; p(2)-2], [0 0])
% numkit: Error — VM: undefined function 'lsqnonlin'
```

## Root cause
Not implemented. numkit has `lsqnonneg` (linear NNLS) and `fminsearch`
(derivative-free), but no general nonlinear least-squares.

## Fix (2026-06-19)
Implemented as **embedded `.m`** (`lsqnonlin_reg.cpp`), reusing the same
Gauss-Newton/Levenberg-Marquardt core as `fsolve` (forward-difference
Jacobian, `(JᵀJ + λ·diag)·dx = −JᵀF`, adaptive λ) — but it terminates at the
least-squares **minimiser** (step shrinking) rather than requiring `F=0`, so
it handles over-determined residuals (`m > n`). Pausable residual (the
objective runs as bytecode). Split into `lsqnonlin` + `lsqcurvefit` +
`nk_lsq_lm` + `nk_lsq_eval`.

- `lsqnonlin(fun,p0)` → `[p, resnorm, residual, exitflag]`. `p` mirrors p0's
  orientation; `residual = F(p)` (column); `resnorm = FᵀF`.
- `lsqcurvefit(fun,p0,x,y)` = `lsqnonlin(@(p) fun(p,x)−y, p0)` — a thin
  wrapper building the residual handle.

Verified vs MATLAB R2025b: `lsqnonlin([p1−1;p2−2])` → `[1 2]` resnorm 0;
Rosenbrock residual → `[1 1]`; `lsqcurvefit` noise-free `2·sin(1.5x)` →
`[2 1.5]` exactly. The `a·exp(b·x)` 3-point fit has a **flat minimum** so its
params are loosely determined (numkit vs MATLAB land ~1e-5 apart) but the
**resnorm is tight** and matches to 8+ digits (`0.001248164767`) — parity
fingerprints the resnorm for that case, not the params. Parity
`lsqnonlin.json` → OK.

Bound constraints (`lb`/`ub`) are **deferred** — accepted but rejected with a
clear error when non-empty; `options` ignored; the `output` struct not
emitted.

## References
- `src/bundle/src/register/optim/lsqnonlin_reg.cpp` (`kLsqnonlinMSource`),
  `optim_library.cpp` (registration)
- `tools/parity/specs/lsqnonlin.json`,
  `src/toolboxes/optim/tests/lsqnonlin_test.cpp` (7 cases),
  `known_bugs_test.cpp` (`Lsqnonlin` + `Lsqcurvefit`, promoted live),
  smoke `tests/smoke/lsqnonlin_smoke.m`
- reused: the `fsolve` LM machinery; pattern from `fzero`/`fminsearch`
- MATLAB `doc lsqcurvefit`, `doc lsqnonlin`
