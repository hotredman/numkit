// quadprog_reg.cpp — quadprog (quadratic program) as an embedded `.m`.
//
// min 0.5·xᵀHx + fᵀx  s.t.  A·x ≤ b,  Aeq·x = beq,  lb ≤ x ≤ ub.
//
// Primal active-set method for a strictly-convex QP (H positive definite),
// so it needs no Phase-1: each iteration solves the equality-constrained
// QP over the current working set via the KKT saddle-point system
//   [H Bᵀ; B 0]·[x; λ] = [−f; c]   (B = [Aeq; active inequalities]),
// then adds the most-violated inactive inequality or drops the active one
// with the most-negative multiplier until the KKT conditions hold. The QP
// optimum is unique for PD H, so this matches MATLAB quadprog's solution.
// No FnHandle here (H, f and the constraint matrices are data) — but the
// `.m` form keeps it on the same self-contained, debuggable footing as the
// other optim solvers.

#include <numkit/core/engine.hpp>

namespace numkit::optim {

static const char *kQuadprogMSource = R"NKM(
function [x, fval, exitflag] = quadprog(H, f, A, b, Aeq, beq, lb, ub, x0)
  n = size(H, 1);
  f = reshape(f, n, 1);
  C = zeros(0, n); d = zeros(0, 1);
  if nargin >= 4 && ~isempty(A)
    C = [C; A]; d = [d; reshape(b, numel(b), 1)];
  end
  if nargin >= 8 && ~isempty(ub)
    C = [C; eye(n)]; d = [d; reshape(ub, n, 1)];
  end
  if nargin >= 7 && ~isempty(lb)
    C = [C; -eye(n)]; d = [d; -reshape(lb, n, 1)];
  end
  if nargin >= 6 && ~isempty(Aeq)
    Be = Aeq; ce = reshape(beq, numel(beq), 1);
  else
    Be = zeros(0, n); ce = zeros(0, 1);
  end
  x = nk_qp_activeset(H, f, C, d, Be, ce, n);
  fval = 0.5 * x.' * H * x + f.' * x;
  exitflag = 1;
end

function [x, mu] = nk_qp_kkt(H, f, Amat, cvec, n)
  k = size(Amat, 1);
  if k == 0
    x = -(H \ f); mu = zeros(0, 1); return;
  end
  At = transpose(Amat);
  Z = zeros(k, k);
  K = [H, At; Amat, Z];
  rhs = [-f; cvec];
  sol = K \ rhs;
  x = sol(1:n);
  mu = sol(n+1:end);
end

function x = nk_qp_activeset(H, f, C, d, Be, ce, n)
  m = size(C, 1);
  W = false(m, 1);
  neq = size(Be, 1);
  x = zeros(n, 1);
  for iter = 1:(60 + 5*m)
    idx = find(W);
    if isempty(idx)
      Amat = Be; cvec = ce;
    else
      Amat = [Be; C(idx, :)]; cvec = [ce; d(idx)];
    end
    [x, mu] = nk_qp_kkt(H, f, Amat, cvec, n);
    muIneq = mu(neq+1:end);
    if m > 0
      resid = C * x - d;        % <= 0 means feasible
      if ~isempty(idx), resid(idx) = -inf; end   % ignore active constraints
      [maxv, jmax] = max(resid);
    else
      maxv = -inf;
    end
    if maxv > 1e-9
      W(jmax) = true;           % add the most-violated inactive constraint
    else
      if ~isempty(muIneq)
        [minmu, jmin] = min(muIneq);
      else
        minmu = 0;
      end
      if minmu < -1e-9
        W(idx(jmin)) = false;   % drop the most-negative active multiplier
      else
        return;                 % KKT satisfied -> optimum
      end
    end
  end
end
)NKM";

void registerQuadprogM(Engine &engine)
{
    engine.registerBuiltinMSource(kQuadprogMSource);
}

} // namespace numkit::optim
