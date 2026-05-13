// libs/stats/src/distributions/gev.cpp

#include <numkit/stats/distributions/gev.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

namespace numkit::stats {

namespace {

constexpr double kEulerGamma = 0.5772156649015328606065120900824024;
constexpr double kPi2Over6   = 1.6449340668482264364724151666460252;

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

// Sample inverse-cdf: x = mu + sigma · ((-log(u))^(-k) - 1)/k for k≠0,
// x = mu − sigma·log(−log(u)) for k=0.
inline double gev_inv_one(double u, double k, double sigma, double mu) {
    if (!(u >= 0.0 && u <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
    if (u == 0.0) return (k > 0) ? mu - sigma / k
                                 : -std::numeric_limits<double>::infinity();
    if (u == 1.0) return (k < 0) ? mu - sigma / k
                                 :  std::numeric_limits<double>::infinity();
    if (k == 0.0) return mu - sigma * std::log(-std::log(u));
    return mu + sigma * (std::pow(-std::log(u), -k) - 1.0) / k;
}

} // anonymous

Value gevpdf(const Value &x, double k, double sigma, double mu, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_s = 1.0 / sigma;
    return elementwise(x, [=](double xi) {
        const double z = (xi - mu) * inv_s;
        if (k == 0.0) return inv_s * std::exp(-z - std::exp(-z));
        const double t = 1.0 + k * z;
        if (t <= 0.0) return 0.0;
        const double tinvk = std::pow(t, -1.0 / k);
        return inv_s * std::pow(t, -1.0 / k - 1.0) * std::exp(-tinvk);
    }, mr);
}

Value gevcdf(const Value &x, double k, double sigma, double mu, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_s = 1.0 / sigma;
    return elementwise(x, [=](double xi) {
        const double z = (xi - mu) * inv_s;
        if (k == 0.0) return std::exp(-std::exp(-z));
        const double t = 1.0 + k * z;
        if (t <= 0.0) {
            // For k > 0 below the lower endpoint: F = 0.
            // For k < 0 above the upper endpoint: F = 1.
            return (k > 0) ? 0.0 : 1.0;
        }
        return std::exp(-std::pow(t, -1.0 / k));
    }, mr);
}

Value gevinv(const Value &p, double k, double sigma, double mu, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p, [=](double pi) {
        return gev_inv_one(pi, k, sigma, mu);
    }, mr);
}

Value gevrnd(double k, double sigma, double mu, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (sigma <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        // genRes53 -- MATLAB-canonical 53-bit uniform; bypasses
        // std::uniform_real_distribution to preserve bit-parity.
        double u = gen.genRes53();
        if (u <= 0.0) u = std::numeric_limits<double>::min();
        od[i] = gev_inv_one(u, k, sigma, mu);
    }
    return out;
}

std::tuple<double, double> gevstat(double k, double sigma, double mu)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (sigma <= 0.0) return std::make_tuple(nan, nan);
    if (k == 0.0)
        return std::make_tuple(mu + sigma * kEulerGamma, sigma * sigma * kPi2Over6);
    if (!(k < 1.0))
        return std::make_tuple(std::numeric_limits<double>::infinity(), nan);
    const double g1 = std::tgamma(1.0 - k);
    const double mean = mu + sigma * (g1 - 1.0) / k;
    if (!(k < 0.5))
        return std::make_tuple(mean, std::numeric_limits<double>::infinity());
    const double g2 = std::tgamma(1.0 - 2.0 * k);
    const double var = (sigma * sigma / (k * k)) * (g2 - g1 * g1);
    return std::make_tuple(mean, var);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void gevpdf_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("gevpdf: requires (x, k, sigma, mu)",
                    0, 0, "gevpdf", "", "m:gevpdf:nargin");
    outs[0] = gevpdf(args[0], args[1].toScalar(), args[2].toScalar(), args[3].toScalar(), ctx.engine->resource());
}

void gevcdf_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 4)
        throw Error("gevcdf: requires (x, k, sigma, mu[, 'upper'])",
                    0, 0, "gevcdf", "", "m:gevcdf:nargin");
    Value v = gevcdf(args[0], args[1].toScalar(), args[2].toScalar(), args[3].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void gevinv_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("gevinv: requires (p, k, sigma, mu)",
                    0, 0, "gevinv", "", "m:gevinv:nargin");
    outs[0] = gevinv(args[0], args[1].toScalar(), args[2].toScalar(), args[3].toScalar(), ctx.engine->resource());
}

void gevrnd_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("gevrnd: requires (k, sigma, mu[, m, n])",
                    0, 0, "gevrnd", "", "m:gevrnd:nargin");
    const double k     = args[0].toScalar();
    const double sigma = args[1].toScalar();
    const double mu    = args[2].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 4 && !args[3].isEmpty()) rows = static_cast<size_t>(args[3].toScalar());
    if (args.size() >= 5 && !args[4].isEmpty()) cols = static_cast<size_t>(args[4].toScalar());
    else if (args.size() >= 4) cols = rows;
    outs[0] = gevrnd(k, sigma, mu, rows, cols, ctx.engine->resource());
}

void gevstat_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_3arg(args, nargout, outs, ctx, "gevstat",
                       [](double k, double sigma, double mu) {
                           return gevstat(k, sigma, mu);
                       });
    return;
}

} // namespace detail
} // namespace numkit::stats
