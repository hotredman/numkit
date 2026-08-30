# optim.fminunc — unconstrained gradient minimizer

- **Status:** ✅ FIXED (2026-06-19) — embedded-.m BFGS quasi-Newton
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep (split from
  [constrained-solvers](constrained-solvers.md) 2026-06-19)

## Symptom
`fminunc(fun, x0)` — unconstrained minimization of a smooth objective — was
not registered. numkit shipped the derivative-free `fminsearch` (Nelder-Mead)
and the 1-D `fminbnd`, but not the gradient-based multivariable minimizer.

## Repro
```matlab
fminunc(@(x) (x-3)^2, 0)                                       % MATLAB: 3
fminunc(@(x) 100*(x(2)-x(1)^2)^2 + (1-x(1))^2, [-1.2 1])       % MATLAB: [1 1]
% numkit (each): Error — VM: undefined function 'fminunc'
```

## Fix (2026-06-19)
Implemented as an **embedded `.m`** (`fminunc_reg.cpp`), mirroring the fzero /
fminsearch / fsolve / lsqnonlin pattern — the objective `f(x)` is always user
code, so writing the solver in `.m` makes every `f(x)` evaluation run as
bytecode (pausable under the debugger). Split into `fminunc` +
`nk_fminunc_eval` + `nk_fminunc_grad` + `nk_fminunc_bfgs`.

Algorithm: **BFGS quasi-Newton** with a central-difference gradient and an
Armijo backtracking line search; maintains an inverse-Hessian estimate `H`
(reset to `I` if FD noise yields a non-descent direction). Faster than
fminsearch on smooth problems. The minimiser mirrors x0's orientation.

Parity with MATLAB is on the **solution** (the minimiser). Verified vs MATLAB
R2025b: parabola `(x−3)² → 3`, quadratic bowl `(x₁−1)²+2(x₂+2)²+3 → [1 −2]`
(fval 3), Rosenbrock `→ [1 1]` (MATLAB's quasi-newton stops at
`[0.99999 0.99998]`; numkit's central-FD BFGS converges tighter — both ≈[1 1],
parity tol 1e-4). Parity `fminunc.json` → OK.

The supplied-gradient form (`fun` returning `[f, grad]`) and `options` are not
yet used (forward to a follow-up); the FD gradient is always used.

## References
- `src/bundle/src/register/optim/fminunc_reg.cpp` (`kFminuncMSource`),
  `optim_library.cpp` (registration)
- `tools/parity/specs/fminunc.json`,
  `src/toolboxes/optim/tests/fminunc_test.cpp` (5 cases),
  `known_bugs_test.cpp` (`Fminunc`, promoted live),
  smoke `tests/smoke/fminunc_smoke.m`
- pattern: `fzero` / `fminsearch` / `fsolve` / `lsqnonlin` embedded-.m wrappers
- MATLAB `doc fminunc`
