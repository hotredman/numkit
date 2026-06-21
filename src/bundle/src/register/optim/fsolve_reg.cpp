// fsolve_reg.cpp — fsolve (nonlinear system solver) as an embedded `.m`
// wrapper, mirroring the fzero / fminsearch pattern (VM_CALLBACKS_PLAN.md):
// the objective F(x) is always user code, so the solver is written in `.m`
// and every F(x) evaluation runs as bytecode → pausable under the debugger
// for free, no C++ state machine.
//
// Algorithm: Levenberg-Marquardt on F(x)=0 with a forward-difference
// Jacobian. (J'J + lambda·diag(J'J))·dx = -J'F, adapting lambda until the
// residual norm decreases — robust on singular / ill-conditioned Jacobians.
// Parity with MATLAB fsolve is on the SOLUTION (the root), not the iterate
// trajectory: for a problem with a unique root near x0 any convergent
// solver lands on the same point.

#include <numkit/core/engine.hpp>

namespace numkit::optim {

static const char *kFsolveMSource = R"NKM(
function [x, fval, exitflag] = fsolve(fn, x0, varargin)
  if ~(strcmp(class(fn), 'function_handle') || iscell(fn))
    error('numkit:fsolve:fnType', 'fsolve: 1st argument must be a function handle');
  end
  n = numel(x0);
  if n == 0
    error('numkit:fsolve:badX0', 'fsolve: x0 must be non-empty');
  end
  isRow = (size(x0, 1) == 1);
  x = reshape(x0, n, 1);
  [x, F, nrm] = nk_fsolve_lm(fn, x, n, isRow);
  if isRow
    x = x.'; fval = F.';
  else
    fval = F;
  end
  if nrm < 1e-6
    exitflag = 1;
  else
    exitflag = 0;
  end
end

function F = nk_fsolve_eval(fn, x, n, isRow)
  if isRow
    v = fn(reshape(x, 1, n));
  else
    v = fn(reshape(x, n, 1));
  end
  F = reshape(v, numel(v), 1);
end

function [x, F, nrm] = nk_fsolve_lm(fn, x, n, isRow)
  F = nk_fsolve_eval(fn, x, n, isRow);
  m = numel(F);
  nrm = norm(F);
  lambda = 1e-2;
  sqeps = sqrt(eps);
  for iter = 1:400
    if nrm < 1e-12
      break;
    end
    J = zeros(m, n);
    for j = 1:n
      h = sqeps * max(abs(x(j)), 1);
      xp = x; xp(j) = xp(j) + h;
      Fp = nk_fsolve_eval(fn, xp, n, isRow);
      J(:, j) = (Fp - F) / h;
    end
    A = J.' * J;
    g = J.' * F;
    d = diag(A);
    accepted = false;
    dx = zeros(n, 1);
    for tries = 1:25
      H = A + lambda * diag(max(d, 1e-12));
      dx = -(H \ g);
      xn = x + dx;
      Fn = nk_fsolve_eval(fn, xn, n, isRow);
      nn = norm(Fn);
      if nn < nrm
        x = xn; F = Fn; nrm = nn;
        lambda = max(lambda * 0.4, 1e-14);
        accepted = true;
        break;
      else
        lambda = lambda * 3;
      end
    end
    if ~accepted
      break;
    end
    if norm(dx) < 1e-13 * (1 + norm(x))
      break;
    end
  end
end
)NKM";

void registerFsolveM(Engine &engine)
{
    engine.registerBuiltinMSource(kFsolveMSource);
}

} // namespace numkit::optim
