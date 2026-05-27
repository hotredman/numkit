// libs/stats/src/regress/glm.cpp
//
// Generalized linear model fit + prediction:
//   glmfit — IRLS GLM fitting (normal / binomial / poisson / gamma)
//   glmval — predict from fitted coefficients

#include <numkit/stats/regress/regress.hpp>

#include <numkit/stats/distributions/normal.hpp>     // for probit via norminv/normcdf

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::stats {

namespace {

// In-place Gauss elimination on row-major p×p `M`, p-length `b`.
bool gaussSolve(double *M, double *b, std::size_t p)
{
    for (std::size_t k = 0; k < p; ++k) {
        std::size_t piv = k;
        double pmax = std::fabs(M[k * p + k]);
        for (std::size_t rr = k + 1; rr < p; ++rr) {
            const double v = std::fabs(M[rr * p + k]);
            if (v > pmax) { pmax = v; piv = rr; }
        }
        if (pmax == 0.0) return false;
        if (piv != k) {
            for (std::size_t j = 0; j < p; ++j)
                std::swap(M[k * p + j], M[piv * p + j]);
            std::swap(b[k], b[piv]);
        }
        const double pv = M[k * p + k];
        for (std::size_t rr = k + 1; rr < p; ++rr) {
            const double f = M[rr * p + k] / pv;
            for (std::size_t j = k; j < p; ++j)
                M[rr * p + j] -= f * M[k * p + j];
            b[rr] -= f * b[k];
        }
    }
    for (std::size_t k = p; k-- > 0;) {
        double s = b[k];
        for (std::size_t j = k + 1; j < p; ++j)
            s -= M[k * p + j] * b[j];
        b[k] = s / M[k * p + k];
    }
    return true;
}

// Link function evaluator. Computes (eta, dmu/deta, V(mu)) given mu.
//
// Returned: linkFwd(mu) -> eta, invLink(eta) -> mu, deta_dmu(mu),
// variance(mu).
struct LinkFn {
    double (*invLink)(double);          // mu = g^{-1}(eta)
    double (*deta_dmu)(double);         // g'(mu)
};

double sigmoid(double x) {
    if (x > 0)  return 1.0 / (1.0 + std::exp(-x));
    const double e = std::exp(x);
    return e / (1.0 + e);
}

double invIdentity(double e) { return e; }
double dEtaIdentity(double)  { return 1.0; }

double invLogit(double e)   { return sigmoid(e); }
double dEtaLogit(double mu) {
    const double m = std::min(std::max(mu, 1e-12), 1.0 - 1e-12);
    return 1.0 / (m * (1.0 - m));
}

double invLog(double e)   { return std::exp(e); }
double dEtaLog(double mu) {
    const double m = std::max(mu, 1e-12);
    return 1.0 / m;
}

double invRecip(double e) {
    const double r = (std::fabs(e) > 1e-300) ? e : ((e < 0) ? -1e-300 : 1e-300);
    return 1.0 / r;
}
double dEtaRecip(double mu) {
    const double m = (std::fabs(mu) > 1e-300) ? mu : 1e-300;
    return -1.0 / (m * m);
}

// Probit: g(mu) = norminv(mu), inverse = normcdf(eta).
// dEta/dMu = 1 / phi(norminv(mu)) where phi is the standard normal pdf.
double invProbit(double e) {
    // normcdf(e) = 0.5 * erfc(-e / sqrt(2))
    return 0.5 * std::erfc(-e / std::sqrt(2.0));
}
double dEtaProbit(double mu) {
    const double m = std::min(std::max(mu, 1e-12), 1.0 - 1e-12);
    // z = norminv(m) — use std::erfinv? not standard. Fall back to a
    // numerical approximation via Beasley-Springer-Moro? For v1 we
    // rely on the inverse-erf approach.
    // norminv(m) = sqrt(2) * erfinv(2m - 1). MSVC doesn't ship erfinv,
    // so use a rational approximation (Beasley-Springer / Wichura):
    const double pp = 2.0 * m - 1.0;
    // Cheap fallback: iterate one Newton step from a logit guess.
    double z = std::log(m / (1.0 - m));
    for (int i = 0; i < 5; ++i) {
        const double F = 0.5 * std::erfc(-z / std::sqrt(2.0));
        const double f = (1.0 / std::sqrt(2.0 * M_PI)) * std::exp(-0.5 * z * z);
        if (f < 1e-300) break;
        z -= (F - m) / f;
    }
    (void)pp;
    const double phi = (1.0 / std::sqrt(2.0 * M_PI)) * std::exp(-0.5 * z * z);
    return (phi > 1e-300) ? 1.0 / phi : 1e300;
}

LinkFn makeLink(GlmLink l) {
    switch (l) {
    case GlmLink::Identity:   return { invIdentity, dEtaIdentity };
    case GlmLink::Logit:      return { invLogit,    dEtaLogit };
    case GlmLink::Log:        return { invLog,      dEtaLog };
    case GlmLink::Reciprocal: return { invRecip,    dEtaRecip };
    case GlmLink::Probit:     return { invProbit,   dEtaProbit };
    }
    return { invIdentity, dEtaIdentity };
}

// Variance function V(mu) for each family.
double varianceFn(double mu, GlmDistribution d) {
    switch (d) {
    case GlmDistribution::Normal:           return 1.0;
    case GlmDistribution::Binomial: {
        const double m = std::min(std::max(mu, 1e-12), 1.0 - 1e-12);
        return m * (1.0 - m);
    }
    case GlmDistribution::Poisson:          return std::max(mu, 1e-12);
    case GlmDistribution::Gamma: {
        const double m = std::max(mu, 1e-12);
        return m * m;
    }
    case GlmDistribution::InverseGaussian: {
        const double m = std::max(mu, 1e-12);
        return m * m * m;
    }
    }
    return 1.0;
}

// Initial mu estimate for IRLS.
double initialMu(double yi, GlmDistribution d) {
    switch (d) {
    case GlmDistribution::Normal:          return yi;
    case GlmDistribution::Binomial:        return (yi + 0.5) / 2.0;  // shrink toward 0.5
    case GlmDistribution::Poisson:         return std::max(yi + 0.1, 0.1);
    case GlmDistribution::Gamma:           return std::max(yi, 0.1);
    case GlmDistribution::InverseGaussian: return std::max(yi, 0.1);
    }
    return yi;
}

// Deviance term per observation (a multiplied by 2 in the total).
double devTerm(double y, double mu, GlmDistribution d) {
    switch (d) {
    case GlmDistribution::Normal: {
        const double r = y - mu;
        return r * r;
    }
    case GlmDistribution::Binomial: {
        const double m = std::min(std::max(mu, 1e-12), 1.0 - 1e-12);
        const double yy = std::min(std::max(y,  0.0), 1.0);
        double t = 0.0;
        if (yy > 0.0)     t += yy * std::log(yy / m);
        if (yy < 1.0)     t += (1.0 - yy) * std::log((1.0 - yy) / (1.0 - m));
        return 2.0 * t;
    }
    case GlmDistribution::Poisson: {
        const double m = std::max(mu, 1e-12);
        const double yy = std::max(y, 0.0);
        const double termY = (yy > 0.0) ? yy * std::log(yy / m) : 0.0;
        return 2.0 * (termY - (yy - m));
    }
    case GlmDistribution::Gamma: {
        const double m = std::max(mu, 1e-12);
        const double yy = std::max(y, 1e-12);
        return 2.0 * (-std::log(yy / m) + (yy - m) / m);
    }
    case GlmDistribution::InverseGaussian: {
        const double m = std::max(mu, 1e-12);
        const double yy = std::max(y, 1e-12);
        const double r = (yy - m);
        return r * r / (yy * m * m);
    }
    }
    return 0.0;
}

// Pick the canonical link if Identity was passed (which we use as
// "default" sentinel).
GlmLink canonicalLink(GlmDistribution d) {
    switch (d) {
    case GlmDistribution::Normal:           return GlmLink::Identity;
    case GlmDistribution::Binomial:         return GlmLink::Logit;
    case GlmDistribution::Poisson:          return GlmLink::Log;
    case GlmDistribution::Gamma:            return GlmLink::Reciprocal;
    case GlmDistribution::InverseGaussian:  return GlmLink::Reciprocal;
    }
    return GlmLink::Identity;
}

} // namespace

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

// ── Adapters ─────────────────────────────────────────────────────────
namespace detail {

static GlmDistribution parseDistr(const std::string &s, const char *fn)
{
    if (s == "normal")             return GlmDistribution::Normal;
    if (s == "binomial")           return GlmDistribution::Binomial;
    if (s == "poisson")            return GlmDistribution::Poisson;
    if (s == "gamma")              return GlmDistribution::Gamma;
    if (s == "inverse gaussian"
        || s == "inversegaussian") return GlmDistribution::InverseGaussian;
    throw Error(std::string(fn) + ": unknown distribution '" + s + "'",
                0, 0, fn, "", std::string("numkit:") + fn + ":badDistr");
}

static GlmLink parseLink(const std::string &s, const char *fn)
{
    if (s.empty() || s == "canonical") return GlmLink::Identity;
    if (s == "identity")    return GlmLink::Identity;
    if (s == "logit")       return GlmLink::Logit;
    if (s == "log")         return GlmLink::Log;
    if (s == "reciprocal")  return GlmLink::Reciprocal;
    if (s == "probit")      return GlmLink::Probit;
    throw Error(std::string(fn) + ": unknown link '" + s + "'",
                0, 0, fn, "", std::string("numkit:") + fn + ":badLink");
}

void glmfit_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("glmfit: requires (X, y, distr [, link])",
                    0, 0, "glmfit", "", "numkit:glmfit:nargin");
    if (!args[2].isChar())
        throw Error("glmfit: distr must be a string",
                    0, 0, "glmfit", "", "numkit:glmfit:badDistr");
    const GlmDistribution d = parseDistr(args[2].toString(), "glmfit");
    GlmLink link = GlmLink::Identity;
    if (args.size() >= 4 && args[3].isChar())
        link = parseLink(args[3].toString(), "glmfit");
    auto r = glmfit(args[0], args[1], d, link, ctx.engine->resource());
    outs[0] = std::move(r.b);
    if (nargout > 1) outs[1] = std::move(r.dev);
}

void glmval_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("glmval: requires (b, X, link)",
                    0, 0, "glmval", "", "numkit:glmval:nargin");
    if (!args[2].isChar())
        throw Error("glmval: link must be a string",
                    0, 0, "glmval", "", "numkit:glmval:badLink");
    GlmLink link = parseLink(args[2].toString(), "glmval");
    outs[0] = glmval(args[0], args[1], link, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
