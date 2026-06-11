// toolboxes/stats/src/regress/nlinfit.cpp
//
// Nonlinear least-squares fit via Levenberg-Marquardt + parameter and
// prediction confidence intervals.

#include <numkit/stats/regress/regress.hpp>

#include <numkit/stats/distributions/students_t.hpp>     // tinv
#include <numkit/linalg/solvers.hpp>                     // linsolve
#include <numkit/linalg/properties.hpp>                  // inv

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/fn_handle.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace numkit::stats {

namespace {

// Pull a Value as a flat double vector (column-major contents).
std::vector<double> toFlatDouble(const Value &v)
{
    const std::size_t n = v.numel();
    std::vector<double> out(n);
    for (std::size_t i = 0; i < n; ++i) out[i] = v.elemAsDouble(i);
    return out;
}

// Build a Value from a column-major buffer.
Value flatToMatrix(const double *data, std::size_t rows, std::size_t cols,
                    std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    std::memcpy(out.doubleDataMut(), data, rows * cols * sizeof(double));
    return out;
}

// Evaluate model(beta, X) → returns flat predictions length n.
std::vector<double> evalModel(FnHandle model, const std::vector<double> &beta,
                               std::size_t p, const Value &X,
                               std::pmr::memory_resource *mr)
{
    auto betaVal = flatToMatrix(beta.data(), p, 1, mr);
    std::array<Value, 2> argv{ betaVal, X };
    Value yhat;
    model(Span<const Value>(argv.data(), 2), Span<Value>(&yhat, 1), mr);
    return toFlatDouble(yhat);
}

// Numerical Jacobian via central differences.
// Returns row-major matrix of size n × p stored column-major
// (column-major: J[col * n + row]).
std::vector<double> numericalJacobian(FnHandle model,
                                       const std::vector<double> &beta,
                                       std::size_t n, std::size_t p,
                                       const Value &X,
                                       std::pmr::memory_resource *mr)
{
    std::vector<double> J(n * p);
    auto bplus = beta;
    auto bminus = beta;
    for (std::size_t j = 0; j < p; ++j) {
        const double scale = std::max(std::abs(beta[j]), 1.0);
        const double h = 1e-7 * scale;
        bplus[j]  = beta[j] + h;
        bminus[j] = beta[j] - h;
        auto fp = evalModel(model, bplus,  p, X, mr);
        auto fm = evalModel(model, bminus, p, X, mr);
        bplus[j]  = beta[j];
        bminus[j] = beta[j];
        const double inv2h = 1.0 / (2.0 * h);
        for (std::size_t i = 0; i < n; ++i)
            J[j * n + i] = (fp[i] - fm[i]) * inv2h;
    }
    return J;
}

// Compute (J' · J + λ · diag(J' · J)) · dβ = J' · r  via Gauss elim.
// J is n × p column-major, r is length n.
std::vector<double> lmStep(const std::vector<double> &J,
                            std::size_t n, std::size_t p,
                            const std::vector<double> &r,
                            double lambda)
{
    // M = J' · J + λ · diag(J' · J)
    std::vector<double> M(p * p, 0.0);
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = 0; j < p; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k)
                s += J[i * n + k] * J[j * n + k];
            M[i * p + j] = s;
        }
    for (std::size_t i = 0; i < p; ++i)
        M[i * p + i] *= (1.0 + lambda);
    // g = J' · r
    std::vector<double> g(p, 0.0);
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t k = 0; k < n; ++k)
            g[i] += J[i * n + k] * r[k];
    // Gauss elimination (M is row-major p×p).
    for (std::size_t k = 0; k < p; ++k) {
        std::size_t piv = k;
        double pmax = std::fabs(M[k * p + k]);
        for (std::size_t rr = k + 1; rr < p; ++rr) {
            const double v = std::fabs(M[rr * p + k]);
            if (v > pmax) { pmax = v; piv = rr; }
        }
        if (pmax == 0.0) {
            // Singular — return zero step.
            return std::vector<double>(p, 0.0);
        }
        if (piv != k) {
            for (std::size_t j = 0; j < p; ++j)
                std::swap(M[k * p + j], M[piv * p + j]);
            std::swap(g[k], g[piv]);
        }
        const double pv = M[k * p + k];
        for (std::size_t rr = k + 1; rr < p; ++rr) {
            const double f = M[rr * p + k] / pv;
            for (std::size_t j = k; j < p; ++j)
                M[rr * p + j] -= f * M[k * p + j];
            g[rr] -= f * g[k];
        }
    }
    // Back-substitute.
    for (std::size_t k = p; k-- > 0;) {
        double s = g[k];
        for (std::size_t j = k + 1; j < p; ++j)
            s -= M[k * p + j] * g[j];
        g[k] = s / M[k * p + k];
    }
    return g;
}

// Compute (J' · J)^{-1} for the covariance. Returns p × p column-major.
std::vector<double> invJtJ(const std::vector<double> &J,
                            std::size_t n, std::size_t p)
{
    std::vector<double> M(p * p, 0.0);
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = 0; j < p; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k)
                s += J[i * n + k] * J[j * n + k];
            M[i * p + j] = s;
        }
    // Invert by Gauss-Jordan with augmented identity.
    std::vector<double> A(p * 2 * p, 0.0);
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = 0; j < p; ++j)
            A[i * 2 * p + j] = M[i * p + j];
    for (std::size_t i = 0; i < p; ++i)
        A[i * 2 * p + (p + i)] = 1.0;
    for (std::size_t k = 0; k < p; ++k) {
        std::size_t piv = k;
        double pmax = std::fabs(A[k * 2 * p + k]);
        for (std::size_t rr = k + 1; rr < p; ++rr) {
            const double v = std::fabs(A[rr * 2 * p + k]);
            if (v > pmax) { pmax = v; piv = rr; }
        }
        if (pmax == 0.0) {
            return std::vector<double>(p * p, std::numeric_limits<double>::quiet_NaN());
        }
        if (piv != k) {
            for (std::size_t j = 0; j < 2 * p; ++j)
                std::swap(A[k * 2 * p + j], A[piv * 2 * p + j]);
        }
        const double pv = A[k * 2 * p + k];
        for (std::size_t j = 0; j < 2 * p; ++j)
            A[k * 2 * p + j] /= pv;
        for (std::size_t rr = 0; rr < p; ++rr) {
            if (rr == k) continue;
            const double f = A[rr * 2 * p + k];
            for (std::size_t j = 0; j < 2 * p; ++j)
                A[rr * 2 * p + j] -= f * A[k * 2 * p + j];
        }
    }
    // Extract inverse (column-major). A right half is row-major inverse.
    std::vector<double> Inv(p * p);
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = 0; j < p; ++j)
            Inv[j * p + i] = A[i * 2 * p + (p + j)];
    return Inv;
}

} // namespace

NlinfitResult nlinfit(const Value &X, const Value &y,
                      FnHandle model, const Value &beta0,
                      std::pmr::memory_resource *mr)
{
    const std::size_t n = y.numel();
    const std::size_t p = beta0.numel();
    if (p == 0)
        throw Error("nlinfit: beta0 cannot be empty",
                    0, 0, "nlinfit", "", "numkit:nlinfit:emptyBeta0");

    std::vector<double> beta = toFlatDouble(beta0);
    std::vector<double> yv = toFlatDouble(y);
    std::vector<double> rv(n);

    // Initial residual + SSE.
    auto compute_r = [&](const std::vector<double> &b) {
        auto yhat = evalModel(model, b, p, X, mr);
        std::vector<double> r(n);
        for (std::size_t i = 0; i < n; ++i) r[i] = yv[i] - yhat[i];
        return r;
    };
    rv = compute_r(beta);
    double sse = 0.0;
    for (double v : rv) sse += v * v;

    double lambda = 1e-3;
    const int maxIter = 200;
    const double tolFun = 1e-10;
    const double tolX   = 1e-10;

    std::vector<double> J;
    for (int iter = 0; iter < maxIter; ++iter) {
        J = numericalJacobian(model, beta, n, p, X, mr);
        auto dbeta = lmStep(J, n, p, rv, lambda);

        // Trial step.
        std::vector<double> betaNew(p);
        double stepNorm2 = 0.0;
        double betaNorm2 = 0.0;
        for (std::size_t j = 0; j < p; ++j) {
            betaNew[j] = beta[j] + dbeta[j];
            stepNorm2 += dbeta[j] * dbeta[j];
            betaNorm2 += betaNew[j] * betaNew[j];
        }
        auto rNew = compute_r(betaNew);
        double sseNew = 0.0;
        for (double v : rNew) sseNew += v * v;

        if (sseNew < sse) {
            // Accept.
            const double rel = std::sqrt(stepNorm2) / (std::sqrt(betaNorm2) + 1e-12);
            const double relF = (sse - sseNew) / (sse + 1e-12);
            beta = std::move(betaNew);
            rv   = std::move(rNew);
            sse  = sseNew;
            lambda *= 0.1;
            if (rel < tolX || relF < tolFun) break;
        } else {
            lambda *= 10.0;
            if (lambda > 1e16) break;     // give up
        }
    }

    // Final Jacobian (refresh at the accepted beta).
    J = numericalJacobian(model, beta, n, p, X, mr);

    // MSE = SSE / (n - p).
    const double mse = (n > p) ? sse / static_cast<double>(n - p) : sse;
    auto Inv = invJtJ(J, n, p);
    std::vector<double> CovB(p * p);
    for (std::size_t i = 0; i < p * p; ++i) CovB[i] = mse * Inv[i];

    NlinfitResult out;
    out.beta = flatToMatrix(beta.data(), p, 1, mr);
    out.R    = flatToMatrix(rv.data(),   n, 1, mr);
    out.J    = flatToMatrix(J.data(),    n, p, mr);
    out.CovB = flatToMatrix(CovB.data(), p, p, mr);
    out.MSE  = Value::scalar(mse, mr);
    return out;
}

Value nlparci(const Value &beta, const Value &R, const Value &J,
              double alpha, std::pmr::memory_resource *mr)
{
    const std::size_t p = beta.numel();
    const std::size_t n = R.numel();
    if (J.dims().rows() != n || J.dims().cols() != p)
        throw Error("nlparci: J must be (n × p) consistent with R, beta",
                    0, 0, "nlparci", "", "numkit:nlparci:shapeMismatch");
    if (n <= p)
        throw Error("nlparci: need n > p degrees of freedom",
                    0, 0, "nlparci", "", "numkit:nlparci:noDOF");

    // MSE = ||R||² / (n - p).
    double sse = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double r = R.elemAsDouble(i);
        sse += r * r;
    }
    const double mse = sse / static_cast<double>(n - p);

    // CovB = MSE · (J' · J)^{-1}.
    auto Jvec = toFlatDouble(J);
    auto Inv = invJtJ(Jvec, n, p);

    // t_{α/2, n-p}.
    Value pHi = Value::scalar(1.0 - alpha / 2.0, mr);
    const double tcrit = tinv(pHi, static_cast<double>(n - p), mr).toScalar();

    auto out = Value::matrix(p, 2, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < p; ++i) {
        const double se = std::sqrt(mse * Inv[i * p + i]);
        const double b  = beta.elemAsDouble(i);
        od[0 * p + i] = b - tcrit * se;  // lower (col 0)
        od[1 * p + i] = b + tcrit * se;  // upper (col 1)
    }
    return out;
}

std::tuple<Value, Value>
nlpredci(FnHandle model, const Value &X, const Value &beta,
         const Value &R, const Value &J,
         double alpha,
         std::pmr::memory_resource *mr)
{
    const std::size_t p = beta.numel();
    const std::size_t n = R.numel();
    const std::size_t m = X.dims().rows();
    if (J.dims().rows() != n || J.dims().cols() != p)
        throw Error("nlpredci: J must be (n × p) consistent with R, beta",
                    0, 0, "nlpredci", "", "numkit:nlpredci:shapeMismatch");
    if (n <= p)
        throw Error("nlpredci: need n > p degrees of freedom",
                    0, 0, "nlpredci", "", "numkit:nlpredci:noDOF");

    double sse = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double r = R.elemAsDouble(i);
        sse += r * r;
    }
    const double mse = sse / static_cast<double>(n - p);
    auto Jvec = toFlatDouble(J);
    auto Inv = invJtJ(Jvec, n, p);

    auto betaFlat = toFlatDouble(beta);

    // ypred at query points: just fun(beta, X).
    auto ypredFlat = evalModel(model, betaFlat, p, X, mr);

    // Per-row prediction sensitivity g_i = ∂fun/∂beta at X(i,:).
    // We compute by central diff on the full X (cheaper: one Jacobian
    // for the whole query set per parameter).
    std::vector<double> Jq(m * p);
    auto bplus = betaFlat;
    auto bminus = betaFlat;
    for (std::size_t j = 0; j < p; ++j) {
        const double scale = std::max(std::abs(betaFlat[j]), 1.0);
        const double h = 1e-7 * scale;
        bplus[j]  = betaFlat[j] + h;
        bminus[j] = betaFlat[j] - h;
        auto yp = evalModel(model, bplus,  p, X, mr);
        auto ym = evalModel(model, bminus, p, X, mr);
        bplus[j]  = betaFlat[j];
        bminus[j] = betaFlat[j];
        const double inv2h = 1.0 / (2.0 * h);
        for (std::size_t i = 0; i < m; ++i)
            Jq[j * m + i] = (yp[i] - ym[i]) * inv2h;
    }

    // t critical.
    Value pHi = Value::scalar(1.0 - alpha / 2.0, mr);
    const double tcrit = tinv(pHi, static_cast<double>(n - p), mr).toScalar();

    auto ypred = flatToMatrix(ypredFlat.data(), m, 1, mr);
    auto delta = Value::matrix(m, 1, ValueType::DOUBLE, mr);
    double *dd = delta.doubleDataMut();
    for (std::size_t i = 0; i < m; ++i) {
        // var_i = g_i' · Inv · g_i  (Inv is p × p, g_i is p × 1)
        double var = 0.0;
        for (std::size_t a = 0; a < p; ++a)
            for (std::size_t b = 0; b < p; ++b)
                var += Jq[a * m + i] * Inv[a * p + b] * Jq[b * m + i];
        dd[i] = tcrit * std::sqrt(mse * var);
    }
    return { std::move(ypred), std::move(delta) };
}

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

// ── nlinfit as an embedded `.m` wrapper (VM_CALLBACKS_PLAN.md) ────────────────
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
