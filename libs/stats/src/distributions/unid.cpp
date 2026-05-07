// libs/stats/src/distributions/unid.cpp

#include <numkit/stats/distributions/unid.hpp>

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

Value unidpdf(std::pmr::memory_resource *mr, const Value &k, double N)
{
    if (N < 1.0 || std::floor(N) != N)
        return elementwise(mr, k, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    const double inv = 1.0 / N;
    return elementwise(mr, k, [=](double ki) {
        if (ki < 1.0 || ki > N || std::floor(ki) != ki) return 0.0;
        return inv;
    });
}

Value unidcdf(std::pmr::memory_resource *mr, const Value &k, double N)
{
    if (N < 1.0 || std::floor(N) != N)
        return elementwise(mr, k, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    const double inv = 1.0 / N;
    return elementwise(mr, k, [=](double ki) {
        if (ki < 1.0) return 0.0;
        if (ki >= N) return 1.0;
        return std::floor(ki) * inv;
    });
}

Value unidinv(std::pmr::memory_resource *mr, const Value &p, double N)
{
    if (N < 1.0 || std::floor(N) != N)
        return elementwise(mr, p, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, p, [=](double pi) {
        if (!(pi >= 0.0 && pi <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 1.0;
        if (pi >= 1.0) return N;
        const double r = std::ceil(pi * N);
        return r < 1.0 ? 1.0 : r;
    });
}

Value unidrnd(std::pmr::memory_resource *mr, double N, size_t rows, size_t cols)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (N < 1.0 || std::floor(N) != N || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t cnt = rows * cols;
    std::uniform_int_distribution<long long> ud(1, static_cast<long long>(N));
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < cnt; ++i) od[i] = static_cast<double>(ud(gen));
    return out;
}

std::tuple<double, double> unidstat(double N)
{
    if (N < 1.0 || std::floor(N) != N) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(0.5 * (N + 1.0), (N * N - 1.0) / 12.0);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void unidpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("unidpdf: requires (k, N)", 0, 0, "unidpdf", "", "m:unidpdf:nargin");
    outs[0] = unidpdf(ctx.engine->resource(), args[0], args[1].toScalar());
}

void unidcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 2)
        throw Error("unidcdf: requires (k, N[, 'upper'])", 0, 0, "unidcdf", "", "m:unidcdf:nargin");
    Value v = unidcdf(ctx.engine->resource(), args[0], args[1].toScalar());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void unidinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("unidinv: requires (p, N)", 0, 0, "unidinv", "", "m:unidinv:nargin");
    outs[0] = unidinv(ctx.engine->resource(), args[0], args[1].toScalar());
}

void unidrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("unidrnd: requires N[, m, n]", 0, 0, "unidrnd", "", "m:unidrnd:nargin");
    const double N = args[0].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 2 && !args[1].isEmpty()) rows = static_cast<size_t>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty()) cols = static_cast<size_t>(args[2].toScalar());
    else if (args.size() >= 2) cols = rows;
    outs[0] = unidrnd(ctx.engine->resource(), N, rows, cols);
}

void unidstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx, "unidstat",
                       [](double N) { return unidstat(N); });
}

} // namespace detail
} // namespace numkit::stats
