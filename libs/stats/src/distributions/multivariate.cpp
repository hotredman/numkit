// libs/stats/src/distributions/multivariate.cpp
//
// Multivariate distribution random / cdf primitives:
//   mvnrnd  — multivariate normal random sampler
//
// Both `mvncdf` and `mvtcdf` are KNOWN GAP (need Genz quadrature for
// integration over the multivariate normal density; v1 only covers the
// random samplers). Single-dim fallback to univariate normcdf works
// trivially if needed.

#include <numkit/builtin/math/random/rng.hpp>   // sharedEngine / rngMutex
#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "dist_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <random>
#include <vector>

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
                         0, 0, "mvnrnd", "", "m:mvnrnd:notPD");
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

// `R = mvnrnd(mu, Sigma)`           — one sample (length-d row)
// `R = mvnrnd(mu, Sigma, n)`        — n samples (n × d)
// `R = mvnrnd(MU, Sigma)`           — MU is n × d, one sample per row
//
// `mu` may be a row or column vector (length d) or an n×d matrix.
// Sigma is d × d, symmetric positive-definite (Cholesky check).
Value mvnrnd(const Value &mu, const Value &Sigma, std::size_t n,
             std::pmr::memory_resource *mr)
{
    if (Sigma.dims().rows() != Sigma.dims().cols())
        throw Error("mvnrnd: Sigma must be square",
                     0, 0, "mvnrnd", "", "m:mvnrnd:notSquare");
    const std::size_t d = Sigma.dims().rows();
    if (d == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // mu can be 1×d, d×1, or n×d. Decide n + broadcast.
    std::size_t muN = 1;
    if (!mu.isScalar()) {
        if (mu.dims().rows() == d && mu.dims().cols() == 1) muN = 1;
        else if (mu.dims().rows() == 1 && mu.dims().cols() == d) muN = 1;
        else if (mu.dims().cols() == d) muN = mu.dims().rows();
        else
            throw Error("mvnrnd: mu must be 1×d, d×1, or n×d",
                         0, 0, "mvnrnd", "", "m:mvnrnd:badMu");
    }

    if (n == 0) n = std::max(muN, std::size_t(1));
    if (muN > 1 && n != muN)
        throw Error("mvnrnd: n must equal rows(mu) when mu is matrix",
                     0, 0, "mvnrnd", "", "m:mvnrnd:nMismatch");

    ScratchArena scratch(mr);

    // Copy Sigma into a scratch column-major buffer for the in-place
    // Cholesky. We actually want ROW-major for the chol algorithm above,
    // so transpose explicitly. Sigma is symmetric so just copy as-is.
    ScratchVec<double> L(d * d, &scratch);
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            L[i * d + j] = Sigma.elemAsDouble(j * d + i);  // col-major → row-major
    choleskyLowerInPlace(L.data(), d);   // L[i*d+j] is L_ij (lower-tri)

    // mu as a flat d-length vector when row/col form; otherwise row-major n×d.
    auto getMu = [&](std::size_t row, std::size_t col) -> double {
        if (mu.isScalar()) return mu.toScalar();
        if (muN == 1) {
            // 1×d or d×1 — index by `col` regardless.
            return mu.elemAsDouble(col);
        }
        // n × d, column-major storage.
        return mu.elemAsDouble(col * muN + row);
    };

    auto out = Value::matrix(n, d, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::normal_distribution<double> Nz(0.0, 1.0);

    std::lock_guard<std::mutex> lk(mtx);
    ScratchVec<double> z(d, &scratch);
    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t k = 0; k < d; ++k) z[k] = Nz(gen);
        // sample = mu + L · z  (one row per call; column-major output)
        for (std::size_t j = 0; j < d; ++j) {
            double acc = 0.0;
            for (std::size_t k = 0; k <= j; ++k)
                acc += L[j * d + k] * z[k];
            od[j * n + r] = getMu(r, j) + acc;
        }
    }
    return out;
}

namespace detail {

void mvnrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mvnrnd: requires (mu, Sigma[, n])",
                     0, 0, "mvnrnd", "", "m:mvnrnd:nargin");
    std::size_t n = 0;
    if (args.size() >= 3) n = static_cast<std::size_t>(args[2].toScalar());
    outs[0] = mvnrnd(args[0], args[1], n, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
