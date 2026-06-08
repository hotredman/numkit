// toolboxes/.../glm_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by glm.cpp + glm_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include "reduction_helpers.hpp"  // engine-free numkit::builtin::detail dim-infra (ops re-export)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
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

} // namespace numkit::stats
