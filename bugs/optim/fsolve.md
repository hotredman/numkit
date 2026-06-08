# optim.fsolve — nonlinear system solver missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
`fsolve(fun, x0)` — solve a system of nonlinear equations `F(x)=0` — is not
registered. numkit ships the 1-D root finder `fzero` and the minimizers
`fminsearch`/`fminbnd`/`fminunc`, but not the vector root solver. This is a
very common need (steady states, implicit equations, multi-variable roots).

## Repro
```matlab
x = fsolve(@(x) x^2 - 2, 1);                       % MATLAB: 1.41421356 (√2)
xv = fsolve(@(v) [v(1)^2+v(2)^2-1; v(1)-v(2)], [0.5 0.5]);
% MATLAB: xv = [0.70710678  0.70710678]
% numkit (each): Error — VM: undefined function 'fsolve'
```

## Root cause
Not implemented. The pieces exist (numkit has linear solves and the
`fminunc`/`lsqnonlin`-style descent machinery is being built); `fsolve` is a
trust-region / Levenberg-Marquardt or Newton iteration on `F` with a
finite-difference (or supplied) Jacobian.

## Suggested fix
Newton with a finite-difference Jacobian and a line search (or
Levenberg-Marquardt for robustness): iterate `x ← x − J⁻¹F(x)` until
`‖F‖<tol`. `fun` is a vector-valued `FnHandle`. Medium. Verify the scalar
case (√2) and a 2×2 system vs MATLAB. Shares the Jacobian/step machinery
with `lsqnonlin` (see optim/nonlinear-lsq.md).

## References
- new file under `toolboxes/optim/src/...`; cf. `fzero`/`fminunc`/`lsqnonlin`
- MATLAB `doc fsolve`
