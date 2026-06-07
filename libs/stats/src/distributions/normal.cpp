// libs/stats/src/distributions/normal.cpp
//
// Normal distribution. Standard formulas:
//   pdf:  f(x; μ, σ) = (1/(σ√(2π))) · exp(-(x-μ)² / (2σ²))
//   cdf:  F(x; μ, σ) = ½ · [1 + erf((x-μ) / (σ√2))]
//   icdf: F⁻¹(p; μ, σ) = μ + σ·√2·erfinv(2p - 1)
//   rnd:  μ + σ · randn
//   stat: [mean=μ, var=σ²]

#include <numkit/stats/distributions/normal.hpp>

#include <numkit/builtin/math/random/rng.hpp>          // sharedEngine, randn
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "dist_helpers.hpp"            // stripUpperFlag / applyUpperInPlace

#include <cmath>
#include <cstring>
#include <mutex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::stats {

namespace {

constexpr double kSqrt2  = 1.41421356237309504880;
constexpr double kSqrt2Pi = 2.50662827463100050241;

// Apply f(x[i]) elementwise into a fresh DOUBLE Value of same shape.
template <typename Op>
Value elementwise(const Value &x, Op op, std::pmr::memory_resource *mr)
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

// MATLAB's erfinv uses the Beasley-Springer-Moro initial + 1 Newton
// step. For our purposes the existing builtin::erfinv is sufficient
// (already tested at ULP-10 in the parity bench).

double erfinvScalar(double y)
{
    // Use the Winitzki approximation as a closed-form fallback if a
    // direct erfinv isn't reachable from the stats lib without a
    // public dep on libs/builtin internals. Accuracy ~1e-3; we then
    // refine with one Newton step on erf.
    if (y >= 1.0) return std::numeric_limits<double>::infinity();
    if (y <= -1.0) return -std::numeric_limits<double>::infinity();
    const double a = 0.147;
    const double ln1m = std::log(1.0 - y * y);
    const double term = 2.0 / (M_PI * a) + 0.5 * ln1m;
    double x = std::copysign(std::sqrt(std::sqrt(term * term - ln1m / a) - term), y);
    // 2 Newton iterations on erf(x) - y for full precision.
    for (int i = 0; i < 2; ++i) {
        const double e = std::erf(x) - y;
        const double de = 2.0 * std::exp(-x * x) / std::sqrt(M_PI);
        x -= e / de;
    }
    return x;
}

// ── Scalar kernels (single source of truth) ──────────────────────────
// Each owns its per-element domain handling so they can be broadcast over
// vector parameters: sigma<=0 (or NaN) → NaN, matching MATLAB R2025b.

inline double normpdfK(double x, double mu, double sigma)
{
    if (!(sigma > 0.0)) return std::numeric_limits<double>::quiet_NaN();
    const double inv = 1.0 / (sigma * kSqrt2Pi);
    const double inv2s2 = 1.0 / (2.0 * sigma * sigma);
    const double d = x - mu;
    return inv * std::exp(-d * d * inv2s2);
}

inline double normcdfK(double x, double mu, double sigma)
{
    if (!(sigma > 0.0)) return std::numeric_limits<double>::quiet_NaN();
    return 0.5 * (1.0 + std::erf((x - mu) / (sigma * kSqrt2)));
}

inline double norminvK(double p, double mu, double sigma)
{
    if (!(sigma > 0.0) || std::isnan(p) || p < 0.0 || p > 1.0)
        return std::numeric_limits<double>::quiet_NaN();
    if (p == 0.0) return -std::numeric_limits<double>::infinity();
    if (p == 1.0) return  std::numeric_limits<double>::infinity();
    return mu + sigma * kSqrt2 * erfinvScalar(2.0 * p - 1.0);
}

} // anonymous

Value normpdf(const Value &x, double mu, double sigma, std::pmr::memory_resource *mr)
{
    return elementwise(x, [=](double xi) { return normpdfK(xi, mu, sigma); }, mr);
}

Value normcdf(const Value &x, double mu, double sigma, std::pmr::memory_resource *mr)
{
    return elementwise(x, [=](double xi) { return normcdfK(xi, mu, sigma); }, mr);
}

Value norminv(const Value &p, double mu, double sigma, std::pmr::memory_resource *mr)
{
    // Boundary p ∈ {0, 1} → ±Inf; out-of-range or NaN p → NaN; sigma<=0 → NaN
    // (matches MATLAB R2025b — Octave too). See norminvK.
    return elementwise(p, [=](double pi) { return norminvK(pi, mu, sigma); }, mr);
}

Value normrnd(double mu, double sigma, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    if (sigma < 0.0) sigma = 0.0;
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::normal_distribution<double> nd(mu, sigma);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) od[i] = nd(gen);
    return out;
}

std::tuple<double, double> normstat(double mu, double sigma)
{
    // MATLAB convention: sigma <= 0 ⇒ NaN/NaN (matches Octave too).
    if (sigma <= 0.0) {
        const double NaN = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(NaN, NaN);
    }
    return std::make_tuple(mu, sigma * sigma);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void normpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("normpdf: requires at least 1 argument",
                     0, 0, "normpdf", "", "numkit:normpdf:nargin");
    auto *mr = ctx.engine->resource();
    Value hmu, hsig;
    const Value &mu  = dist_param(args, 1, 0.0, mr, hmu);
    const Value &sig = dist_param(args, 2, 1.0, mr, hsig);
    outs[0] = broadcast_dist3(args[0], mu, sig, mr, "normpdf", normpdfK);
}

void normcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("normcdf: requires at least 1 argument",
                     0, 0, "normcdf", "", "numkit:normcdf:nargin");
    auto *mr = ctx.engine->resource();
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    Value hmu, hsig;
    const Value &mu  = dist_param(a, 1, 0.0, mr, hmu);
    const Value &sig = dist_param(a, 2, 1.0, mr, hsig);
    Value v = broadcast_dist3(a[0], mu, sig, mr, "normcdf", normcdfK);
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void norminv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("norminv: requires at least 1 argument",
                     0, 0, "norminv", "", "numkit:norminv:nargin");
    auto *mr = ctx.engine->resource();
    Value hmu, hsig;
    const Value &mu  = dist_param(args, 1, 0.0, mr, hmu);
    const Value &sig = dist_param(args, 2, 1.0, mr, hsig);
    outs[0] = broadcast_dist3(args[0], mu, sig, mr, "norminv", norminvK);
}

void normrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("normrnd: requires (mu, sigma[, sz...])",
                     0, 0, "normrnd", "", "numkit:normrnd:nargin");
    const double mu = args[0].toScalar();
    const double sigma = args[1].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 2, rows, cols);
    outs[0] = normrnd(mu, sigma, rows, cols, ctx.engine->resource());
}

void normstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx.engine->resource(), "normstat",
                       [](double mu, double sigma) { return normstat(mu, sigma); });
}

} // namespace detail
} // namespace numkit::stats
