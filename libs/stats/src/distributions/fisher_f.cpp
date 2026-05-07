// libs/stats/src/distributions/fisher_f.cpp
//
// F-distribution. Composes betainc / betaincinv on
// (v1·x/(v1·x + v2), v1/2, v2/2) for cdf / icdf; rnd from two
// independent χ²-distributed samples.

#include <numkit/stats/distributions/fisher_f.hpp>

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

Value fpdf(std::pmr::memory_resource *mr, const Value &x, double v1, double v2)
{
    if (v1 <= 0.0 || v2 <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    // log f(x; v1, v2) = (v1/2) log(v1) + (v2/2) log(v2)
    //                  + (v1/2 - 1) log x
    //                  - ((v1+v2)/2) log(v2 + v1 x)
    //                  - lbeta(v1/2, v2/2)
    // where lbeta(a, b) = lgamma(a) + lgamma(b) - lgamma(a+b).
    const double a = 0.5 * v1;
    const double b = 0.5 * v2;
    const double lbeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    const double log_v1 = std::log(v1);
    const double log_v2 = std::log(v2);
    return elementwise(mr, x, [=](double xi) {
        if (xi < 0.0) return 0.0;
        if (xi == 0.0) {
            // Density at 0 has three regimes (limit of x^(v1/2 - 1) as x → 0+):
            //   v1 < 2  →  +Inf
            //   v1 == 2 →  finite, value computed without the (a-1)·log x term
            //   v1 > 2  →  0
            if (v1 <  2.0) return std::numeric_limits<double>::infinity();
            if (v1 == 2.0) {
                const double lp0 = a * log_v1 + b * log_v2
                                 - (a + b) * std::log(v2)
                                 - lbeta;
                return std::exp(lp0);
            }
            return 0.0;
        }
        const double lp = a * log_v1 + b * log_v2
                        + (a - 1.0) * std::log(xi)
                        - (a + b) * std::log(v2 + v1 * xi)
                        - lbeta;
        return std::exp(lp);
    });
}

Value fcdf(std::pmr::memory_resource *mr, const Value &x, double v1, double v2)
{
    if (v1 <= 0.0 || v2 <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    Value z = elementwise(mr, x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        return (v1 * xi) / (v1 * xi + v2);
    });
    Value a = Value::scalar(0.5 * v1, mr);
    Value b = Value::scalar(0.5 * v2, mr);
    return ::numkit::builtin::betainc(mr, z, a, b);
}

Value finv(std::pmr::memory_resource *mr, const Value &p, double v1, double v2)
{
    if (v1 <= 0.0 || v2 <= 0.0)
        return elementwise(mr, p, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    Value a = Value::scalar(0.5 * v1, mr);
    Value b = Value::scalar(0.5 * v2, mr);
    Value z = ::numkit::builtin::betaincinv(mr, p, a, b);
    // x = (v2 / v1) · z / (1 - z)
    return elementwise(mr, z, [=](double zi){
        if (zi <= 0.0) return 0.0;
        if (zi >= 1.0) return std::numeric_limits<double>::infinity();
        return (v2 / v1) * zi / (1.0 - zi);
    });
}

Value frnd(std::pmr::memory_resource *mr, double v1, double v2, size_t rows, size_t cols)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (v1 <= 0.0 || v2 <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::gamma_distribution<double> g1(0.5 * v1, 2.0); // χ²(v1)
    std::gamma_distribution<double> g2(0.5 * v2, 2.0); // χ²(v2)
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        const double x1 = g1(gen);
        const double x2 = g2(gen);
        od[i] = (x1 / v1) / (x2 / v2);
    }
    return out;
}

std::tuple<double, double> fstat(double v1, double v2)
{
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    // Invalid params ⇒ NaN/NaN (matches MATLAB R2025b).
    if (v1 <= 0.0 || v2 <= 0.0) return std::make_tuple(NaN, NaN);
    const double mean = (v2 > 2.0) ? (v2 / (v2 - 2.0)) : NaN;
    double var = NaN;
    if (v2 > 4.0) {
        const double num = 2.0 * v2 * v2 * (v1 + v2 - 2.0);
        const double den = v1 * (v2 - 2.0) * (v2 - 2.0) * (v2 - 4.0);
        var = num / den;
    }
    return std::make_tuple(mean, var);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void fpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("fpdf: requires (x, v1, v2)", 0, 0, "fpdf", "", "m:fpdf:nargin");
    outs[0] = fpdf(ctx.engine->resource(), args[0], args[1].toScalar(), args[2].toScalar());
}

void fcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 3)
        throw Error("fcdf: requires (x, v1, v2[, 'upper'])", 0, 0, "fcdf", "", "m:fcdf:nargin");
    Value v = fcdf(ctx.engine->resource(), args[0], args[1].toScalar(), args[2].toScalar());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void finv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("finv: requires (p, v1, v2)", 0, 0, "finv", "", "m:finv:nargin");
    outs[0] = finv(ctx.engine->resource(), args[0], args[1].toScalar(), args[2].toScalar());
}

void frnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("frnd: requires (v1, v2[, m, n])", 0, 0, "frnd", "", "m:frnd:nargin");
    const double v1 = args[0].toScalar();
    const double v2 = args[1].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty()) cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3) cols = rows;
    outs[0] = frnd(ctx.engine->resource(), v1, v2, rows, cols);
}

void fstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx, "fstat",
                       [](double v1, double v2) { return fstat(v1, v2); });
}

} // namespace detail
} // namespace numkit::stats
