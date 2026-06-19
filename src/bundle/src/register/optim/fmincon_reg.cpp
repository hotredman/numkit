// fmincon_reg.cpp — fmincon (constrained minimization) as an embedded `.m`,
// mirroring the other optim solvers (pausable objective).
//
// min f(x) s.t. A·x ≤ b, Aeq·x = beq, lb ≤ x ≤ ub.
//
// Sequential Quadratic Programming (SQP) reusing the shipped `quadprog` as
// the QP subproblem solver. At each iterate x: FD gradient g = ∇f, a BFGS
// Hessian estimate B of the objective, and the step d from
//   min 0.5 dᵀB d + gᵀd  s.t.  A·d ≤ b−Ax, Aeq·d = beq−Aeq x, lb−x ≤ d ≤ ub−x,
// followed by a backtracking line search on f (the QP keeps the linear/bound
// constraints feasible, and the feasible region is convex, so every
// x + α·d, α∈[0,1], stays feasible). BFGS update; repeat until the step
// vanishes. Parity with MATLAB fmincon is on the SOLUTION (the minimiser).
//
// SCOPE: nonlinear constraints (`nonlcon`) are NOT supported — their MATLAB
// interface is `[c, ceq] = nonlcon(x)`, a *multi-output* function-handle
// call, which the numkit VM cannot currently make (see
// bugs/lang/multi-output-handle-call). A non-empty `nonlcon` is rejected
// with a clear error.

#include <numkit/core/engine.hpp>

namespace numkit::optim {

static const char *kFminconMSource = R"NKM(
function [x, fval, exitflag] = fmincon(fn, x0, A, b, Aeq, beq, lb, ub, nonlcon, varargin)
  if ~(strcmp(class(fn), 'function_handle') || iscell(fn))
    error('numkit:fmincon:fnType', 'fmincon: 1st argument must be a function handle');
  end
  if nargin >= 9 && ~isempty(nonlcon)
    error('numkit:fmincon:nonlcon', 'fmincon: nonlinear constraints (nonlcon) are not yet supported (the numkit VM cannot make the [c,ceq]=nonlcon(x) multi-output handle call); use linear and bound constraints');
  end
  n = numel(x0);
  if nargin < 3, A = []; end
  if nargin < 4, b = []; end
  if nargin < 5, Aeq = []; end
  if nargin < 6, beq = []; end
  if nargin < 7, lb = []; end
  if nargin < 8, ub = []; end
  isRow = (size(x0, 1) == 1);
  x = reshape(x0, n, 1);
  [x, fval] = nk_fc_sqp(fn, x, n, isRow, A, b, Aeq, beq, lb, ub);
  if isRow
    x = x.';
  end
  exitflag = 1;
end

function f = nk_fc_obj(fn, x, n, isRow)
  if isRow
    v = fn(reshape(x, 1, n));
  else
    v = fn(reshape(x, n, 1));
  end
  f = v(1);
end

function g = nk_fc_grad(fn, x, n, isRow)
  g = zeros(n, 1);
  c = eps ^ (1/3);
  for i = 1:n
    h = c * max(abs(x(i)), 1);
    xp = x; xp(i) = xp(i) + h;
    xm = x; xm(i) = xm(i) - h;
    g(i) = (nk_fc_obj(fn, xp, n, isRow) - nk_fc_obj(fn, xm, n, isRow)) / (2*h);
  end
end

function [x, fx] = nk_fc_sqp(fn, x, n, isRow, A, b, Aeq, beq, lb, ub)
  B = eye(n);
  fx = nk_fc_obj(fn, x, n, isRow);
  g = nk_fc_grad(fn, x, n, isRow);
  bc = reshape(b, numel(b), 1);
  bec = reshape(beq, numel(beq), 1);
  for iter = 1:200
    if ~isempty(A), Ad = A; bd = bc - A*x; else, Ad = []; bd = []; end
    if ~isempty(Aeq), Aed = Aeq; bed = bec - Aeq*x; else, Aed = []; bed = []; end
    if ~isempty(lb), lbd = reshape(lb, n, 1) - x; else, lbd = []; end
    if ~isempty(ub), ubd = reshape(ub, n, 1) - x; else, ubd = []; end
    d = quadprog(B, g, Ad, bd, Aed, bed, lbd, ubd);
    nd = norm(d);
    if nd < 1e-10
      break;
    end
    f0 = nk_fc_obj(fn, x, n, isRow);
    alpha = 1; xn = x + d;
    for ls = 1:40
      xn = x + alpha * d;
      if nk_fc_obj(fn, xn, n, isRow) < f0
        break;
      end
      alpha = alpha * 0.5;
    end
    gn = nk_fc_grad(fn, xn, n, isRow);
    s = alpha * d; y = gn - g;
    sBs = s.' * (B * s);
    if (s.' * y) > 1e-12 && sBs > 1e-12
      Bs = B * s;
      B = B - (Bs * Bs.') / sBs + (y * y.') / (s.' * y);
    end
    x = xn; fx = nk_fc_obj(fn, x, n, isRow); g = gn;
    if alpha * nd < 1e-10
      break;
    end
  end
end
)NKM";

void registerFminconM(Engine &engine)
{
    engine.registerBuiltinMSource(kFminconMSource);
}

} // namespace numkit::optim
