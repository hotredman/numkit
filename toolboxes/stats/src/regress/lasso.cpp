// toolboxes/stats/src/regress/lasso.cpp
//
// LASSO / elastic-net (lasso) + LASSO-regularised GLM (lassoglm) via
// cyclic coordinate descent on standardised predictors.

#include <numkit/stats/regress/regress.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include "lasso_detail.hpp"

namespace numkit::stats {


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

} // namespace numkit::stats
