// linprog_reg.cpp — linprog (linear program) as an embedded `.m`.
//
// min fᵀx  s.t.  A·x ≤ b,  Aeq·x = beq,  lb ≤ x ≤ ub.
//
// Solved by a **proximal (Tikhonov) regularization** that reuses the
// strictly-convex quadprog active-set: minimise fᵀx + (ε/2)·‖x‖² for a tiny
// ε. At an LP optimum that is a *vertex*, exactly n constraints are active
// and fully determine x via a linear solve — independent of the ε·I term —
// so the regularized QP returns the **exact** vertex (verified across
// ε = 1e-6 … 1e-10). The ε term only selects which active set is optimal,
// and for small ε the fᵀx term dominates → the LP-optimal vertex.
//
// Scope / caveats (documented in bugs/optim/linprog.md):
//   * Exact for bounded, feasible LPs with a **unique** optimum.
//   * On a **degenerate** optimum (an optimal face, not a single vertex) the
//     regularization returns the minimum-‖x‖ optimal point — MATLAB returns
//     a basic vertex, which may differ (the objective value still matches).
//   * Unboundedness is only flagged heuristically (‖x‖ blows up); a full
//     2-phase simplex would be the exact follow-up.

#include <numkit/core/engine.hpp>

namespace numkit::optim {

static const char *kLinprogMSource = R"NKM(
function [x, fval, exitflag] = linprog(f, A, b, Aeq, beq, lb, ub, x0)
  n = numel(f);
  fc = reshape(f, n, 1);
  if nargin < 2, A = []; end
  if nargin < 3, b = []; end
  if nargin < 4, Aeq = []; end
  if nargin < 5, beq = []; end
  if nargin < 6, lb = []; end
  if nargin < 7, ub = []; end
  ep = 1e-9;
  H = ep * eye(n);
  x = quadprog(H, fc, A, b, Aeq, beq, lb, ub);
  fval = fc.' * x;
  if norm(x) > 1e8
    exitflag = -3;     % heuristic: regularization-only bound -> likely unbounded
  else
    exitflag = 1;
  end
end
)NKM";

void registerLinprogM(Engine &engine)
{
    engine.registerBuiltinMSource(kLinprogMSource);
}

} // namespace numkit::optim
