// libs/stats/src/distributions/extreme_value.cpp

#include <numkit/stats/distributions/extreme_value.hpp>

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
constexpr double kPi2Over6   = 1.6449340668482264364724151666460252;  // π²/6

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

} // anonymous

Value evpdf(std::pmr::memory_resource *mr, const Value &x, double mu, double sigma)
{
    if (sigma <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    const double inv_s = 1.0 / sigma;
    return elementwise(mr, x, [=](double xi) {
        const double t = (xi - mu) * inv_s;
        return inv_s * std::exp(t - std::exp(t));
    });
}

Value evcdf(std::pmr::memory_resource *mr, const Value &x, double mu, double sigma)
{
    if (sigma <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, x, [=](double xi) {
        const double t = (xi - mu) / sigma;
        return -std::expm1(-std::exp(t));
    });
}

Value evinv(std::pmr::memory_resource *mr, const Value &p, double mu, double sigma)
{
    if (sigma <= 0.0)
        return elementwise(mr, p, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, p, [=](double pi) {
        if (!(pi >= 0.0 && pi <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return -std::numeric_limits<double>::infinity();
        if (pi >= 1.0) return  std::numeric_limits<double>::infinity();
        // x = mu + sigma · log(-log1p(-p))
        return mu + sigma * std::log(-std::log1p(-pi));
    });
}

Value evrnd(std::pmr::memory_resource *mr, double mu, double sigma,
            size_t rows, size_t cols)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (sigma <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::uniform_real_distribution<double> ud(0.0, 1.0);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        const double u = ud(gen);
        // Avoid log(0) for u==0; uniform_real never returns exactly 1, but
        // guard anyway for robustness.
        const double safe = (u > 0.0) ? u : std::numeric_limits<double>::min();
        od[i] = mu + sigma * std::log(-std::log1p(-safe));
    }
    return out;
}

std::tuple<double, double> evstat(double mu, double sigma)
{
    if (sigma <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(mu - sigma * kEulerGamma,
                           sigma * sigma * kPi2Over6);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void evpdf_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("evpdf: requires x[, mu, sigma]",
                    0, 0, "evpdf", "", "m:evpdf:nargin");
    const double mu    = (args.size() >= 2) ? args[1].toScalar() : 0.0;
    const double sigma = (args.size() >= 3) ? args[2].toScalar() : 1.0;
    outs[0] = evpdf(ctx.engine->resource(), args[0], mu, sigma);
}

void evcdf_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 1)
        throw Error("evcdf: requires x[, mu, sigma][, 'upper']",
                    0, 0, "evcdf", "", "m:evcdf:nargin");
    const double mu    = (n >= 2) ? args[1].toScalar() : 0.0;
    const double sigma = (n >= 3) ? args[2].toScalar() : 1.0;
    Value v = evcdf(ctx.engine->resource(), args[0], mu, sigma);
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void evinv_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("evinv: requires p[, mu, sigma]",
                    0, 0, "evinv", "", "m:evinv:nargin");
    const double mu    = (args.size() >= 2) ? args[1].toScalar() : 0.0;
    const double sigma = (args.size() >= 3) ? args[2].toScalar() : 1.0;
    outs[0] = evinv(ctx.engine->resource(), args[0], mu, sigma);
}

void evrnd_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("evrnd: requires (mu, sigma[, m, n])",
                    0, 0, "evrnd", "", "m:evrnd:nargin");
    const double mu    = args[0].toScalar();
    const double sigma = args[1].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty()) cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3) cols = rows;
    outs[0] = evrnd(ctx.engine->resource(), mu, sigma, rows, cols);
}

void evstat_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx, "evstat",
                       [](double mu, double sigma) { return evstat(mu, sigma); });
}

} // namespace detail
} // namespace numkit::stats
