// toolboxes/stats/src/regress/glm.cpp
//
// Generalized linear model fit + prediction:
//   glmfit — IRLS GLM fitting (normal / binomial / poisson / gamma)
//   glmval — predict from fitted coefficients

#include <numkit/stats/regress/regress.hpp>

#include <numkit/stats/distributions/normal.hpp>     // for probit via norminv/normcdf

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "glm_detail.hpp"

namespace numkit::stats {


GlmfitResult glmfit(const Value &X, const Value &y,
                     GlmDistribution distr, GlmLink link,
                     std::pmr::memory_resource *mr)
{
    const std::size_t n = y.numel();
    if (X.dims().rows() != n)
        throw Error("glmfit: rows(X) must equal length(y)",
                    0, 0, "glmfit", "", "numkit:glmfit:shapeMismatch");
    const std::size_t p_in = X.dims().cols();
    const std::size_t p = p_in + 1;   // +1 for auto intercept
    if (n <= p)
        throw Error("glmfit: need rows(X) > cols(X) + 1",
                    0, 0, "glmfit", "", "numkit:glmfit:noDOF");

    // If user passed Identity AND it's not Normal, treat that as
    // "default" and switch to canonical link.
    if (link == GlmLink::Identity && distr != GlmDistribution::Normal)
        link = canonicalLink(distr);

    const LinkFn fn = makeLink(link);

    // Build design matrix Xb = [1, X] column-major.
    std::vector<double> Xb(n * p, 0.0);
    for (std::size_t i = 0; i < n; ++i) Xb[0 * n + i] = 1.0;
    for (std::size_t j = 0; j < p_in; ++j)
        for (std::size_t i = 0; i < n; ++i)
            Xb[(j + 1) * n + i] = X.elemAsDouble(j * n + i);

    std::vector<double> yv(n);
    for (std::size_t i = 0; i < n; ++i) yv[i] = y.elemAsDouble(i);

    // Initial mu and eta.
    std::vector<double> mu(n), eta(n);
    for (std::size_t i = 0; i < n; ++i) {
        mu[i] = initialMu(yv[i], distr);
        // eta = g(mu) — compute via numerical inverse:
        // We don't have direct g(mu) symbolic, but we can use the
        // identity g'(mu) = dEta/dMu and iterate. Simpler: bootstrap
        // eta from a transform consistent with the link.
        switch (link) {
        case GlmLink::Identity:   eta[i] = mu[i]; break;
        case GlmLink::Logit:      eta[i] = std::log(mu[i] / (1.0 - mu[i])); break;
        case GlmLink::Log:        eta[i] = std::log(std::max(mu[i], 1e-12)); break;
        case GlmLink::Reciprocal: eta[i] = 1.0 / std::max(mu[i], 1e-12); break;
        case GlmLink::Probit: {
            // bootstrap via logit-ish guess
            const double m = std::min(std::max(mu[i], 1e-12), 1.0 - 1e-12);
            eta[i] = std::log(m / (1.0 - m)) / 1.6;   // rough scaling
            break;
        }
        }
    }

    std::vector<double> beta(p, 0.0), beta_prev(p, 0.0);
    const int maxIter = 100;
    const double tol = 1e-8;
    double dev = 0.0;
    for (int it = 0; it < maxIter; ++it) {
        // Working response z = eta + (y - mu) * dEta/dMu, weight = 1 / (V(mu) * (dEta/dMu)²).
        std::vector<double> z(n), w(n);
        for (std::size_t i = 0; i < n; ++i) {
            const double dE = fn.deta_dmu(mu[i]);
            const double V = varianceFn(mu[i], distr);
            w[i] = 1.0 / (V * dE * dE + 1e-30);
            z[i] = eta[i] + (yv[i] - mu[i]) * dE;
        }

        // Solve weighted least squares: (X' W X) β = X' W z.
        std::vector<double> M(p * p, 0.0), b(p, 0.0);
        for (std::size_t i = 0; i < p; ++i) {
            for (std::size_t j = 0; j < p; ++j) {
                double s = 0.0;
                for (std::size_t k = 0; k < n; ++k)
                    s += Xb[i * n + k] * w[k] * Xb[j * n + k];
                M[i * p + j] = s;
            }
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k)
                s += Xb[i * n + k] * w[k] * z[k];
            b[i] = s;
        }
        if (!gaussSolve(M.data(), b.data(), p)) {
            // Singular — bail with current beta.
            break;
        }

        beta_prev = beta;
        beta = b;

        // Update eta = X·β, then mu.
        for (std::size_t i = 0; i < n; ++i) {
            double e = 0.0;
            for (std::size_t j = 0; j < p; ++j)
                e += Xb[j * n + i] * beta[j];
            eta[i] = e;
            mu[i] = fn.invLink(e);
        }

        // Convergence on coefficient change.
        double maxStep = 0.0, maxBeta = 0.0;
        for (std::size_t j = 0; j < p; ++j) {
            maxStep = std::max(maxStep, std::fabs(beta[j] - beta_prev[j]));
            maxBeta = std::max(maxBeta, std::fabs(beta[j]));
        }
        if (it > 0 && maxStep < tol * std::max(maxBeta, 1.0)) break;
    }

    // Deviance.
    dev = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        dev += devTerm(yv[i], mu[i], distr);

    auto bv = Value::matrix(p, 1, ValueType::DOUBLE, mr);
    std::memcpy(bv.doubleDataMut(), beta.data(), p * sizeof(double));
    return { std::move(bv), Value::scalar(dev, mr) };
}

Value glmval(const Value &b, const Value &X, GlmLink link,
              std::pmr::memory_resource *mr)
{
    const std::size_t p = b.numel();
    const std::size_t m = X.dims().rows();
    const std::size_t p_in = X.dims().cols();
    if (p != p_in + 1)
        throw Error("glmval: length(b) must equal cols(X) + 1",
                    0, 0, "glmval", "", "numkit:glmval:shapeMismatch");

    const LinkFn fn = makeLink(link);
    std::vector<double> bv(p);
    for (std::size_t i = 0; i < p; ++i) bv[i] = b.elemAsDouble(i);

    auto out = Value::matrix(m, 1, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < m; ++i) {
        double eta = bv[0];      // intercept
        for (std::size_t j = 0; j < p_in; ++j)
            eta += X.elemAsDouble(j * m + i) * bv[j + 1];
        od[i] = fn.invLink(eta);
    }
    return out;
}

} // namespace numkit::stats
