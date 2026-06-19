// lsqnonlin_reg.cpp — lsqnonlin / lsqcurvefit (nonlinear least squares) as
// embedded `.m` wrappers, mirroring the fzero / fminsearch / fsolve pattern
// (VM_CALLBACKS_PLAN.md): the residual F(p) is always user code, so the
// solver is written in `.m` and every F(p) evaluation runs as bytecode →
// pausable under the debugger, no C++ state machine.
//
// lsqnonlin(fun, p0) minimises ‖F(p)‖² via Levenberg-Marquardt with a
// forward-difference Jacobian — the same Gauss-Newton/LM core as fsolve,
// but it terminates at the least-squares minimiser (the step shrinking)
// rather than requiring F=0, so it handles over-determined residuals.
// lsqcurvefit(fun, p0, xdata, ydata) = lsqnonlin(@(p) fun(p,xdata)-ydata, p0).
// Bound constraints (lb/ub) are deferred — rejected with a clear error.

#include <numkit/core/engine.hpp>

namespace numkit::optim {

static const char *kLsqnonlinMSource = R"NKM(
function [p, resnorm, residual, exitflag] = lsqnonlin(fn, p0, lb, ub, varargin)
  if ~(strcmp(class(fn), 'function_handle') || iscell(fn))
    error('numkit:lsqnonlin:fnType', 'lsqnonlin: 1st argument must be a function handle');
  end
  if (nargin >= 3 && ~isempty(lb)) || (nargin >= 4 && ~isempty(ub))
    error('numkit:lsqnonlin:bounds', 'lsqnonlin: bound constraints (lb/ub) are not yet supported');
  end
  n = numel(p0);
  if n == 0
    error('numkit:lsqnonlin:badP0', 'lsqnonlin: p0 must be non-empty');
  end
  isRow = (size(p0, 1) == 1);
  x = reshape(p0, n, 1);
  [x, F] = nk_lsq_lm(fn, x, n, isRow);
  if isRow
    p = x.';
  else
    p = x;
  end
  residual = F;
  resnorm = F.' * F;
  exitflag = 1;
end

function [p, resnorm, residual, exitflag] = lsqcurvefit(fn, p0, xdata, ydata, lb, ub, varargin)
  if ~(strcmp(class(fn), 'function_handle') || iscell(fn))
    error('numkit:lsqcurvefit:fnType', 'lsqcurvefit: 1st argument must be a function handle');
  end
  if (nargin >= 5 && ~isempty(lb)) || (nargin >= 6 && ~isempty(ub))
    error('numkit:lsqcurvefit:bounds', 'lsqcurvefit: bound constraints (lb/ub) are not yet supported');
  end
  yc = reshape(ydata, numel(ydata), 1);
  resfn = @(p) reshape(fn(p, xdata), numel(yc), 1) - yc;
  [p, resnorm, residual, exitflag] = lsqnonlin(resfn, p0);
end

function F = nk_lsq_eval(fn, x, n, isRow)
  if isRow
    v = fn(reshape(x, 1, n));
  else
    v = fn(reshape(x, n, 1));
  end
  F = reshape(v, numel(v), 1);
end

function [x, F] = nk_lsq_lm(fn, x, n, isRow)
  F = nk_lsq_eval(fn, x, n, isRow);
  m = numel(F);
  nrm = norm(F);
  lambda = 1e-2;
  sqeps = sqrt(eps);
  for iter = 1:400
    J = zeros(m, n);
    for j = 1:n
      h = sqeps * max(abs(x(j)), 1);
      xp = x; xp(j) = xp(j) + h;
      Fp = nk_lsq_eval(fn, xp, n, isRow);
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
      Fn = nk_lsq_eval(fn, xn, n, isRow);
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

void registerLsqnonlinM(Engine &engine)
{
    engine.registerBuiltinMSource(kLsqnonlinMSource);
}

} // namespace numkit::optim
