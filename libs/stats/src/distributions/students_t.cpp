// libs/stats/src/distributions/students_t.cpp
//
// Student's t-distribution. Composes betainc / betaincinv for cdf /
// icdf; rnd uses Z / sqrt(X/ν) with Z ~ N(0,1), X ~ χ²(ν).

#include <numkit/stats/distributions/students_t.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <cstring>
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

// Read a Value's first element as scalar (for inputs we know are
// scalar Values produced by the helpers). Avoids extra copies.
double scalarOf(const Value &v) { return v.toScalar(); }

} // anonymous

Value tpdf(const Value &x, double nu, std::pmr::memory_resource *mr)
{
    if (!(nu > 0.0))  // nu <= 0 or NaN
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    // Gaussian limit: as nu -> Inf, t-PDF -> N(0, 1) PDF.
    if (std::isinf(nu)) {
        const double inv = 1.0 / std::sqrt(2.0 * M_PI);
        return elementwise(x, [inv](double xi){
            return inv * std::exp(-0.5 * xi * xi);
        }, mr);
    }
    // log f(x) = lgamma((ν+1)/2) - lgamma(ν/2) - 0.5*log(ν π)
    //          - ((ν+1)/2) * log(1 + x²/ν)
    const double log_norm = std::lgamma(0.5 * (nu + 1.0))
                          - std::lgamma(0.5 * nu)
                          - 0.5 * std::log(nu * M_PI);
    const double exponent = -0.5 * (nu + 1.0);
    return elementwise(x, [=](double xi) {
        return std::exp(log_norm + exponent * std::log1p(xi * xi / nu));
    }, mr);
}

Value tcdf(const Value &x, double nu, std::pmr::memory_resource *mr)
{
    if (nu <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    // Build z[i] = ν / (ν + x[i]²); pass through betainc with (ν/2, 1/2).
    // The cdf is then 1 - ½·I_z(ν/2, ½) for x ≥ 0 and ½·I_z otherwise.
    const size_t n = x.numel();
    Value z = elementwise(x, [=](double xi){ return nu / (nu + xi * xi); }, mr);
    Value a = Value::scalar(0.5 * nu, mr);
    Value b = Value::scalar(0.5, mr);
    Value Iz = ::numkit::builtin::betainc(mr, z, a, b);

    // Walk x and Iz in parallel, picking branch by sign of x.
    auto out = Value::matrix(x.dims().rows(), x.dims().cols(), ValueType::DOUBLE, mr);
    if (x.dims().is3D())
        out = Value::matrix3d(x.dims().rows(), x.dims().cols(), x.dims().pages(), ValueType::DOUBLE, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i) {
        const double xi = x.elemAsDouble(i);
        const double Ii = Iz.elemAsDouble(i);
        od[i] = (xi >= 0.0) ? 1.0 - 0.5 * Ii : 0.5 * Ii;
    }
    return out;
}

Value tinv(const Value &p, double nu, std::pmr::memory_resource *mr)
{
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double PINF = std::numeric_limits<double>::infinity();
    const double NINF = -PINF;
    if (!(nu > 0.0))  // nu <= 0 or NaN
        return elementwise(p, [NaN](double){ return NaN; }, mr);
    // Gaussian limit: as nu -> Inf, t-distribution -> N(0, 1), so tinv(p, Inf) = norminv(p).
    if (std::isinf(nu)) {
        return elementwise(p, [NaN, PINF, NINF](double pi){
            if (std::isnan(pi) || pi < 0.0 || pi > 1.0) return NaN;
            if (pi == 0.0) return NINF;
            if (pi == 1.0) return PINF;
            // norminv(p) = sqrt(2)·erfinv(2p - 1); local Winitzki + 2 Newton steps.
            const double y = 2.0 * pi - 1.0;
            if (y >= 1.0) return PINF;
            if (y <= -1.0) return NINF;
            const double a = 0.147;
            const double ln1m = std::log(1.0 - y * y);
            const double term = 2.0 / (M_PI * a) + 0.5 * ln1m;
            double xv = std::copysign(std::sqrt(std::sqrt(term * term - ln1m / a) - term), y);
            for (int it = 0; it < 2; ++it) {
                const double e  = std::erf(xv) - y;
                const double de = 2.0 * std::exp(-xv * xv) / std::sqrt(M_PI);
                xv -= e / de;
            }
            return std::sqrt(2.0) * xv;
        }, mr);
    }
    // Use betaincinv: from p ↔ z via I_z(ν/2, 1/2) = q where:
    //   p ≥ 0.5: q = 2(1-p), x = +sqrt(ν · (1/z - 1))
    //   p < 0.5: q = 2p,     x = -sqrt(ν · (1/z - 1))
    const size_t n = p.numel();
    auto qv = elementwise(p, [](double pi){
        // Out-of-range / NaN handled later; clamp to a valid betaincinv arg here.
        if (std::isnan(pi) || pi < 0.0 || pi > 1.0) return 0.5;
        return (pi >= 0.5) ? 2.0 * (1.0 - pi) : 2.0 * pi;
    }, mr);
    Value a = Value::scalar(0.5 * nu, mr);
    Value b = Value::scalar(0.5, mr);
    Value zv = ::numkit::builtin::betaincinv(mr, qv, a, b);

    auto out = Value::matrix(p.dims().rows(), p.dims().cols(), ValueType::DOUBLE, mr);
    if (p.dims().is3D())
        out = Value::matrix3d(p.dims().rows(), p.dims().cols(), p.dims().pages(), ValueType::DOUBLE, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i) {
        const double pi = p.elemAsDouble(i);
        if (std::isnan(pi) || pi < 0.0 || pi > 1.0) { od[i] = NaN; continue; }
        if (pi == 0.0) { od[i] = NINF; continue; }
        if (pi == 1.0) { od[i] = PINF; continue; }
        const double zi = zv.elemAsDouble(i);
        if (zi <= 0.0) { od[i] = (pi >= 0.5) ? PINF : NINF; continue; }
        const double mag = std::sqrt(nu * (1.0 / zi - 1.0));
        od[i] = (pi >= 0.5) ? mag : -mag;
    }
    return out;
}

Value trnd(double nu, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (nu <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::normal_distribution<double> nd(0.0, 1.0);
    std::gamma_distribution<double>  gd(0.5 * nu, 2.0);  // χ²(ν) = Gamma(ν/2, 2)
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        const double Z = nd(gen);
        const double X = gd(gen);
        od[i] = Z / std::sqrt(X / nu);
    }
    return out;
}

std::tuple<double, double> tstat(double nu)
{
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    if (nu <= 0.0) return std::make_tuple(NaN, NaN);
    // mean defined only for nu > 1; variance only for nu > 2.
    const double m = (nu > 1.0) ? 0.0 : NaN;
    const double v = (nu > 2.0) ? (nu / (nu - 2.0)) : NaN;
    return std::make_tuple(m, v);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void tpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tpdf: requires (x, nu)", 0, 0, "tpdf", "", "m:tpdf:nargin");
    outs[0] = tpdf(args[0], args[1].toScalar(), ctx.engine->resource());
}

void tcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 2)
        throw Error("tcdf: requires (x, nu[, 'upper'])", 0, 0, "tcdf", "", "m:tcdf:nargin");
    Value v = tcdf(args[0], args[1].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void tinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tinv: requires (p, nu)", 0, 0, "tinv", "", "m:tinv:nargin");
    outs[0] = tinv(args[0], args[1].toScalar(), ctx.engine->resource());
}

void trnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("trnd: requires nu[, sz...]", 0, 0, "trnd", "", "m:trnd:nargin");
    const double nu = args[0].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 1, rows, cols);
    outs[0] = trnd(nu, rows, cols, ctx.engine->resource());
}

void tstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx, "tstat",
                       [](double nu) { return tstat(nu); });
}

} // namespace detail
} // namespace numkit::stats
