// fminunc_reg.cpp — fminunc (unconstrained gradient minimization) as an
// embedded `.m` wrapper, mirroring the fzero / fminsearch / fsolve pattern
// (VM_CALLBACKS_PLAN.md): the objective f(x) is always user code, so the
// solver is written in `.m` and every f(x) evaluation runs as bytecode →
// pausable under the debugger, no C++ state machine.
//
// Algorithm: BFGS quasi-Newton with a central-difference gradient and an
// Armijo backtracking line search. Maintains an inverse-Hessian estimate H
// (reset to I if a non-descent direction appears from FD noise). Parity with
// MATLAB fminunc is on the SOLUTION (the minimiser): for a convex / well-
// conditioned objective any convergent quasi-Newton method lands on the same
// point. Unlike fminsearch (derivative-free Nelder-Mead), fminunc uses the
// gradient — faster on smooth problems.

#include <numkit/core/engine.hpp>

namespace numkit::optim {

static const char *kFminuncMSource = R"NKM(
function [x, fval, exitflag] = fminunc(fn, x0, varargin)
  if ~(strcmp(class(fn), 'function_handle') || iscell(fn))
    error('numkit:fminunc:fnType', 'fminunc: 1st argument must be a function handle');
  end
  n = numel(x0);
  if n == 0
    error('numkit:fminunc:badX0', 'fminunc: x0 must be non-empty');
  end
  isRow = (size(x0, 1) == 1);
  x = reshape(x0, n, 1);
  [x, fval] = nk_fminunc_bfgs(fn, x, n, isRow);
  if isRow
    x = x.';
  end
  exitflag = 1;
end

function f = nk_fminunc_eval(fn, x, n, isRow)
  if isRow
    v = fn(reshape(x, 1, n));
  else
    v = fn(reshape(x, n, 1));
  end
  f = v(1);
end

function g = nk_fminunc_grad(fn, x, n, isRow)
  g = zeros(n, 1);
  c = eps ^ (1/3);
  for i = 1:n
    h = c * max(abs(x(i)), 1);
    xp = x; xp(i) = xp(i) + h;
    xm = x; xm(i) = xm(i) - h;
    g(i) = (nk_fminunc_eval(fn, xp, n, isRow) - nk_fminunc_eval(fn, xm, n, isRow)) / (2*h);
  end
end

function [x, fx] = nk_fminunc_bfgs(fn, x, n, isRow)
  fx = nk_fminunc_eval(fn, x, n, isRow);
  g = nk_fminunc_grad(fn, x, n, isRow);
  H = eye(n);
  for iter = 1:400
    if norm(g, inf) < 1e-7
      break;
    end
    p = -H * g;
    slope = g.' * p;
    if slope >= 0
      p = -g; slope = g.' * p; H = eye(n);
    end
    alpha = 1; ok = false; xn = x; fn_ = fx;
    for ls = 1:60
      xn = x + alpha * p;
      fn_ = nk_fminunc_eval(fn, xn, n, isRow);
      if fn_ <= fx + 1e-4 * alpha * slope
        ok = true; break;
      end
      alpha = alpha * 0.5;
    end
    if ~ok
      break;
    end
    s = alpha * p;
    gn = nk_fminunc_grad(fn, xn, n, isRow);
    y = gn - g;
    sy = s.' * y;
    if sy > 1e-12
      rho = 1 / sy;
      I = eye(n);
      H = (I - rho*(s*y.')) * H * (I - rho*(y*s.')) + rho*(s*s.');
    end
    x = xn; fx = fn_; g = gn;
    if norm(s) < 1e-12 * (1 + norm(x))
      break;
    end
  end
end
)NKM";

void registerFminuncM(Engine &engine)
{
    engine.registerBuiltinMSource(kFminuncMSource);
}

} // namespace numkit::optim
