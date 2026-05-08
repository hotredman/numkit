// libs/stats/src/distributions/uniform.cpp

#include <numkit/stats/distributions/uniform.hpp>

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

Value unifpdf(std::pmr::memory_resource *mr, const Value &x, double a, double b)
{
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    // b <= a -> NaN (matches MATLAB; a==b is degenerate 0-width support).
    if (b <= a)
        return elementwise(mr, x, [NaN](double){ return NaN; });
    const double inv = 1.0 / (b - a);
    return elementwise(mr, x, [=](double xi) {
        if (std::isnan(xi)) return NaN;  // propagate NaN x — matches MATLAB
        return (xi >= a && xi <= b) ? inv : 0.0;
    });
}

Value unifcdf(std::pmr::memory_resource *mr, const Value &x, double a, double b)
{
    if (b <= a)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    const double inv = 1.0 / (b - a);
    return elementwise(mr, x, [=](double xi) {
        if (xi <= a) return 0.0;
        if (xi >= b) return 1.0;
        return (xi - a) * inv;
    });
}

Value unifinv(std::pmr::memory_resource *mr, const Value &p, double a, double b)
{
    if (b <= a)
        return elementwise(mr, p, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    const double w = b - a;
    return elementwise(mr, p, [=](double pi) {
        if (pi < 0.0 || pi > 1.0) return std::numeric_limits<double>::quiet_NaN();
        return a + pi * w;
    });
}

Value unifrnd(std::pmr::memory_resource *mr, double a, double b, size_t rows, size_t cols)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (b <= a || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::uniform_real_distribution<double> ud(a, b);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) od[i] = ud(gen);
    return out;
}

std::tuple<double, double> unifstat(double a, double b)
{
    if (b <= a) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double w = b - a;
    return std::make_tuple(0.5 * (a + b), (w * w) / 12.0);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

namespace {
// MATLAB defaults: a = 0, b = 1 when omitted.
inline double argA(Span<const Value> args, size_t i) {
    return (args.size() > i && !args[i].isEmpty()) ? args[i].toScalar() : 0.0;
}
inline double argB(Span<const Value> args, size_t i) {
    return (args.size() > i && !args[i].isEmpty()) ? args[i].toScalar() : 1.0;
}
}

void unifpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("unifpdf: requires (x[, a, b])", 0, 0, "unifpdf", "", "m:unifpdf:nargin");
    outs[0] = unifpdf(ctx.engine->resource(), args[0], argA(args, 1), argB(args, 2));
}

void unifcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> stripped = args.subspan(0, stripUpperFlag(args, upper));
    if (stripped.empty())
        throw Error("unifcdf: requires (x[, a, b][, 'upper'])", 0, 0, "unifcdf", "", "m:unifcdf:nargin");
    Value v = unifcdf(ctx.engine->resource(), stripped[0],
                      argA(stripped, 1), argB(stripped, 2));
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void unifinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("unifinv: requires (p[, a, b])", 0, 0, "unifinv", "", "m:unifinv:nargin");
    outs[0] = unifinv(ctx.engine->resource(), args[0], argA(args, 1), argB(args, 2));
}

void unifrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("unifrnd: requires (a, b[, m, n])", 0, 0, "unifrnd", "", "m:unifrnd:nargin");
    const double a = args[0].toScalar();
    const double b = args[1].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty()) cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3) cols = rows;
    outs[0] = unifrnd(ctx.engine->resource(), a, b, rows, cols);
}

void unifstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx, "unifstat",
                       [](double a, double b) { return unifstat(a, b); });
}

} // namespace detail
} // namespace numkit::stats
