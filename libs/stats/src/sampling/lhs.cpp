// libs/stats/src/sampling/lhs.cpp
//
// Latin Hypercube sampling (lhsdesign / lhsnorm).

#include <numkit/stats/sampling/lhs.hpp>
#include <numkit/stats/distributions/normal.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <numeric>
#include <random>
#include <vector>

namespace numkit::stats {

namespace {

// In-place Cholesky factorisation (upper triangle on return; lower zeroed).
// MATLAB's chol(Sigma) returns the upper R such that R' * R = Sigma.
void choleskyUpperInPlace(double *S, std::size_t n)
{
    // Standard column-by-column Cholesky producing the upper triangle.
    // We treat S as row-major.
    for (std::size_t j = 0; j < n; ++j) {
        double diag = S[j * n + j];
        for (std::size_t k = 0; k < j; ++k)
            diag -= S[k * n + j] * S[k * n + j];
        if (diag <= 0.0)
            throw Error("lhsnorm: Sigma must be positive definite "
                        "(Cholesky failed at row " + std::to_string(j + 1) + ")",
                        0, 0, "lhsnorm", "", "m:lhsnorm:notPD");
        const double Rjj = std::sqrt(diag);
        S[j * n + j] = Rjj;
        for (std::size_t i = j + 1; i < n; ++i) {
            double s = S[j * n + i];
            for (std::size_t k = 0; k < j; ++k)
                s -= S[k * n + j] * S[k * n + i];
            S[j * n + i] = s / Rjj;
        }
        // Zero strict-lower triangle column for clarity.
        for (std::size_t i = j + 1; i < n; ++i)
            S[i * n + j] = 0.0;
    }
}

} // anonymous

Value lhsdesign(std::size_t n, std::size_t p,
                std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(n, p, ValueType::DOUBLE, mr);
    if (n == 0 || p == 0) return out;
    double *od = out.doubleDataMut();   // col-major

    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::uniform_real_distribution<double> U(0.0, 1.0);

    std::lock_guard<std::mutex> lk(mtx);
    std::vector<std::size_t> perm(n);
    const double inv_n = 1.0 / static_cast<double>(n);
    for (std::size_t j = 0; j < p; ++j) {
        // Random permutation of 1..n (Fisher-Yates).
        std::iota(perm.begin(), perm.end(), std::size_t(1));
        for (std::size_t i = n - 1; i > 0; --i) {
            std::uniform_int_distribution<std::size_t> dist(0, i);
            const std::size_t k = dist(gen);
            std::swap(perm[i], perm[k]);
        }
        for (std::size_t i = 0; i < n; ++i) {
            const double u = U(gen);
            // X[i, j] = (perm[i] - u) / n
            od[j * n + i] = (static_cast<double>(perm[i]) - u) * inv_n;
        }
    }
    return out;
}

Value lhsnorm(const Value &mu, const Value &Sigma, std::size_t n,
              std::pmr::memory_resource *mr)
{
    // Resolve d from mu's length, validate against Sigma's shape.
    const std::size_t d = mu.numel();
    if (d == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (Sigma.dims().rows() != d || Sigma.dims().cols() != d)
        throw Error("lhsnorm: Sigma must be d × d where d = length(mu)",
                    0, 0, "lhsnorm", "", "m:lhsnorm:shape");

    // U is the LHS uniform design (n × d).
    Value U = lhsdesign(n, d, mr);
    // Z = norminv(U), n × d standard-normal samples.
    Value Z = norminv(U, 0.0, 1.0, mr);

    // Build the Cholesky factor R (upper) of Sigma, row-major.
    std::vector<double> R(d * d);
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            R[i * d + j] = Sigma.elemAsDouble(j * d + i);   // col-major → row-major copy
    choleskyUpperInPlace(R.data(), d);

    // mu values (row vector).
    std::vector<double> muv(d);
    for (std::size_t k = 0; k < d; ++k) muv[k] = mu.elemAsDouble(k);

    auto out = Value::matrix(n, d, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();   // col-major

    if (n == 0) return out;

    // Y[i, j] = mu[j] + Σ_k Z[i, k] * R[k, j]
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < d; ++j) {
            double s = muv[j];
            for (std::size_t k = 0; k <= j; ++k)
                s += Z.elemAsDouble(k * n + i) * R[k * d + j];
            od[j * n + i] = s;
        }
    }
    return out;
}

namespace detail {

void lhsdesign_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lhsdesign: requires (n, p)",
                    0, 0, "lhsdesign", "", "m:lhsdesign:nargin");
    const std::size_t n = static_cast<std::size_t>(args[0].toScalar());
    const std::size_t p = static_cast<std::size_t>(args[1].toScalar());
    outs[0] = lhsdesign(n, p, ctx.engine->resource());
}

void lhsnorm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("lhsnorm: requires (mu, Sigma, n)",
                    0, 0, "lhsnorm", "", "m:lhsnorm:nargin");
    const std::size_t n = static_cast<std::size_t>(args[2].toScalar());
    outs[0] = lhsnorm(args[0], args[1], n, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
