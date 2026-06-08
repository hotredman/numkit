// toolboxes/.../lasso_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by lasso.cpp + lasso_reg.cpp.
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

} // namespace numkit::stats
