// libs/stats/src/distributions/rayleigh.cpp

#include <numkit/stats/distributions/rayleigh.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "dist_helpers.hpp"

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

// Scalar kernels for the parameter-broadcast path (vector b). Own their
// per-element domain (b<=0 → NaN). Mirror the public fns bit-identically.
inline double raylpdfK(double x, double b)
{
    if (b <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    const double inv_b2 = 1.0 / (b * b);
    if (x < 0.0) return 0.0;
    return x * inv_b2 * std::exp(-0.5 * x * x * inv_b2);
}

inline double raylcdfK(double x, double b)
{
    if (b <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    const double inv_b2 = 1.0 / (b * b);
    if (x <= 0.0) return 0.0;
    return -std::expm1(-0.5 * x * x * inv_b2);
}

inline double raylinvK(double p, double b)
{
    if (b <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    if (p < 0.0 || p > 1.0) return std::numeric_limits<double>::quiet_NaN();
    if (p == 0.0) return 0.0;
    if (p >= 1.0) return std::numeric_limits<double>::infinity();
    return b * std::sqrt(-2.0 * std::log1p(-p));
}

} // anonymous

Value raylpdf(const Value &x, double b, std::pmr::memory_resource *mr)
{
    if (b <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_b2 = 1.0 / (b * b);
    return elementwise(x, [=](double xi) {
        if (xi < 0.0) return 0.0;
        return xi * inv_b2 * std::exp(-0.5 * xi * xi * inv_b2);
    }, mr);
}

Value raylcdf(const Value &x, double b, std::pmr::memory_resource *mr)
{
    if (b <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_b2 = 1.0 / (b * b);
    return elementwise(x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        return -std::expm1(-0.5 * xi * xi * inv_b2);
    }, mr);
}

Value raylinv(const Value &p, double b, std::pmr::memory_resource *mr)
{
    if (b <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p, [=](double pi) {
        if (pi < 0.0 || pi > 1.0) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 0.0;
        if (pi >= 1.0) return std::numeric_limits<double>::infinity();
        return b * std::sqrt(-2.0 * std::log1p(-pi));
    }, mr);
}

Value raylrnd(double b, size_t rows, size_t cols, std::pmr::memory_resource *mr)
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
        throw Error("raylpdf: requires (x, b)", 0, 0, "raylpdf", "", "numkit:raylpdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &b = args[1];
    if (b.isScalar())
        outs[0] = raylpdf(args[0], b.toScalar(), mr);
    else
        outs[0] = broadcast_dist2(args[0], b, mr, "raylpdf", raylpdfK);
}

void raylcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    if (a.size() < 2)
        throw Error("raylcdf: requires (x, b[, 'upper'])", 0, 0, "raylcdf", "", "numkit:raylcdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &b = a[1];
    Value v = b.isScalar() ? raylcdf(a[0], b.toScalar(), mr)
                           : broadcast_dist2(a[0], b, mr, "raylcdf", raylcdfK);
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void raylinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("raylinv: requires (p, b)", 0, 0, "raylinv", "", "numkit:raylinv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &b = args[1];
    if (b.isScalar())
        outs[0] = raylinv(args[0], b.toScalar(), mr);
    else
        outs[0] = broadcast_dist2(args[0], b, mr, "raylinv", raylinvK);
}

void raylrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("raylrnd: requires b[, sz...]", 0, 0, "raylrnd", "", "numkit:raylrnd:nargin");
    const double b = args[0].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 1, rows, cols);
    outs[0] = raylrnd(b, rows, cols, ctx.engine->resource());
}

void raylstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx.engine->resource(), "raylstat",
                       [](double b) { return raylstat(b); });
}

} // namespace detail
} // namespace numkit::stats
