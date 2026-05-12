// libs/stats/src/distributions/geometric.cpp

#include <numkit/stats/distributions/geometric.hpp>

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

} // anonymous

Value geopdf(const Value &k, double p, std::pmr::memory_resource *mr)
{
    if (p <= 0.0 || p > 1.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki) {
        if (ki < 0.0 || std::floor(ki) != ki) return 0.0;
        if (p == 1.0) return ki == 0.0 ? 1.0 : 0.0;
        return std::pow(1.0 - p, ki) * p;
    }, mr);
}

Value geocdf(const Value &k, double p, std::pmr::memory_resource *mr)
{
    if (p <= 0.0 || p > 1.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki) {
        if (ki < 0.0) return 0.0;
        if (p == 1.0) return ki >= 0.0 ? 1.0 : 0.0;
        // F(k) = 1 - (1-p)^(⌊k⌋ + 1)
        return -std::expm1((std::floor(ki) + 1.0) * std::log1p(-p));
    }, mr);
}

Value geoinv(const Value &q, double p, std::pmr::memory_resource *mr)
{
    if (p <= 0.0 || p > 1.0)
        return elementwise(q, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(q, [=](double qi) {
        if (!(qi >= 0.0 && qi <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
        if (qi == 0.0) return 0.0;
        if (qi >= 1.0) return std::numeric_limits<double>::infinity();
        if (p == 1.0) return 0.0;
        // Smallest integer k such that 1 - (1-p)^(k+1) ≥ qi
        //   ⇔ (1-p)^(k+1) ≤ 1-qi
        //   ⇔ k+1 ≥ log(1-qi) / log(1-p)
        //   ⇔ k ≥ log(1-qi)/log(1-p) - 1
        const double v = std::log1p(-qi) / std::log1p(-p) - 1.0;
        double k = std::ceil(v);
        if (k < 0.0) k = 0.0;
        // One-ULP tolerance: if v - (k - 1) < tol, we already had k-1 satisfying.
        if (k > 0.0) {
            const double cdf_prev = -std::expm1(k * std::log1p(-p)); // F(k-1)
            const double tol = std::max(1e-13, qi * 1e-13);
            if (cdf_prev >= qi - tol) k -= 1.0;
        }
        return k;
    }, mr);
}

Value geornd(double p, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (p <= 0.0 || p > 1.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t cnt = rows * cols;
    std::geometric_distribution<int> gd(p);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < cnt; ++i) od[i] = static_cast<double>(gd(gen));
    return out;
}

std::tuple<double, double> geostat(double p)
{
    if (p <= 0.0 || p > 1.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double q = 1.0 - p;
    return std::make_tuple(q / p, q / (p * p));
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void geopdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("geopdf: requires (k, p)", 0, 0, "geopdf", "", "m:geopdf:nargin");
    outs[0] = geopdf(args[0], args[1].toScalar(), ctx.engine->resource());
}

void geocdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 2)
        throw Error("geocdf: requires (k, p[, 'upper'])", 0, 0, "geocdf", "", "m:geocdf:nargin");
    Value v = geocdf(args[0], args[1].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void geoinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("geoinv: requires (q, p)", 0, 0, "geoinv", "", "m:geoinv:nargin");
    outs[0] = geoinv(args[0], args[1].toScalar(), ctx.engine->resource());
}

void geornd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("geornd: requires p[, m, n]", 0, 0, "geornd", "", "m:geornd:nargin");
    const double p = args[0].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 2 && !args[1].isEmpty()) rows = static_cast<size_t>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty()) cols = static_cast<size_t>(args[2].toScalar());
    else if (args.size() >= 2) cols = rows;
    outs[0] = geornd(p, rows, cols, ctx.engine->resource());
}

void geostat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx, "geostat",
                       [](double p) { return geostat(p); });
}

} // namespace detail
} // namespace numkit::stats
