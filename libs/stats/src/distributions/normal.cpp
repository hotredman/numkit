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

} // anonymous

Value normpdf(std::pmr::memory_resource *mr, const Value &x, double mu, double sigma)
{
    if (sigma <= 0.0) {
        return elementwise(mr, x, [](double){
            return std::numeric_limits<double>::quiet_NaN();
        });
    }
    const double inv = 1.0 / (sigma * kSqrt2Pi);
    const double inv2s2 = 1.0 / (2.0 * sigma * sigma);
    return elementwise(mr, x, [=](double xi) {
        const double d = xi - mu;
        return inv * std::exp(-d * d * inv2s2);
    });
}

Value normcdf(std::pmr::memory_resource *mr, const Value &x, double mu, double sigma)
{
    if (sigma <= 0.0)
        return elementwise(mr, x, [](double){
            return std::numeric_limits<double>::quiet_NaN();
        });
    const double inv = 1.0 / (sigma * kSqrt2);
    return elementwise(mr, x, [=](double xi) {
        return 0.5 * (1.0 + std::erf((xi - mu) * inv));
    });
}

Value norminv(std::pmr::memory_resource *mr, const Value &p, double mu, double sigma)
{
    if (sigma <= 0.0)
        return elementwise(mr, p, [](double){
            return std::numeric_limits<double>::quiet_NaN();
        });
    return elementwise(mr, p, [=](double pi) {
        if (pi <= 0.0) return -std::numeric_limits<double>::infinity();
        if (pi >= 1.0) return  std::numeric_limits<double>::infinity();
        return mu + sigma * kSqrt2 * erfinvScalar(2.0 * pi - 1.0);
    });
}

Value normrnd(std::pmr::memory_resource *mr, double mu, double sigma,
              size_t rows, size_t cols)
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
    return std::make_tuple(mu, sigma * sigma);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

namespace {

// Helper: parse optional (mu, sigma) from args[1..]. Returns defaults
// (0, 1) when not supplied.
std::pair<double, double> parseMuSigma(Span<const Value> args, size_t start)
{
    double mu = 0.0, sigma = 1.0;
    if (args.size() > start && !args[start].isEmpty())
        mu = args[start].toScalar();
    if (args.size() > start + 1 && !args[start + 1].isEmpty())
        sigma = args[start + 1].toScalar();
    return {mu, sigma};
}

} // anonymous

void normpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("normpdf: requires at least 1 argument",
                     0, 0, "normpdf", "", "m:normpdf:nargin");
    auto [mu, sigma] = parseMuSigma(args, 1);
    outs[0] = normpdf(ctx.engine->resource(), args[0], mu, sigma);
}

void normcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("normcdf: requires at least 1 argument",
                     0, 0, "normcdf", "", "m:normcdf:nargin");
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    auto [mu, sigma] = parseMuSigma(args.subspan(0, n), 1);
    Value v = normcdf(ctx.engine->resource(), args[0], mu, sigma);
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void norminv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("norminv: requires at least 1 argument",
                     0, 0, "norminv", "", "m:norminv:nargin");
    auto [mu, sigma] = parseMuSigma(args, 1);
    outs[0] = norminv(ctx.engine->resource(), args[0], mu, sigma);
}

void normrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("normrnd: requires (mu, sigma[, m, n])",
                     0, 0, "normrnd", "", "m:normrnd:nargin");
    const double mu = args[0].toScalar();
    const double sigma = args[1].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty())
        rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty())
        cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3)
        cols = rows; // MATLAB: normrnd(mu, sigma, n) → n×n
    outs[0] = normrnd(ctx.engine->resource(), mu, sigma, rows, cols);
}

void normstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("normstat: requires (mu, sigma)",
                     0, 0, "normstat", "", "m:normstat:nargin");
    auto [mu, sigma] = parseMuSigma(args, 0);
    auto [m, v] = normstat(mu, sigma);
    outs[0] = Value::scalar(m, ctx.engine->resource());
    if (nargout > 1) outs[1] = Value::scalar(v, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
