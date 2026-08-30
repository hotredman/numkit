// nlinfit_reg.cpp — Engine adapters + embedded-.m registration relocated from
// the stats toolbox in Phase E (callback decoupling): the FnHandle LM kernels
// (nlinfit/nlpredci) stay core-free in the toolbox; this Engine-coupled glue
// (the @handle->FnHandle bridge + the pausable .m wrapper) lives in bundle.
#include <numkit/stats/regress/regress.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>

namespace numkit::stats {

// ── Adapters ─────────────────────────────────────────────────────────
namespace detail {

void nlinfit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("nlinfit: requires (X, y, fun, beta0)",
                    0, 0, "nlinfit", "", "numkit:nlinfit:nargin");
    if (!args[2].isFuncHandle())
        throw Error("nlinfit: `fun` must be a function handle",
                    0, 0, "nlinfit", "", "numkit:nlinfit:notFuncHandle");
    const Value &fun = args[2];
    auto model = [&ctx, &fun](Span<const Value> a, Span<Value> o,
                              std::pmr::memory_resource * /*mr*/) {
        Value r = ctx.engine->callFunctionHandle(fun, a);
        if (!o.empty()) o[0] = std::move(r);
    };
    auto res = nlinfit(args[0], args[1], model, args[3],
                       ctx.engine->resource());
    outs[0] = std::move(res.beta);
    if (nargout > 1) outs[1] = std::move(res.R);
    if (nargout > 2) outs[2] = std::move(res.J);
    if (nargout > 3) outs[3] = std::move(res.CovB);
    if (nargout > 4) outs[4] = std::move(res.MSE);
}

void nlparci_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nlparci: requires (beta, R, J [, alpha])",
                    0, 0, "nlparci", "", "numkit:nlparci:nargin");
    double alpha = 0.05;
    if (args.size() >= 4 && !args[3].isEmpty())
        alpha = args[3].toScalar();
    outs[0] = nlparci(args[0], args[1], args[2], alpha,
                      ctx.engine->resource());
}

void nlpredci_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 5)
        throw Error("nlpredci: requires (fun, X, beta, R, J [, alpha])",
                    0, 0, "nlpredci", "", "numkit:nlpredci:nargin");
    double alpha = 0.05;
    if (args.size() >= 6 && !args[5].isEmpty())
        alpha = args[5].toScalar();
    if (!args[0].isFuncHandle())
        throw Error("nlpredci: `fun` must be a function handle",
                    0, 0, "nlpredci", "", "numkit:nlpredci:notFuncHandle");
    const Value &fun = args[0];
    auto model = [&ctx, &fun](Span<const Value> a, Span<Value> o,
                              std::pmr::memory_resource * /*mr*/) {
        Value r = ctx.engine->callFunctionHandle(fun, a);
        if (!o.empty()) o[0] = std::move(r);
    };
    auto [ypred, delta] = nlpredci(model, args[1], args[2],
                                    args[3], args[4], alpha,
                                    ctx.engine->resource());
    outs[0] = std::move(ypred);
    if (nargout > 1) outs[1] = std::move(delta);
}

} // namespace detail

// ── nlinfit as an embedded `.m` wrapper (vm_callbacks_plan.md) ────────────────
// The model `fun(beta, X)` is always user code, so nlinfit takes the `.m` path:
// every model evaluation (residual + central-difference Jacobian) runs as
// bytecode → pausable under the debugger. Faithful transcription of the C++
// Levenberg-Marquardt loop above (same λ schedule 1e-3 ×0.1/×10, central-diff
// step h = 1e-7·max(|βj|,1), tolFun/tolX = 1e-10, maxIter = 200, final-Jacobian
// refresh, MSE = SSE/(n-p), CovB = MSE·inv(JᵀJ)). The linear LM step uses the
// built-in operators `JᵀJ + λ·diag`, `\`, and `inv` rather than the C++'s
// hand-rolled Gauss elimination: it converges to the same least-squares minimum
// (tight tolerances) and matches the regression-test outputs. nlinfit consumes
// no RNG, so noise realisations in callers are unchanged. The C++
// `nlinfit(...)` API is retained as the synchronous embedder path. Split into
// nlinfit + nk_nlinfit_jac + nk_nlinfit_model (255-register VM chunk limit).
static const char *kNlinfitMSource = R"NKM(
function [beta, R, J, CovB, MSE] = nlinfit(X, y, fun, beta0)
  if ~(strcmp(class(fun), 'function_handle') || iscell(fun))
    error('numkit:nlinfit:notFuncHandle', 'nlinfit: `fun` must be a function handle');
  end
  n = numel(y);
  p = numel(beta0);
  if p == 0
    error('numkit:nlinfit:emptyBeta0', 'nlinfit: beta0 cannot be empty');
  end
  beta = beta0(:);
  yv = y(:);
  r = yv - nk_nlinfit_model(fun, beta, X);
  sse = sum(r.^2);
  lambda = 1e-3; max_iter = 200; tol_fun = 1e-10; tol_x = 1e-10;
  for iter = 1:max_iter
    J = nk_nlinfit_jac(fun, beta, n, p, X);
    M = J' * J;
    for i = 1:p
      M(i,i) = M(i,i) * (1 + lambda);
    end
    g = J' * r;
    dbeta = M \ g;
    beta_new = beta + dbeta;
    step_norm = sqrt(sum(dbeta.^2));
    beta_norm = sqrt(sum(beta_new.^2));
    r_new = yv - nk_nlinfit_model(fun, beta_new, X);
    sse_new = sum(r_new.^2);
    if sse_new < sse
      rel = step_norm / (beta_norm + 1e-12);
      relf = (sse - sse_new) / (sse + 1e-12);
      beta = beta_new;
      r = r_new;
      sse = sse_new;
      lambda = lambda * 0.1;
      if rel < tol_x || relf < tol_fun, break; end
    else
      lambda = lambda * 10;
      if lambda > 1e16, break; end
    end
  end
  J = nk_nlinfit_jac(fun, beta, n, p, X);
  if n > p, mse = sse / (n - p); else, mse = sse; end
  CovB = mse * inv(J' * J);
  R = r;
  MSE = mse;
end

function yhat = nk_nlinfit_model(fun, beta, X)
  yhat = fun(beta, X);
  yhat = yhat(:);
end

function J = nk_nlinfit_jac(fun, beta, n, p, X)
  J = zeros(n, p);
  for j = 1:p
    scale = max(abs(beta(j)), 1);
    h = 1e-7 * scale;
    bp = beta; bp(j) = beta(j) + h;
    bm = beta; bm(j) = beta(j) - h;
    fp = nk_nlinfit_model(fun, bp, X);
    fm = nk_nlinfit_model(fun, bm, X);
    inv2h = 1 / (2 * h);
    J(:, j) = (fp - fm) * inv2h;
  end
end
)NKM";

void registerNlinfitM(Engine &engine)
{
    engine.registerBuiltinMSource(kNlinfitMSource);
}


} // namespace numkit::stats
