# optim.lsqcurvefit / optim.lsqnonlin — missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing functions)
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

## Suggested fix
Levenberg–Marquardt (or trust-region-reflective) over a FnHandle residual
vector with a finite-difference Jacobian. `lsqcurvefit(fun,p0,x,y)` is
`lsqnonlin(@(p) fun(p,x)-y, p0)` — implement `lsqnonlin` core, wrap for
`lsqcurvefit`. Both take FnHandles, so likely script-only public surface.
Medium. Outputs `[p, resnorm, residual, exitflag, output]`.

## References
- new file under `libs/optim/src/...`
- shipped: `lsqnonneg`, `fminsearch`, `fzero`, `fminbnd`
- MATLAB `doc lsqcurvefit`, `doc lsqnonlin`
