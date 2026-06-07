// libs/stats/src/distributions/multivariate_detail.hpp
//
// Private (src-only) compute substrate for the multivariate distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in multivariate.cpp and
// its CallContext register half in multivariate_reg.cpp. Kept in an anonymous
// namespace (internal linkage per TU) — pure stateless math, no ODR risk.
//
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::stats {

namespace {

// In-place Cholesky factorisation of an n×n symmetric positive-definite
// matrix. On return, the lower-triangular factor lives in the lower
// half of S; upper half is zeroed. Throws if S is not PD (a diagonal
// becomes ≤ 0). Algorithm: classical Cholesky-Banachiewicz.
void choleskyLowerInPlace(double *S, std::size_t n)
{
    for (std::size_t j = 0; j < n; ++j) {
        // Diagonal entry: L[j][j] = sqrt(S[j][j] - sum_{k<j} L[j][k]^2)
        double diag = S[j * n + j];
        for (std::size_t k = 0; k < j; ++k)
            diag -= S[j * n + k] * S[j * n + k];
        if (diag <= 0.0)
            throw Error("mvnrnd: Sigma must be positive definite "
                        "(Cholesky failed at row " + std::to_string(j + 1) + ")",
                         0, 0, "mvnrnd", "", "numkit:mvnrnd:notPD");
        const double Ljj = std::sqrt(diag);
        S[j * n + j] = Ljj;
        // Below-diagonal entries: L[i][j] = (S[i][j] - sum_{k<j} L[i][k]*L[j][k]) / L[j][j]
        for (std::size_t i = j + 1; i < n; ++i) {
            double s = S[i * n + j];
            for (std::size_t k = 0; k < j; ++k)
                s -= S[i * n + k] * S[j * n + k];
            S[i * n + j] = s / Ljj;
        }
        // Zero the strictly-upper triangle for clarity.
        for (std::size_t i = 0; i < j; ++i)
            S[i * n + j] = 0.0;
    }
}

} // namespace
namespace {

// Standard normal CDF.
double phiCdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

// 16-point Gauss-Legendre quadrature on [0, 1] for the bivariate
// normal CDF via the parametric integral
//   Φ_2(h, k; ρ) = Φ(h) · Φ(k) + ∫_0^ρ φ_2(h, k; t) dt
// where φ_2 is the bivariate normal pdf.
double bivariateCdf(double h, double k, double rho)
{
    // Clamp ρ to avoid issues at the boundary.
    if (rho >  1.0 - 1e-12) rho =  1.0 - 1e-12;
    if (rho < -1.0 + 1e-12) rho = -1.0 + 1e-12;

    // Gauss-Legendre 16-point on [0, 1].
    static constexpr double GL_X[16] = {
        0.0052995325041750, 0.0277124884633837, 0.0671843988060841,
        0.1222977958224985, 0.1910618777986781, 0.2709916111713863,
        0.3591982246103705, 0.4524937450811813, 0.5475062549188187,
        0.6408017753896295, 0.7290083888286137, 0.8089381222013219,
        0.8777022041775015, 0.9328156011939160, 0.9722875115366163,
        0.9947004674958250
    };
    static constexpr double GL_W[16] = {
        0.0135762297058770, 0.0311267619693239, 0.0475792558412464,
        0.0623144856277781, 0.0747979944082885, 0.0845782596975013,
        0.0913017075224618, 0.0947253052275342, 0.0947253052275342,
        0.0913017075224618, 0.0845782596975013, 0.0747979944082885,
        0.0623144856277781, 0.0475792558412464, 0.0311267619693239,
        0.0135762297058770
    };

    double integral = 0.0;
    for (int i = 0; i < 16; ++i) {
        const double t = rho * GL_X[i];   // map [0, 1] → [0, ρ]
        const double r2 = 1.0 - t * t;
        // φ_2(h, k; t) at point (h, k) with corr t.
        const double quad = (h * h - 2.0 * t * h * k + k * k) / (2.0 * r2);
        const double pdf = std::exp(-quad) / (2.0 * M_PI * std::sqrt(r2));
        integral += GL_W[i] * pdf;
    }
    integral *= rho;
    return phiCdf(h) * phiCdf(k) + integral;
}

// Monte Carlo CDF (d ≥ 3): sample from N(mu, Sigma), count how many
// fall componentwise <= q. Antithetic sampling improves variance.
double monteCarloCdf(const double *L, std::size_t d,
                      const double *mu, const double *q,
                      std::size_t N, std::mt19937 &gen)
{
    std::normal_distribution<double> Nz(0.0, 1.0);
    std::size_t below = 0;
    std::vector<double> z(d), sample(d);
    const std::size_t halfN = N / 2;
    for (std::size_t s = 0; s < halfN; ++s) {
        for (std::size_t k = 0; k < d; ++k) z[k] = Nz(gen);
        // sample1 = mu + L * z, sample2 = mu - L * z  (antithetic).
        for (int sign = 0; sign < 2; ++sign) {
            const double sz = (sign == 0) ? 1.0 : -1.0;
            bool ok = true;
            for (std::size_t j = 0; j < d; ++j) {
                double v = mu[j];
                for (std::size_t k = 0; k <= j; ++k)
                    v += sz * L[j * d + k] * z[k];
                if (v > q[j]) { ok = false; break; }
            }
            if (ok) ++below;
        }
    }
    const std::size_t total = halfN * 2;
    return static_cast<double>(below) / static_cast<double>(total);
}

} // namespace
namespace {

// Validate C square, infer d. Return (d, n).
std::pair<std::size_t, std::size_t>
mvtcdf_shape_check(const Value &X, const Value &C, const char *fn)
{
    if (C.dims().rows() != C.dims().cols())
        throw Error(std::string(fn) + ": C must be square",
                    0, 0, fn, "", std::string("numkit:") + fn + ":notSquareC");
    const std::size_t d = C.dims().rows();
    if (d == 0) return {0, 0};
    std::size_t n;
    if (X.isScalar()) {
        if (d != 1)
            throw Error(std::string(fn) + ": X is scalar but d > 1",
                        0, 0, fn, "", std::string("numkit:") + fn + ":shapeX");
        n = 1;
    } else if (X.dims().rows() == 1 && X.dims().cols() == d) {
        n = 1;
    } else if (X.dims().cols() == d) {
        n = X.dims().rows();
    } else {
        throw Error(std::string(fn) + ": X must be 1×d or n×d",
                    0, 0, fn, "", std::string("numkit:") + fn + ":shapeX");
    }
    return {d, n};
}

// Sample-count from tol: standard error of indicator MC ≈ 1/(2·sqrt(N))
// for p near 0.5; pick N = max(10000, 1/tol²).
int mvtcdf_N_from_tol(double tol)
{
    if (!(tol > 0.0)) tol = 0.01;
    const double n_d = 1.0 / (tol * tol);
    int N = (n_d > 1e7) ? 10000000 : static_cast<int>(std::ceil(n_d));
    if (N < 10000) N = 10000;
    if (N % 2) ++N;   // antithetic pairs
    return N;
}

// Core MC: hits when L[row] < y < U[row] componentwise, antithetic.
// Lvec / Uvec are length-d (-Inf / +Inf allowed). Returns probability.
template <typename FetchLU>
double mvtcdf_mc(const std::vector<double> &Lchol, std::size_t d,
                 double df, int N, std::mt19937_64 &gen,
                 FetchLU &&fetch_LU /* (j) -> {Lj, Uj} */)
{
    std::normal_distribution<double> Nz(0.0, 1.0);
    std::chi_squared_distribution<double> Chi(df);
    std::vector<double> z(d), y(d), Lvec(d), Uvec(d);
    for (std::size_t j = 0; j < d; ++j) {
        auto [lj, uj] = fetch_LU(j);
        Lvec[j] = lj; Uvec[j] = uj;
    }
    int hits = 0;
    for (int k = 0; k < N / 2; ++k) {
        for (std::size_t j = 0; j < d; ++j) z[j] = Nz(gen);
        const double scale = std::sqrt(df / Chi(gen));
        // Pass 1: z direct.
        for (std::size_t j = 0; j < d; ++j) {
            double s = 0.0;
            for (std::size_t k2 = 0; k2 <= j; ++k2)
                s += Lchol[j * d + k2] * z[k2];
            y[j] = s * scale;
        }
        bool inside = true;
        for (std::size_t j = 0; j < d; ++j) {
            if (y[j] < Lvec[j] || y[j] > Uvec[j]) { inside = false; break; }
        }
        if (inside) ++hits;
        // Pass 2: -z antithetic.
        for (std::size_t j = 0; j < d; ++j) {
            double s = 0.0;
            for (std::size_t k2 = 0; k2 <= j; ++k2)
                s -= Lchol[j * d + k2] * z[k2];
            y[j] = s * scale;
        }
        inside = true;
        for (std::size_t j = 0; j < d; ++j) {
            if (y[j] < Lvec[j] || y[j] > Uvec[j]) { inside = false; break; }
        }
        if (inside) ++hits;
    }
    return static_cast<double>(hits) / static_cast<double>(N);
}

} // anonymous

} // namespace numkit::stats
