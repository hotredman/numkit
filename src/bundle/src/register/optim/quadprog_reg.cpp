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
  x = nk_qp_admm(H, f, C, d, Be, ce, n);
  fval = 0.5 * x.' * H * x + f.' * x;
  exitflag = 1;
end

function [x, mu] = nk_qp_kkt(H, f, Amat, cvec, n)
  k = size(Amat, 1);
  if k == 0
    x = -(H \ f); mu = zeros(0, 1); return;
  end
  At = transpose(Amat);
  % Primal-dual KKT regularisation (the OSQP trick): a tiny -delta*I on
  % the multiplier block makes the saddle system quasi-definite —
  % nonsingular even when the active rows are linearly dependent (which
  % a strict mldivide rejects; pinv instead returns min-norm garbage that
  % violates the constraints — tried). delta is far below H's scale.
  % (bugs/opened/optim/linprog-rank-deficient-error.md)
  delta = 1e-12;
  Z = -delta * eye(k);
  K = [H, At; Amat, Z];
  rhs = [-f; cvec];
  sol = K \ rhs;
  x = sol(1:n);
  mu = sol(n+1:end);
end

function x = nk_qp_admm(H, f, C, d, Be, ce, n)
  % OSQP-style ADMM (no feasible start needed, rank-deficiency-tolerant)
  % followed by an exact POLISH: solve the equality-KKT on the final
  % active set via nk_qp_kkt and keep the polished vertex when it is
  % feasible and at least as good. Replaces the old constraint-
  % accumulation active set, whose delta-regularised multipliers (O(1/
  % delta)) drove noise add/drop CYCLING to the iteration cap, which it
  % then returned silently (bugs/closed/optim/linprog-unbounded-result-
  % on-bounded-lp).
  A = [C; Be];
  mA = size(A, 1);
  l = [-inf(size(C, 1), 1); ce];
  u = [d; ce];
  if mA == 0
    [x, ~] = nk_qp_kkt(H, f, zeros(0, n), zeros(0, 1), n);
    return;
  end
  rho = 0.1; sig = 1e-6;
  K = H + rho * (A' * A) + sig * eye(n);
  x = zeros(n, 1); z = zeros(mA, 1); y = zeros(mA, 1);
  for it = 1:4000
    x = K \ (-f + rho * A' * (z - y / rho) + sig * x);
    w = A * x + y / rho;
    z = min(max(w, l), u);
    y = y + rho * (A * x - z);
    rz = A * x - z;
    pr = norm(rz, inf);
    dr = norm(H * x + f + A' * y, inf);
    if pr < 1e-9 && dr < 1e-9
      break;
    end
  end
  % POLISH: rows where z clamped to a (finite) bound are the active set.
  act = find((w <= l + 1e-7 & ~isinf(l)) | (w >= u - 1e-7 & ~isinf(u)));
  if isempty(act)
    return;
  end
  bact = zeros(numel(act), 1);
  for k = 1:numel(act)
    if w(act(k)) >= u(act(k)) - 1e-7
      bact(k) = u(act(k));
    else
      bact(k) = l(act(k));
    end
  end
  [xp, ~] = nk_qp_kkt(H, f, A(act, :), bact, n);
  feas = max([C * xp - d; abs(Be * xp - ce)]);
  % Accept the polish on FEASIBILITY alone: the ADMM iterate is only
  % residual-tolerance feasible, so its objective can sit *below* the
  % true optimum — comparing objectives would reject the exact vertex
  % (observed as a 2e-9 objVal drift on the LP-book repro).
  if isempty(feas) || max(feas) < 1e-7
    x = xp;
  end
end
)NKM";

void registerQuadprogM(Engine &engine)
{
    engine.registerBuiltinMSource(kQuadprogMSource);
}

} // namespace numkit::optim
