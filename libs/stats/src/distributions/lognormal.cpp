// libs/stats/src/distributions/lognormal.cpp

#include <numkit/stats/distributions/lognormal.hpp>

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

constexpr double kSqrt2 = 1.41421356237309504880;
constexpr double kSqrt2Pi = 2.50662827463100050241;

// Φ(z) via complementary error fn (avoids cancellation in tail).
inline double phi(double z) { return 0.5 * std::erfc(-z / kSqrt2); }

// Φ⁻¹(p) — Beasley-Springer-Moro / Acklam-style approximation; refined by
// one Newton step against erfc. Same construction as norminv in normal.cpp.
inline double phiInv(double p)
{
    if (!(p > 0.0) || !(p < 1.0)) {
        if (p == 0.0) return -std::numeric_limits<double>::infinity();
        if (p == 1.0) return  std::numeric_limits<double>::infinity();
        return std::numeric_limits<double>::quiet_NaN();
    }
    // Acklam coefficients
    static const double a[] = { -3.969683028665376e+01,  2.209460984245205e+02,
                                 -2.759285104469687e+02,  1.383577518672690e+02,
                                 -3.066479806614716e+01,  2.506628277459239e+00 };
    static const double b[] = { -5.447609879822406e+01,  1.615858368580409e+02,
                                 -1.556989798598866e+02,  6.680131188771972e+01,
                                 -1.328068155288572e+01 };
    static const double c[] = { -7.784894002430293e-03, -3.223964580411365e-01,
                                 -2.400758277161838e+00, -2.549732539343734e+00,
                                  4.374664141464968e+00,  2.938163982698783e+00 };
    static const double d[] = {  7.784695709041462e-03,  3.224671290700398e-01,
                                  2.445134137142996e+00,  3.754408661907416e+00 };
    const double pl = 0.02425, ph = 1.0 - pl;
    double q, r, x;
    if (p < pl) {
        q = std::sqrt(-2.0 * std::log(p));
        x = (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) /
            ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1.0);
    } else if (p <= ph) {
        q = p - 0.5;
        r = q*q;
        x = (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5]) * q /
            (((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1.0);
    } else {
        q = std::sqrt(-2.0 * std::log(1.0 - p));
        x = -(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) /
             ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1.0);
    }
    // One Newton refinement: x ← x - (Φ(x) - p) / φ(x)
    const double e = phi(x) - p;
    const double u = e * kSqrt2Pi * std::exp(0.5 * x * x);
    return x - u;
}

} // anonymous

Value lognpdf(std::pmr::memory_resource *mr, const Value &x, double mu, double sigma)
{
    if (sigma <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    const double inv_sig = 1.0 / sigma;
    const double inv_sqrt2pi = 1.0 / kSqrt2Pi;
    return elementwise(mr, x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        const double z = (std::log(xi) - mu) * inv_sig;
        return inv_sqrt2pi * inv_sig * std::exp(-0.5 * z * z) / xi;
    });
}

Value logncdf(std::pmr::memory_resource *mr, const Value &x, double mu, double sigma)
{
    if (sigma <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    const double inv_sig = 1.0 / sigma;
    return elementwise(mr, x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        return phi((std::log(xi) - mu) * inv_sig);
    });
}

Value logninv(std::pmr::memory_resource *mr, const Value &p, double mu, double sigma)
{
    if (sigma <= 0.0)
        return elementwise(mr, p, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, p, [=](double pi) {
        if (std::isnan(pi) || pi < 0.0 || pi > 1.0)
            return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 0.0;
        if (pi == 1.0) return std::numeric_limits<double>::infinity();
        return std::exp(mu + sigma * phiInv(pi));
    });
}

Value lognrnd(std::pmr::memory_resource *mr, double mu, double sigma, size_t rows, size_t cols)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (sigma <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::lognormal_distribution<double> ld(mu, sigma);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) od[i] = ld(gen);
    return out;
}

std::tuple<double, double> lognstat(double mu, double sigma)
{
    if (sigma <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double s2 = sigma * sigma;
    const double mean = std::exp(mu + 0.5 * s2);
    const double var = std::expm1(s2) * std::exp(2.0 * mu + s2);
    return std::make_tuple(mean, var);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

namespace {
inline double argMu(Span<const Value> args, size_t i) {
    return (args.size() > i && !args[i].isEmpty()) ? args[i].toScalar() : 0.0;
}
inline double argSigma(Span<const Value> args, size_t i) {
    return (args.size() > i && !args[i].isEmpty()) ? args[i].toScalar() : 1.0;
}
}

void lognpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("lognpdf: requires (x[, mu, sigma])", 0, 0, "lognpdf", "", "m:lognpdf:nargin");
    outs[0] = lognpdf(ctx.engine->resource(), args[0], argMu(args, 1), argSigma(args, 2));
}

void logncdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> stripped = args.subspan(0, stripUpperFlag(args, upper));
    if (stripped.empty())
        throw Error("logncdf: requires (x[, mu, sigma][, 'upper'])", 0, 0, "logncdf", "", "m:logncdf:nargin");
    Value v = logncdf(ctx.engine->resource(), stripped[0],
                      argMu(stripped, 1), argSigma(stripped, 2));
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void logninv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("logninv: requires (p[, mu, sigma])", 0, 0, "logninv", "", "m:logninv:nargin");
    outs[0] = logninv(ctx.engine->resource(), args[0], argMu(args, 1), argSigma(args, 2));
}

void lognrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    const double mu    = argMu(args, 0);
    const double sigma = argSigma(args, 1);
    size_t rows, cols;
    parse_rng_size(args, 2, rows, cols);
    outs[0] = lognrnd(ctx.engine->resource(), mu, sigma, rows, cols);
}

void lognstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx, "lognstat",
                       [](double mu, double sigma) { return lognstat(mu, sigma); });
}

} // namespace detail
} // namespace numkit::stats
