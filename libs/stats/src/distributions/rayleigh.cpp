// libs/stats/src/distributions/rayleigh.cpp

#include <numkit/stats/distributions/rayleigh.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

Value raylpdf(std::pmr::memory_resource *mr, const Value &x, double b)
{
    if (b <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    const double inv_b2 = 1.0 / (b * b);
    return elementwise(mr, x, [=](double xi) {
        if (xi < 0.0) return 0.0;
        return xi * inv_b2 * std::exp(-0.5 * xi * xi * inv_b2);
    });
}

Value raylcdf(std::pmr::memory_resource *mr, const Value &x, double b)
{
    if (b <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    const double inv_b2 = 1.0 / (b * b);
    return elementwise(mr, x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        return -std::expm1(-0.5 * xi * xi * inv_b2);
    });
}

Value raylinv(std::pmr::memory_resource *mr, const Value &p, double b)
{
    if (b <= 0.0)
        return elementwise(mr, p, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, p, [=](double pi) {
        if (pi < 0.0 || pi > 1.0) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 0.0;
        if (pi >= 1.0) return std::numeric_limits<double>::infinity();
        return b * std::sqrt(-2.0 * std::log1p(-pi));
    });
}

Value raylrnd(std::pmr::memory_resource *mr, double b, size_t rows, size_t cols)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (b <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    // Inverse-cdf sampling: X = b·sqrt(-2 log U), U ~ U(0,1).
    std::uniform_real_distribution<double> ud(0.0, 1.0);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        double u;
        do { u = ud(gen); } while (u == 0.0);
        od[i] = b * std::sqrt(-2.0 * std::log(u));
    }
    return out;
}

std::tuple<double, double> raylstat(double b)
{
    if (b <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double mean = b * std::sqrt(0.5 * M_PI);
    const double var  = (2.0 - 0.5 * M_PI) * b * b;
    return std::make_tuple(mean, var);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void raylpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("raylpdf: requires (x, b)", 0, 0, "raylpdf", "", "m:raylpdf:nargin");
    outs[0] = raylpdf(ctx.engine->resource(), args[0], args[1].toScalar());
}

void raylcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("raylcdf: requires (x, b)", 0, 0, "raylcdf", "", "m:raylcdf:nargin");
    outs[0] = raylcdf(ctx.engine->resource(), args[0], args[1].toScalar());
}

void raylinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("raylinv: requires (p, b)", 0, 0, "raylinv", "", "m:raylinv:nargin");
    outs[0] = raylinv(ctx.engine->resource(), args[0], args[1].toScalar());
}

void raylrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("raylrnd: requires b[, m, n]", 0, 0, "raylrnd", "", "m:raylrnd:nargin");
    const double b = args[0].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 2 && !args[1].isEmpty()) rows = static_cast<size_t>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty()) cols = static_cast<size_t>(args[2].toScalar());
    else if (args.size() >= 2) cols = rows;
    outs[0] = raylrnd(ctx.engine->resource(), b, rows, cols);
}

void raylstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("raylstat: requires b", 0, 0, "raylstat", "", "m:raylstat:nargin");
    auto [m, v] = raylstat(args[0].toScalar());
    outs[0] = Value::scalar(m, ctx.engine->resource());
    if (nargout > 1) outs[1] = Value::scalar(v, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
