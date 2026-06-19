# optim.fsolve — nonlinear system solver missing

- **Status:** ✅ FIXED (2026-06-19) — embedded-.m Levenberg-Marquardt
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

## Fix (2026-06-19)
Implemented as an **embedded `.m`** (`fsolve_reg.cpp`, `kFsolveMSource` +
`registerFsolveM`), mirroring the fzero / fminsearch pattern — the objective
`F(x)` is always user code, so writing the solver in `.m` makes every `F(x)`
evaluation run as bytecode (pausable under the debugger, no C++ state
machine). Split into `fsolve` + `nk_fsolve_eval` + `nk_fsolve_lm` (per the
255-register VM gotcha).

Algorithm: **Levenberg-Marquardt** with a forward-difference Jacobian —
`(JᵀJ + λ·diag(JᵀJ))·dx = −JᵀF`, adapting λ (×0.4 on accept, ×3 on reject)
until the residual norm decreases; robust on singular / ill-conditioned
Jacobians. Iterates to ‖F‖<1e-12 or a tiny step. The input shape is
preserved when calling `F` (row x0 → row), and the root mirrors x0's
orientation.

Parity with MATLAB is on the **solution** (the root), not the iterate
trajectory: for a unique nearby root any convergent solver lands on the same
point. Verified vs MATLAB R2025b: `x^2−2 → 1.41421356`, the 2×2 unit-circle
system `→ [0.70710678 0.70710678]` (exitflag 1, ‖F‖≈6e-13), a Rosenbrock
square system `→ [1 1]`, and a 3-variable system with multiple roots that —
from x0=[1 0 4] — lands on `[1 2 3]` exactly as MATLAB. Parity `fsolve.json`
→ OK.

`options` (a 3rd arg) is accepted and ignored — the defaults are what's
implemented; the 4th `output`-struct output is not emitted.

## References
- `src/bundle/src/register/optim/fsolve_reg.cpp` (`kFsolveMSource`),
  `optim_library.cpp` (registration)
- `tools/parity/specs/fsolve.json`,
  `src/toolboxes/optim/tests/fsolve_test.cpp` (7 cases),
  `known_bugs_test.cpp` (`Fsolve`, promoted live),
  smoke `tests/smoke/fsolve_smoke.m`
- pattern: `fzero` / `fminsearch` embedded-.m wrappers
- MATLAB `doc fsolve`
