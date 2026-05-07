// libs/stats/src/distributions/beta.cpp
//
// Beta distribution. pdf via log-form using lbeta = lgamma(a)+lgamma(b)-lgamma(a+b);
// cdf is betainc(x, a, b); icdf is betaincinv(p, a, b); rnd via two Gamma draws.

#include <numkit/stats/distributions/beta.hpp>

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

Value betapdf(std::pmr::memory_resource *mr, const Value &x, double a, double b)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    // log f(x) = (a-1) log x + (b-1) log(1-x) - lbeta(a, b)
    const double lbeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    return elementwise(mr, x, [=](double xi) {
        if (xi < 0.0 || xi > 1.0) return 0.0;
        if (xi == 0.0) return (a == 1.0) ? std::exp(-lbeta) * (b == 1.0 ? 1.0 : std::pow(1.0, b - 1.0))
                                          : (a > 1.0 ? 0.0 : std::numeric_limits<double>::infinity());
        if (xi == 1.0) return (b == 1.0) ? std::exp(-lbeta) * (a == 1.0 ? 1.0 : std::pow(1.0, a - 1.0))
                                          : (b > 1.0 ? 0.0 : std::numeric_limits<double>::infinity());
        const double lp = (a - 1.0) * std::log(xi)
                        + (b - 1.0) * std::log1p(-xi)
                        - lbeta;
        return std::exp(lp);
    });
}

Value betacdf(std::pmr::memory_resource *mr, const Value &x, double a, double b)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    Value av = Value::scalar(a, mr);
    Value bv = Value::scalar(b, mr);
    // Clamp into [0, 1] so betainc behaves on out-of-domain input.
    Value xc = elementwise(mr, x, [](double xi) {
        if (xi <= 0.0) return 0.0;
        if (xi >= 1.0) return 1.0;
        return xi;
    });
    return ::numkit::builtin::betainc(mr, xc, av, bv);
}

Value betainv(std::pmr::memory_resource *mr, const Value &p, double a, double b)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(mr, p, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    Value av = Value::scalar(a, mr);
    Value bv = Value::scalar(b, mr);
    return ::numkit::builtin::betaincinv(mr, p, av, bv);
}

Value betarnd(std::pmr::memory_resource *mr, double a, double b, size_t rows, size_t cols)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (a <= 0.0 || b <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::gamma_distribution<double> ga(a, 1.0);
    std::gamma_distribution<double> gb(b, 1.0);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        const double u = ga(gen);
        const double v = gb(gen);
        const double s = u + v;
        od[i] = (s > 0.0) ? u / s : 0.5;
    }
    return out;
}

std::tuple<double, double> betastat(double a, double b)
{
    if (a <= 0.0 || b <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double s = a + b;
    const double mean = a / s;
    const double var = (a * b) / (s * s * (s + 1.0));
    return std::make_tuple(mean, var);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void betapdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("betapdf: requires (x, a, b)", 0, 0, "betapdf", "", "m:betapdf:nargin");
    outs[0] = betapdf(ctx.engine->resource(), args[0], args[1].toScalar(), args[2].toScalar());
}

void betacdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 3)
        throw Error("betacdf: requires (x, a, b[, 'upper'])", 0, 0, "betacdf", "", "m:betacdf:nargin");
    Value v = betacdf(ctx.engine->resource(), args[0], args[1].toScalar(), args[2].toScalar());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void betainv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("betainv: requires (p, a, b)", 0, 0, "betainv", "", "m:betainv:nargin");
    outs[0] = betainv(ctx.engine->resource(), args[0], args[1].toScalar(), args[2].toScalar());
}

void betarnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("betarnd: requires (a, b[, m, n])", 0, 0, "betarnd", "", "m:betarnd:nargin");
    const double a = args[0].toScalar();
    const double b = args[1].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty()) cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3) cols = rows;
    outs[0] = betarnd(ctx.engine->resource(), a, b, rows, cols);
}

void betastat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx, "betastat",
                       [](double a, double b) { return betastat(a, b); });
}

} // namespace detail
} // namespace numkit::stats
