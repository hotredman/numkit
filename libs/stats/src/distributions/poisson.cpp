// libs/stats/src/distributions/poisson.cpp

#include <numkit/stats/distributions/poisson.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

namespace numkit::stats {

namespace {

template <typename Op>
Value elementwise(std::pmr::memory_resource *mr, const Value &x, Op op)
{
    if (x.isScalar()) return Value::scalar(op(x.toScalar()), mr);
    const auto &d = x.dims();
    Value out;
    if (d.is3D()) out = Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr);
    else          out = Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    const size_t n = x.numel();
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i) od[i] = op(x.elemAsDouble(i));
    return out;
}

// log Γ(k+1) = log k! ; we use lgamma for stability across the full range.
inline double poiss_pmf(double k, double lambda) {
    if (k < 0.0 || std::floor(k) != k) return 0.0;
    // log f = k·log λ - λ - lgamma(k+1)
    if (lambda == 0.0) return (k == 0.0) ? 1.0 : 0.0;
    return std::exp(k * std::log(lambda) - lambda - std::lgamma(k + 1.0));
}

inline double poiss_cdf_scalar(double k, double lambda) {
    if (k < 0.0) return 0.0;
    if (lambda == 0.0) return 1.0;
    const double kfloor = std::floor(k);
    // F(k; λ) = Q(⌊k⌋+1, λ) = 1 - P(⌊k⌋+1, λ)
    // For tiny ⌊k⌋ it's faster to just sum the pmf — but use Q for stability.
    return std::tgamma(kfloor + 1.0) > 0.0
         ? 1.0 - 0.0  // dead branch placeholder; we won't take this path
         : 0.0;
}

} // anonymous

Value poisspdf(std::pmr::memory_resource *mr, const Value &k, double lambda)
{
    if (lambda < 0.0)
        return elementwise(mr, k, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, k, [=](double ki){ return poiss_pmf(ki, lambda); });
}

Value poisscdf(std::pmr::memory_resource *mr, const Value &k, double lambda)
{
    if (lambda < 0.0)
        return elementwise(mr, k, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    if (lambda == 0.0)
        return elementwise(mr, k, [](double ki){ return ki >= 0.0 ? 1.0 : 0.0; });
    // Build a vector x = floor(k) + 1, run gammainc(λ, x), then F = 1 - that.
    Value xs = elementwise(mr, k, [=](double ki) {
        if (ki < 0.0) return 0.0;        // clamp; gammainc(λ, 0) sentinel
        return std::floor(ki) + 1.0;
    });
    Value lam = Value::scalar(lambda, mr);
    Value lower = ::numkit::builtin::gammainc(mr, lam, xs);
    // F = 1 - P, but: for ki < 0, xs = 0 and we want F = 0. gammainc(λ, 0) is
    // technically undefined; if it returns 0, F = 1 — wrong. Patch via a
    // walk that knows the original ki.
    Value out;
    const auto &d = k.dims();
    if (k.isScalar()) {
        const double ki = k.toScalar();
        if (ki < 0.0) return Value::scalar(0.0, mr);
        return Value::scalar(1.0 - lower.elemAsDouble(0), mr);
    }
    if (d.is3D()) out = Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr);
    else          out = Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    const size_t n = k.numel();
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i) {
        const double ki = k.elemAsDouble(i);
        if (ki < 0.0) { od[i] = 0.0; continue; }
        od[i] = 1.0 - lower.elemAsDouble(i);
    }
    return out;
}

namespace {

// Walk the cdf upward until F(k) ≥ p. For large λ, start from a normal-
// approximation guess and walk in either direction.
inline double poiss_inv_scalar(double p, double lambda) {
    if (!(p >= 0.0 && p <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
    if (p == 0.0) return 0.0;
    if (p >= 1.0) return std::numeric_limits<double>::infinity();
    if (lambda == 0.0) return 0.0;

    // Initial guess: ⌊λ + √λ · Φ⁻¹(p)⌋ — same trick MATLAB uses.
    // Approximate Φ⁻¹ via a coarse rational approx; we don't need precision
    // since we walk to the exact answer.
    double k = 0.0;
    if (lambda >= 10.0) {
        // Crude Φ⁻¹ approximation good enough for a start.
        const double t = std::sqrt(-2.0 * std::log(std::min(p, 1.0 - p)));
        const double num = 2.515517 + 0.802853 * t + 0.010328 * t * t;
        const double den = 1.0 + 1.432788 * t + 0.189269 * t * t + 0.001308 * t * t * t;
        const double z = (p < 0.5) ? -(t - num / den) : (t - num / den);
        k = std::floor(lambda + std::sqrt(lambda) * z);
        if (k < 0.0) k = 0.0;
    }

    auto cdfAt = [&](double kk) {
        if (kk < 0.0) return 0.0;
        // Avoid recursion into Value-API: compute via lgamma + gammainc-equivalent.
        // We can call a small private summation since k is integer; for large λ
        // and large k that's slow, so use the Q(s,x) identity via std::tgamma
        // would also be heavy. Use forward summation here — λ is bounded by user
        // and k stays close to it.
        const double kf = std::floor(kk);
        // F(kf;λ) = sum_{j=0..kf} pmf(j;λ)
        // To keep this O(kf) bounded by the search, we do it iteratively
        // outside. For the binary check we use forward recurrence.
        (void)kf; (void)lambda;
        return 0.0; // unused
    };
    (void)cdfAt;

    // We'll instead do an iterative upward/downward walk from k using the
    // recurrence pmf(j+1) = pmf(j) · λ / (j+1). Start by computing F(k).
    auto pmf_at = [&](double j) { return poiss_pmf(j, lambda); };

    auto cdf_at = [&](double kk) {
        if (kk < 0.0) return 0.0;
        const double kf = std::floor(kk);
        double f = std::exp(-lambda);
        double s = f;
        for (double j = 1.0; j <= kf; j += 1.0) {
            f *= lambda / j;
            s += f;
        }
        return std::min(1.0, std::max(0.0, s));
    };

    // Forward-sum cdf and gammainc-based public cdf can disagree by ~1 ULP
    // when p is itself produced by poisscdf — apply a small relative
    // tolerance so round-trips don't overshoot by one unit.
    const double tol = std::max(1e-13, p * 1e-13);

    double cur = cdf_at(k);
    if (cur >= p - tol) {
        // walk down
        while (k > 0.0) {
            const double prev = cur - pmf_at(k);
            if (prev < p - tol) return k;
            cur = prev;
            k -= 1.0;
        }
        return 0.0;
    } else {
        // walk up
        while (cur < p - tol && k < 1e18) {
            k += 1.0;
            cur += pmf_at(k);
            if (cur >= p - tol) return k;
        }
        return k;
    }
}

} // anonymous

Value poissinv(std::pmr::memory_resource *mr, const Value &p, double lambda)
{
    if (lambda < 0.0)
        return elementwise(mr, p, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, p, [=](double pi){ return poiss_inv_scalar(pi, lambda); });
}

Value poissrnd(std::pmr::memory_resource *mr, double lambda, size_t rows, size_t cols)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (lambda < 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    if (lambda == 0.0) {
        for (size_t i = 0; i < n; ++i) od[i] = 0.0;
        return out;
    }
    std::poisson_distribution<int> pd(lambda);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) od[i] = static_cast<double>(pd(gen));
    return out;
}

std::tuple<double, double> poisstat(double lambda)
{
    // MATLAB convention: lambda <= 0 ⇒ NaN/NaN (degenerate at 0).
    if (lambda <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(lambda, lambda);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void poisspdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("poisspdf: requires (k, lambda)", 0, 0, "poisspdf", "", "m:poisspdf:nargin");
    outs[0] = poisspdf(ctx.engine->resource(), args[0], args[1].toScalar());
}

void poisscdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 2)
        throw Error("poisscdf: requires (k, lambda[, 'upper'])", 0, 0, "poisscdf", "", "m:poisscdf:nargin");
    Value v = poisscdf(ctx.engine->resource(), args[0], args[1].toScalar());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void poissinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("poissinv: requires (p, lambda)", 0, 0, "poissinv", "", "m:poissinv:nargin");
    outs[0] = poissinv(ctx.engine->resource(), args[0], args[1].toScalar());
}

void poissrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("poissrnd: requires lambda[, sz...]", 0, 0, "poissrnd", "", "m:poissrnd:nargin");
    const double lambda = args[0].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 1, rows, cols);
    outs[0] = poissrnd(ctx.engine->resource(), lambda, rows, cols);
}

void poisstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx, "poisstat",
                       [](double lambda) { return poisstat(lambda); });
}

} // namespace detail
} // namespace numkit::stats
