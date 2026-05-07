// libs/stats/src/distributions/weibull.cpp

#include <numkit/stats/distributions/weibull.hpp>

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

Value wblpdf(std::pmr::memory_resource *mr, const Value &x, double a, double b)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, x, [=](double xi) {
        if (xi < 0.0) return 0.0;
        if (xi == 0.0) {
            if (b < 1.0) return std::numeric_limits<double>::infinity();
            if (b > 1.0) return 0.0;
            return 1.0 / a; // b == 1 (exponential)
        }
        const double r = xi / a;
        return (b / a) * std::pow(r, b - 1.0) * std::exp(-std::pow(r, b));
    });
}

Value wblcdf(std::pmr::memory_resource *mr, const Value &x, double a, double b)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        return -std::expm1(-std::pow(xi / a, b));
    });
}

Value wblinv(std::pmr::memory_resource *mr, const Value &p, double a, double b)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(mr, p, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, p, [=](double pi) {
        if (pi < 0.0 || pi > 1.0) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 0.0;
        if (pi >= 1.0) return std::numeric_limits<double>::infinity();
        return a * std::pow(-std::log1p(-pi), 1.0 / b);
    });
}

Value wblrnd(std::pmr::memory_resource *mr, double a, double b, size_t rows, size_t cols)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (a <= 0.0 || b <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    // std::weibull_distribution(shape=a, scale=b) — note ORDER FLIP relative
    // to MATLAB (a=scale, b=shape).
    std::weibull_distribution<double> wd(b, a);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) od[i] = wd(gen);
    return out;
}

std::tuple<double, double> wblstat(double a, double b)
{
    if (a <= 0.0 || b <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double g1 = std::tgamma(1.0 + 1.0 / b);
    const double g2 = std::tgamma(1.0 + 2.0 / b);
    const double mean = a * g1;
    const double var  = a * a * (g2 - g1 * g1);
    return std::make_tuple(mean, var);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

namespace {
inline double argA(Span<const Value> args, size_t i) {
    return (args.size() > i && !args[i].isEmpty()) ? args[i].toScalar() : 1.0;
}
inline double argB(Span<const Value> args, size_t i) {
    return (args.size() > i && !args[i].isEmpty()) ? args[i].toScalar() : 1.0;
}
}

void wblpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("wblpdf: requires (x[, a, b])", 0, 0, "wblpdf", "", "m:wblpdf:nargin");
    outs[0] = wblpdf(ctx.engine->resource(), args[0], argA(args, 1), argB(args, 2));
}

void wblcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> stripped = args.subspan(0, stripUpperFlag(args, upper));
    if (stripped.empty())
        throw Error("wblcdf: requires (x[, a, b][, 'upper'])", 0, 0, "wblcdf", "", "m:wblcdf:nargin");
    Value v = wblcdf(ctx.engine->resource(), stripped[0],
                     argA(stripped, 1), argB(stripped, 2));
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void wblinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("wblinv: requires (p[, a, b])", 0, 0, "wblinv", "", "m:wblinv:nargin");
    outs[0] = wblinv(ctx.engine->resource(), args[0], argA(args, 1), argB(args, 2));
}

void wblrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    const double a = argA(args, 0);
    const double b = argB(args, 1);
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty()) cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3) cols = rows;
    outs[0] = wblrnd(ctx.engine->resource(), a, b, rows, cols);
}

void wblstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx, "wblstat",
                       [](double a, double b) { return wblstat(a, b); });
}

} // namespace detail
} // namespace numkit::stats
