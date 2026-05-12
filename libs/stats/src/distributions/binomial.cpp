// libs/stats/src/distributions/binomial.cpp

#include <numkit/stats/distributions/binomial.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>

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

inline double bino_pmf(double k, double n, double p) {
    if (k < 0.0 || k > n || std::floor(k) != k) return 0.0;
    if (p == 0.0) return (k == 0.0) ? 1.0 : 0.0;
    if (p == 1.0) return (k == n)   ? 1.0 : 0.0;
    // log f = lgamma(n+1) - lgamma(k+1) - lgamma(n-k+1) + k log p + (n-k) log(1-p)
    const double lc = std::lgamma(n + 1.0) - std::lgamma(k + 1.0) - std::lgamma(n - k + 1.0);
    return std::exp(lc + k * std::log(p) + (n - k) * std::log1p(-p));
}

inline double bino_cdf_scalar(double k, double n, double p, std::pmr::memory_resource *mr) {
    if (k < 0.0) return 0.0;
    if (k >= n) return 1.0;
    if (p == 0.0) return 1.0;            // P(K = 0) = 1
    if (p == 1.0) return 0.0;            // P(K < n) = 0
    const double kf = std::floor(k);
    // F(k) = I_{1-p}(n - kf, kf + 1)
    Value xv = Value::scalar(1.0 - p, mr);
    Value av = Value::scalar(n - kf, mr);
    Value bv = Value::scalar(kf + 1.0, mr);
    Value r = ::numkit::builtin::betainc(xv, av, bv, mr);
    return r.toScalar();
}

} // anonymous

Value binopdf(const Value &k, double n, double p, std::pmr::memory_resource *mr)
{
    if (n < 0.0 || std::floor(n) != n || p < 0.0 || p > 1.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki){ return bino_pmf(ki, n, p); }, mr);
}

Value binocdf(const Value &k, double n, double p, std::pmr::memory_resource *mr)
{
    if (n < 0.0 || std::floor(n) != n || p < 0.0 || p > 1.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki){ return bino_cdf_scalar(ki, n, p, mr); }, mr);
}

Value binoinv(const Value &p_in, double n, double p, std::pmr::memory_resource *mr)
{
    if (n < 0.0 || std::floor(n) != n || p < 0.0 || p > 1.0)
        return elementwise(p_in, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p_in, [=](double pi) {
        if (!(pi >= 0.0 && pi <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 0.0;
        if (pi >= 1.0) return n;
        if (n == 0.0)  return 0.0;
        // Walk via pmf recurrence: pmf(j+1) / pmf(j) = (n-j)/(j+1) · p/(1-p).
        const double r = p / (1.0 - p);
        double pmf = std::pow(1.0 - p, n);  // pmf(0)
        double cdf = pmf;
        const double tol = std::max(1e-13, pi * 1e-13);
        if (cdf >= pi - tol) return 0.0;
        for (double j = 0.0; j < n; j += 1.0) {
            pmf *= (n - j) / (j + 1.0) * r;
            cdf += pmf;
            if (cdf >= pi - tol) return j + 1.0;
        }
        return n;
    }, mr);
}

Value binornd(double n, double p, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (n < 0.0 || std::floor(n) != n || p < 0.0 || p > 1.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t cnt = rows * cols;
    std::binomial_distribution<int> bd(static_cast<int>(n), p);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < cnt; ++i) od[i] = static_cast<double>(bd(gen));
    return out;
}

std::tuple<double, double> binostat(double n, double p)
{
    if (n < 0.0 || std::floor(n) != n || p < 0.0 || p > 1.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(n * p, n * p * (1.0 - p));
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void binopdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("binopdf: requires (k, n, p)", 0, 0, "binopdf", "", "m:binopdf:nargin");
    outs[0] = binopdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void binocdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 3)
        throw Error("binocdf: requires (k, n, p[, 'upper'])", 0, 0, "binocdf", "", "m:binocdf:nargin");
    Value v = binocdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void binoinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("binoinv: requires (p, n, prob)", 0, 0, "binoinv", "", "m:binoinv:nargin");
    outs[0] = binoinv(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void binornd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("binornd: requires (n, p[, sz...])", 0, 0, "binornd", "", "m:binornd:nargin");
    const double n = args[0].toScalar();
    const double p = args[1].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 2, rows, cols);
    outs[0] = binornd(n, p, rows, cols, ctx.engine->resource());
}

void binostat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx, "binostat",
                       [](double n, double p) { return binostat(n, p); });
}

} // namespace detail
} // namespace numkit::stats
