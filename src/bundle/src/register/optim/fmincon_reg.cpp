// fmincon_reg.cpp — fmincon (nonlinear constrained minimization) as an
// embedded `.m`, mirroring the other optim solvers (pausable objective).
//
// min f(x) s.t. A·x ≤ b, Aeq·x = beq, lb ≤ x ≤ ub, c(x) ≤ 0, ceq(x) = 0.
//
// Sequential Quadratic Programming (SQP) reusing the shipped `quadprog` as
// the QP subproblem solver. At each iterate x: FD gradient g = ∇f, a BFGS
// Hessian estimate B of the objective, the nonlinear constraints + FD
// Jacobians (linearised), and the step d from
//   min 0.5 dᵀB d + gᵀd  s.t.  [A; Jc]·d ≤ [b−Ax; −c],
//                               [Aeq; Jceq]·d = [beq−Aeq x; −ceq],
//                               lb−x ≤ d ≤ ub−x,
// then an L1-merit backtracking line search (penalising the nonlinear
// constraint violation; the linear/bound constraints stay feasible via the
// QP). BFGS update; repeat until the step vanishes. Parity with MATLAB
// fmincon is on the SOLUTION (the local minimiser).
//
// nonlcon returns `[c, ceq]` — a multi-output function-handle call, enabled
// by the anonymous-multi-output / varargout support (the objective and
// nonlcon both run as bytecode, pausable).

#include <numkit/core/engine.hpp>

namespace numkit::optim {

static const char *kFminconMSource = R"NKM(
function [x, fval, exitflag] = fmincon(fn, x0, A, b, Aeq, beq, lb, ub, nonlcon, varargin)
  if ~(strcmp(class(fn), 'function_handle') || iscell(fn))
    error('numkit:fmincon:fnType', 'fmincon: 1st argument must be a function handle');
  end
  n = numel(x0);
  if nargin < 3, A = []; end
  if nargin < 4, b = []; end
  if nargin < 5, Aeq = []; end
  if nargin < 6, beq = []; end
  if nargin < 7, lb = []; end
  if nargin < 8, ub = []; end
  if nargin < 9, nonlcon = []; end
  isRow = (size(x0, 1) == 1);
  x = reshape(x0, n, 1);
  [x, fval] = nk_fc_sqp(fn, x, n, isRow, A, b, Aeq, beq, lb, ub, nonlcon);
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

function [cv, ev] = nk_fc_nleval(nonlcon, x, n, isRow)
  if isRow
    [c, ceq] = nonlcon(reshape(x, 1, n));
  else
    [c, ceq] = nonlcon(reshape(x, n, 1));
  end
  cv = reshape(c, numel(c), 1);
  ev = reshape(ceq, numel(ceq), 1);
end

function [cv, ev, Jc, Jceq] = nk_fc_nljac(nonlcon, x, n, isRow)
  [cv, ev] = nk_fc_nleval(nonlcon, x, n, isRow);
  mc = numel(cv); me = numel(ev);
  Jc = zeros(mc, n); Jceq = zeros(me, n);
  d = eps ^ (1/3);
  for i = 1:n
    h = d * max(abs(x(i)), 1);
    xp = x; xp(i) = xp(i) + h;
    xm = x; xm(i) = xm(i) - h;
    [cp, ep] = nk_fc_nleval(nonlcon, xp, n, isRow);
    [cm, em] = nk_fc_nleval(nonlcon, xm, n, isRow);
    if mc > 0, Jc(:, i) = (cp - cm) / (2*h); end
    if me > 0, Jceq(:, i) = (ep - em) / (2*h); end
  end
end

function phi = nk_fc_merit(fn, x, n, isRow, nonlcon, mu)
  phi = nk_fc_obj(fn, x, n, isRow);
  if ~isempty(nonlcon)
    [cv, ev] = nk_fc_nleval(nonlcon, x, n, isRow);
    if ~isempty(cv), phi = phi + mu * sum(max(0, cv)); end
    if ~isempty(ev), phi = phi + mu * sum(abs(ev)); end
  end
end

function [x, fx] = nk_fc_sqp(fn, x, n, isRow, A, b, Aeq, beq, lb, ub, nonlcon)
  B = eye(n);
  fx = nk_fc_obj(fn, x, n, isRow);
  g = nk_fc_grad(fn, x, n, isRow);
  hasNL = ~isempty(nonlcon);
  mu = 100;
  bc = reshape(b, numel(b), 1);
  bec = reshape(beq, numel(beq), 1);
  for iter = 1:200
    if hasNL
      [cv, ev, Jc, Jceq] = nk_fc_nljac(nonlcon, x, n, isRow);
    else
      cv = zeros(0, 1); ev = zeros(0, 1); Jc = zeros(0, n); Jceq = zeros(0, n);
    end
    if ~isempty(A)
      Ad = [A; Jc]; bd = [bc - A*x; -cv];
    else
      Ad = Jc; bd = -cv;
    end
    if ~isempty(Aeq)
      Aed = [Aeq; Jceq]; bed = [bec - Aeq*x; -ev];
    else
      Aed = Jceq; bed = -ev;
    end
    if ~isempty(lb), lbd = reshape(lb, n, 1) - x; else, lbd = []; end
    if ~isempty(ub), ubd = reshape(ub, n, 1) - x; else, ubd = []; end
    d = quadprog(B, g, Ad, bd, Aed, bed, lbd, ubd);
    nd = norm(d);
    if nd < 1e-10
      break;
    end
    m0 = nk_fc_merit(fn, x, n, isRow, nonlcon, mu);
    alpha = 1; xn = x + d;
    for ls = 1:40
      xn = x + alpha * d;
      if nk_fc_merit(fn, xn, n, isRow, nonlcon, mu) < m0
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
