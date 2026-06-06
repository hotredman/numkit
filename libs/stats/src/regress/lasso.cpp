// libs/stats/src/regress/lasso.cpp
//
// LASSO / elastic-net (lasso) + LASSO-regularised GLM (lassoglm) via
// cyclic coordinate descent on standardised predictors.

#include <numkit/stats/regress/regress.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace numkit::stats {

namespace {

// Soft-threshold operator: sign(z) · max(|z| - g, 0).
double softThreshold(double z, double g)
{
    if (z >  g) return z - g;
    if (z < -g) return z + g;
    return 0.0;
}

// Standardise X (n×p column-major) in-place: each column has mean 0,
// std 1 (population std, divides by sqrt(n)). Returns (mu, sd) vectors.
void standardise(double *X, std::size_t n, std::size_t p,
                  std::vector<double> &mu, std::vector<double> &sd)
{
    mu.assign(p, 0.0);
    sd.assign(p, 1.0);
    for (std::size_t j = 0; j < p; ++j) {
        double s = 0.0;
        for (std::size_t i = 0; i < n; ++i) s += X[j * n + i];
        mu[j] = s / static_cast<double>(n);
        double s2 = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double d = X[j * n + i] - mu[j];
            s2 += d * d;
        }
        sd[j] = std::sqrt(s2 / static_cast<double>(n));
        if (sd[j] < 1e-12) sd[j] = 1.0;   // constant column → keep as 0-coef
        for (std::size_t i = 0; i < n; ++i)
            X[j * n + i] = (X[j * n + i] - mu[j]) / sd[j];
    }
}

// One-pass coordinate descent on standardised X. Updates beta in place.
// Returns true if max coefficient change < tol.
bool coordDescentSweep(const double *Xs, std::size_t n, std::size_t p,
                        const double *y,
                        std::vector<double> &r, std::vector<double> &beta,
                        const std::vector<double> &XjNormSq,
                        double lambda_eff, double lambda_l2,
                        double tol)
{
    double maxChange = 0.0;
    for (std::size_t j = 0; j < p; ++j) {
        const double oldB = beta[j];
        // z_j = (X_j · r + ||X_j||² · β_j) / n
        double xr = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            xr += Xs[j * n + i] * r[i];
        const double z = xr + XjNormSq[j] * oldB;
        const double newB = softThreshold(z / static_cast<double>(n), lambda_eff)
                            / (XjNormSq[j] / static_cast<double>(n) + lambda_l2);
        beta[j] = newB;
        const double delta = newB - oldB;
        if (delta != 0.0) {
            for (std::size_t i = 0; i < n; ++i)
                r[i] -= delta * Xs[j * n + i];
            maxChange = std::max(maxChange, std::fabs(delta));
        }
    }
    return maxChange < tol;
}

} // namespace

LassoResult lasso(const Value &X, const Value &y, const Value &lambdas,
                   double alpha, std::pmr::memory_resource *mr)
{
    if (alpha < 0.0 || alpha > 1.0)
        throw Error("lasso: alpha must be in [0, 1]",
                    0, 0, "lasso", "", "numkit:lasso:badAlpha");

    const std::size_t n = X.dims().rows();
    const std::size_t p = X.dims().cols();
    if (y.numel() != n)
        throw Error("lasso: length(y) must equal rows(X)",
                    0, 0, "lasso", "", "numkit:lasso:shapeMismatch");
    const std::size_t nL = lambdas.numel();
    if (nL == 0)
        throw Error("lasso: lambdas must be a non-empty vector",
                    0, 0, "lasso", "", "numkit:lasso:emptyLambda");

    // Copy X (n×p, col-major) and standardise in place.
    std::vector<double> Xs(n * p);
    for (std::size_t i = 0; i < n * p; ++i) Xs[i] = X.elemAsDouble(i);
    std::vector<double> mu_X, sd_X;
    standardise(Xs.data(), n, p, mu_X, sd_X);

    // y centred.
    std::vector<double> yc(n);
    double yMean = 0.0;
    for (std::size_t i = 0; i < n; ++i) yMean += y.elemAsDouble(i);
    yMean /= static_cast<double>(n);
    for (std::size_t i = 0; i < n; ++i) yc[i] = y.elemAsDouble(i) - yMean;

    // Pre-compute ||X_j||² for each (standardised) column = n.
    std::vector<double> XjNormSq(p);
    for (std::size_t j = 0; j < p; ++j) {
        double s = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            s += Xs[j * n + i] * Xs[j * n + i];
        XjNormSq[j] = s;
    }

    // Output storage: coefficients in ORIGINAL units (transformed back
    // from standardised), intercepts per λ.
    auto Bout = Value::matrix(p, nL, ValueType::DOUBLE, mr);
    double *Bd = Bout.doubleDataMut();
    auto intOut = Value::matrix(1, nL, ValueType::DOUBLE, mr);
    double *intD = intOut.doubleDataMut();
    auto lambdaOut = Value::matrix(1, nL, ValueType::DOUBLE, mr);
    double *lamD = lambdaOut.doubleDataMut();

    // Warm-start: walk λ values largest→smallest (typical lasso path).
    std::vector<std::size_t> idx(nL);
    for (std::size_t i = 0; i < nL; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
        return lambdas.elemAsDouble(a) > lambdas.elemAsDouble(b);
    });

    std::vector<double> beta(p, 0.0);
    std::vector<double> r(yc);                  // residual on standardised X
    const int maxIter = 1000;
    const double tol = 1e-7;

    for (std::size_t k : idx) {
        const double lambda = lambdas.elemAsDouble(k);
        const double lambda_eff = alpha * lambda;
        const double lambda_l2  = (1.0 - alpha) * lambda;
        for (int it = 0; it < maxIter; ++it) {
            if (coordDescentSweep(Xs.data(), n, p, yc.data(),
                                   r, beta, XjNormSq,
                                   lambda_eff, lambda_l2, tol)) {
                break;
            }
        }
        // Untransform: β_orig = β_std / sd_X. Intercept = yMean - Σ β_orig · mu_X.
        double interc = yMean;
        for (std::size_t j = 0; j < p; ++j) {
            const double bOrig = beta[j] / sd_X[j];
            Bd[k * p + j] = bOrig;
            interc -= bOrig * mu_X[j];
        }
        intD[k] = interc;
        lamD[k] = lambda;
    }

    return { std::move(Bout), std::move(intOut), std::move(lambdaOut) };
}

// ── lassoglm — IRLS + lasso inner loop ─────────────────────────────
namespace {

// Same link / variance machinery as glm.cpp (duplicated locally to
// keep the cpp self-contained; small enough to be acceptable).
double invIdentity_g(double e) { return e; }
double dEtaIdentity_g(double) { return 1.0; }
double invLogit_g(double e) {
    if (e > 0) return 1.0 / (1.0 + std::exp(-e));
    const double ex = std::exp(e); return ex / (1.0 + ex);
}
double dEtaLogit_g(double mu) {
    const double m = std::min(std::max(mu, 1e-12), 1.0 - 1e-12);
    return 1.0 / (m * (1.0 - m));
}
double invLog_g(double e) { return std::exp(e); }
double dEtaLog_g(double mu) { return 1.0 / std::max(mu, 1e-12); }

struct LinkPair { double (*inv)(double); double (*dEta)(double); };
LinkPair pickLink(GlmDistribution d) {
    switch (d) {
    case GlmDistribution::Normal:   return { invIdentity_g, dEtaIdentity_g };
    case GlmDistribution::Binomial: return { invLogit_g,    dEtaLogit_g };
    case GlmDistribution::Poisson:  return { invLog_g,      dEtaLog_g };
    default: return { invIdentity_g, dEtaIdentity_g };  // gamma/IG fall back
    }
}

double varG(double mu, GlmDistribution d) {
    switch (d) {
    case GlmDistribution::Normal:   return 1.0;
    case GlmDistribution::Binomial: {
        const double m = std::min(std::max(mu, 1e-12), 1.0 - 1e-12);
        return m * (1.0 - m);
    }
    case GlmDistribution::Poisson:  return std::max(mu, 1e-12);
    default: return 1.0;
    }
}

double initMu(double yi, GlmDistribution d) {
    switch (d) {
    case GlmDistribution::Normal:   return yi;
    case GlmDistribution::Binomial: return (yi + 0.5) * 0.5;
    case GlmDistribution::Poisson:  return std::max(yi + 0.1, 0.1);
    default: return std::max(yi, 0.1);
    }
}

} // namespace

LassoResult lassoglm(const Value &X, const Value &y,
                      GlmDistribution distr,
                      const Value &lambdas, double alpha,
                      std::pmr::memory_resource *mr)
{
    if (alpha < 0.0 || alpha > 1.0)
        throw Error("lassoglm: alpha must be in [0, 1]",
                    0, 0, "lassoglm", "", "numkit:lassoglm:badAlpha");

    const std::size_t n = X.dims().rows();
    const std::size_t p = X.dims().cols();
    if (y.numel() != n)
        throw Error("lassoglm: length(y) must equal rows(X)",
                    0, 0, "lassoglm", "", "numkit:lassoglm:shapeMismatch");
    const std::size_t nL = lambdas.numel();
    if (nL == 0)
        throw Error("lassoglm: lambdas must be a non-empty vector",
                    0, 0, "lassoglm", "", "numkit:lassoglm:emptyLambda");

    const LinkPair link = pickLink(distr);

    std::vector<double> Xs(n * p);
    for (std::size_t i = 0; i < n * p; ++i) Xs[i] = X.elemAsDouble(i);
    std::vector<double> mu_X, sd_X;
    standardise(Xs.data(), n, p, mu_X, sd_X);

    std::vector<double> yv(n);
    for (std::size_t i = 0; i < n; ++i) yv[i] = y.elemAsDouble(i);

    auto Bout = Value::matrix(p, nL, ValueType::DOUBLE, mr);
    double *Bd = Bout.doubleDataMut();
    auto intOut = Value::matrix(1, nL, ValueType::DOUBLE, mr);
    double *intD = intOut.doubleDataMut();
    auto lambdaOut = Value::matrix(1, nL, ValueType::DOUBLE, mr);
    double *lamD = lambdaOut.doubleDataMut();

    // Lambda path largest → smallest.
    std::vector<std::size_t> idx(nL);
    for (std::size_t i = 0; i < nL; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
        return lambdas.elemAsDouble(a) > lambdas.elemAsDouble(b);
    });

    std::vector<double> beta(p, 0.0);
    double b0 = 0.0;
    // Initialise eta = b0 + Xs · β = b0; mu = invLink(eta).
    std::vector<double> mu(n), eta(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) mu[i] = initMu(yv[i], distr);
    // Bootstrap eta from mu via family-appropriate transform.
    auto resetEta = [&]() {
        for (std::size_t i = 0; i < n; ++i) {
            switch (distr) {
            case GlmDistribution::Normal:   eta[i] = mu[i]; break;
            case GlmDistribution::Binomial: {
                const double m = std::min(std::max(mu[i], 1e-12), 1.0 - 1e-12);
                eta[i] = std::log(m / (1.0 - m)); break;
            }
            case GlmDistribution::Poisson:
                eta[i] = std::log(std::max(mu[i], 1e-12)); break;
            default: eta[i] = mu[i];
            }
        }
    };
    resetEta();

    for (std::size_t k : idx) {
        const double lambda = lambdas.elemAsDouble(k);
        const double lambda_eff = alpha * lambda;
        const double lambda_l2  = (1.0 - alpha) * lambda;
        constexpr int outerIter = 25;
        for (int outer = 0; outer < outerIter; ++outer) {
            // Working response and weights.
            std::vector<double> z(n), w(n);
            for (std::size_t i = 0; i < n; ++i) {
                const double dE = link.dEta(mu[i]);
                const double V = varG(mu[i], distr);
                w[i] = 1.0 / (V * dE * dE + 1e-30);
                z[i] = eta[i] + (yv[i] - mu[i]) * dE;
            }
            // Centre z by weighted mean (so intercept = weighted-mean(z)).
            double wsum = 0.0, wz = 0.0;
            for (std::size_t i = 0; i < n; ++i) { wsum += w[i]; wz += w[i] * z[i]; }
            const double zMean = wz / std::max(wsum, 1e-30);
            std::vector<double> zc(n);
            for (std::size_t i = 0; i < n; ++i) zc[i] = z[i] - zMean;

            // Weighted lasso inner coord descent. We absorb weights
            // into the variables: X_jw = sqrt(w) · X_j, z_w = sqrt(w) · z.
            std::vector<double> Xw(n * p), zw(n);
            for (std::size_t i = 0; i < n; ++i) {
                const double sw = std::sqrt(w[i]);
                zw[i] = sw * zc[i];
                for (std::size_t j = 0; j < p; ++j)
                    Xw[j * n + i] = sw * Xs[j * n + i];
            }
            std::vector<double> XjNorm(p);
            for (std::size_t j = 0; j < p; ++j) {
                double s = 0.0;
                for (std::size_t i = 0; i < n; ++i)
                    s += Xw[j * n + i] * Xw[j * n + i];
                XjNorm[j] = s;
            }
            std::vector<double> r(zw);
            for (std::size_t i = 0; i < n; ++i)
                for (std::size_t j = 0; j < p; ++j)
                    r[i] -= Xw[j * n + i] * beta[j];

            constexpr int innerIter = 500;
            const double tol = 1e-6;
            for (int inner = 0; inner < innerIter; ++inner) {
                if (coordDescentSweep(Xw.data(), n, p, zw.data(),
                                       r, beta, XjNorm,
                                       lambda_eff, lambda_l2, tol)) {
                    break;
                }
            }

            // Update eta from new β (intercept = zMean - Σ β · mu_X / sd_X).
            b0 = zMean;
            for (std::size_t i = 0; i < n; ++i) {
                double e = b0;
                for (std::size_t j = 0; j < p; ++j)
                    e += beta[j] * Xs[j * n + i];
                eta[i] = e;
                mu[i] = link.inv(e);
            }
        }
        // Untransform.
        double interc = b0;
        for (std::size_t j = 0; j < p; ++j) {
            const double bOrig = beta[j] / sd_X[j];
            Bd[k * p + j] = bOrig;
            interc -= bOrig * mu_X[j];
        }
        intD[k] = interc;
        lamD[k] = lambda;
    }

    return { std::move(Bout), std::move(intOut), std::move(lambdaOut) };
}

// ── Adapters ─────────────────────────────────────────────────────────
namespace detail {

void lasso_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("lasso: requires (X, y, lambdas [, alpha])",
                    0, 0, "lasso", "", "numkit:lasso:nargin");
    double alpha = 1.0;
    if (args.size() >= 4 && !args[3].isEmpty())
        alpha = args[3].toScalar();
    auto r = lasso(args[0], args[1], args[2], alpha, ctx.engine->resource());
    outs[0] = std::move(r.B);
    if (nargout > 1) outs[1] = std::move(r.Intercept);
    if (nargout > 2) outs[2] = std::move(r.Lambda);
}

void lassoglm_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("lassoglm: requires (X, y, distr, lambdas [, alpha])",
                    0, 0, "lassoglm", "", "numkit:lassoglm:nargin");
    if (!args[2].isChar())
        throw Error("lassoglm: distr must be a string",
                    0, 0, "lassoglm", "", "numkit:lassoglm:badDistr");
    const std::string s = args[2].toString();
    GlmDistribution d;
    if (s == "normal")        d = GlmDistribution::Normal;
    else if (s == "binomial") d = GlmDistribution::Binomial;
    else if (s == "poisson")  d = GlmDistribution::Poisson;
    else
        throw Error("lassoglm: unsupported distribution '" + s
                    + "' (v1: normal, binomial, poisson)",
                    0, 0, "lassoglm", "", "numkit:lassoglm:badDistr");
    double alpha = 1.0;
    if (args.size() >= 5 && !args[4].isEmpty())
        alpha = args[4].toScalar();
    auto r = lassoglm(args[0], args[1], d, args[3], alpha, ctx.engine->resource());
    outs[0] = std::move(r.B);
    if (nargout > 1) outs[1] = std::move(r.Intercept);
    if (nargout > 2) outs[2] = std::move(r.Lambda);
}

} // namespace detail
} // namespace numkit::stats
