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

// Scalar kernels for the parameter-broadcast path (vector N). Own their
// per-element domain (N<1 or noninteger N → NaN). Mirror the public fns.
inline double unidpdfK(double k, double N) {
    if (N < 1.0 || std::floor(N) != N) return std::numeric_limits<double>::quiet_NaN();
    if (k < 1.0 || k > N || std::floor(k) != k) return 0.0;
    return 1.0 / N;
}

inline double unidcdfK(double k, double N) {
    if (N < 1.0 || std::floor(N) != N) return std::numeric_limits<double>::quiet_NaN();
    if (k < 1.0) return 0.0;
    if (k >= N) return 1.0;
    return std::floor(k) / N;
}

inline double unidinvK(double p, double N) {
    if (!(N >= 1.0) || std::floor(N) != N) return std::numeric_limits<double>::quiet_NaN();
    if (std::isnan(p) || p <= 0.0 || p > 1.0) return std::numeric_limits<double>::quiet_NaN();
    const double r = std::ceil(p * N);
    return r < 1.0 ? 1.0 : (r > N ? N : r);
}

} // anonymous

Value unidpdf(const Value &k, double N, std::pmr::memory_resource *mr)
{
    if (N < 1.0 || std::floor(N) != N)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv = 1.0 / N;
    return elementwise(k, [=](double ki) {
        if (ki < 1.0 || ki > N || std::floor(ki) != ki) return 0.0;
        return inv;
    }, mr);
}

Value unidcdf(const Value &k, double N, std::pmr::memory_resource *mr)
{
    if (N < 1.0 || std::floor(N) != N)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv = 1.0 / N;
    return elementwise(k, [=](double ki) {
        if (ki < 1.0) return 0.0;
        if (ki >= N) return 1.0;
        return std::floor(ki) * inv;
    }, mr);
}

Value unidinv(const Value &p, double N, std::pmr::memory_resource *mr)
{
    if (!(N >= 1.0) || std::floor(N) != N)  // also catches NaN N
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p, [=](double pi) {
        // MATLAB convention: p outside (0, 1] -> NaN. p=0 has no
        // integer pre-image in the support, so it is also NaN.
        if (std::isnan(pi) || pi <= 0.0 || pi > 1.0)
            return std::numeric_limits<double>::quiet_NaN();
        const double r = std::ceil(pi * N);
        return r < 1.0 ? 1.0 : (r > N ? N : r);
    }, mr);
}

Value unidrnd(double N, size_t rows, size_t cols, std::pmr::memory_resource *mr)
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
        throw Error("unidpdf: requires (k, N)", 0, 0, "unidpdf", "", "numkit:unidpdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &N = args[1];
    if (N.isScalar())
        outs[0] = unidpdf(args[0], N.toScalar(), mr);
    else
        outs[0] = broadcast_dist2(args[0], N, mr, "unidpdf", unidpdfK);
}

void unidcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    if (a.size() < 2)
        throw Error("unidcdf: requires (k, N[, 'upper'])", 0, 0, "unidcdf", "", "numkit:unidcdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &N = a[1];
    Value v = N.isScalar() ? unidcdf(a[0], N.toScalar(), mr)
                           : broadcast_dist2(a[0], N, mr, "unidcdf", unidcdfK);
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void unidinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("unidinv: requires (p, N)", 0, 0, "unidinv", "", "numkit:unidinv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &N = args[1];
    if (N.isScalar())
        outs[0] = unidinv(args[0], N.toScalar(), mr);
    else
        outs[0] = broadcast_dist2(args[0], N, mr, "unidinv", unidinvK);
}

void unidrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("unidrnd: requires N[, sz...]", 0, 0, "unidrnd", "", "numkit:unidrnd:nargin");
    const double N = args[0].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 1, rows, cols);
    outs[0] = unidrnd(N, rows, cols, ctx.engine->resource());
}

void unidstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx, "unidstat",
                       [](double N) { return unidstat(N); });
}

} // namespace detail
} // namespace numkit::stats
