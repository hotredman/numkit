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

// Scalar pdf kernel for the parameter-broadcast path (vector a/b). Owns its
// per-element domain: a<=0 or b<=0 → NaN. Mirrors public betapdf exactly,
// including the x∈{0,1} boundary cases.
inline double betapdfK(double x, double a, double b)
{
    if (a <= 0.0 || b <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    const double lbeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    if (x < 0.0 || x > 1.0) return 0.0;
    if (x == 0.0)
        return (a == 1.0) ? std::exp(-lbeta) * (b == 1.0 ? 1.0 : std::pow(1.0, b - 1.0))
                          : (a > 1.0 ? 0.0 : std::numeric_limits<double>::infinity());
    if (x == 1.0)
        return (b == 1.0) ? std::exp(-lbeta) * (a == 1.0 ? 1.0 : std::pow(1.0, a - 1.0))
                          : (b > 1.0 ? 0.0 : std::numeric_limits<double>::infinity());
    return std::exp((a - 1.0) * std::log(x) + (b - 1.0) * std::log1p(-x) - lbeta);
}

} // anonymous

Value betapdf(const Value &x, double a, double b, std::pmr::memory_resource *mr)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    // log f(x) = (a-1) log x + (b-1) log(1-x) - lbeta(a, b)
    const double lbeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    return elementwise(x, [=](double xi) {
        if (xi < 0.0 || xi > 1.0) return 0.0;
        if (xi == 0.0) return (a == 1.0) ? std::exp(-lbeta) * (b == 1.0 ? 1.0 : std::pow(1.0, b - 1.0))
                                          : (a > 1.0 ? 0.0 : std::numeric_limits<double>::infinity());
        if (xi == 1.0) return (b == 1.0) ? std::exp(-lbeta) * (a == 1.0 ? 1.0 : std::pow(1.0, a - 1.0))
                                          : (b > 1.0 ? 0.0 : std::numeric_limits<double>::infinity());
        const double lp = (a - 1.0) * std::log(xi)
                        + (b - 1.0) * std::log1p(-xi)
                        - lbeta;
        return std::exp(lp);
    }, mr);
}

Value betacdf(const Value &x, double a, double b, std::pmr::memory_resource *mr)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    Value av = Value::scalar(a, mr);
    Value bv = Value::scalar(b, mr);
    // Clamp into [0, 1] so betainc behaves on out-of-domain input.
    Value xc = elementwise(x, [](double xi) {
        if (xi <= 0.0) return 0.0;
        if (xi >= 1.0) return 1.0;
        return xi;
    }, mr);
    return ::numkit::builtin::betainc(xc, av, bv, mr);
}

Value betainv(const Value &p, double a, double b, std::pmr::memory_resource *mr)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    Value av = Value::scalar(a, mr);
    Value bv = Value::scalar(b, mr);
    return ::numkit::builtin::betaincinv(p, av, bv, mr);
}

Value betarnd(double a, double b, size_t rows, size_t cols, std::pmr::memory_resource *mr)
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
        throw Error("betapdf: requires (x, a, b)", 0, 0, "betapdf", "", "numkit:betapdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &a = args[1];
    const Value &b = args[2];
    if (a.isScalar() && b.isScalar())
        outs[0] = betapdf(args[0], a.toScalar(), b.toScalar(), mr);
    else
        outs[0] = broadcast_dist3(args[0], a, b, mr, "betapdf", betapdfK);
}

void betacdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a0 = args.subspan(0, stripUpperFlag(args, upper));
    if (a0.size() < 3)
        throw Error("betacdf: requires (x, a, b[, 'upper'])", 0, 0, "betacdf", "", "numkit:betacdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &a = a0[1];
    const Value &b = a0[2];
    Value v;
    if (a.isScalar() && b.isScalar()) {
        v = betacdf(a0[0], a.toScalar(), b.toScalar(), mr);
    } else {
        // F(x; a, b) = betainc(clamp01(x), a, b); betainc broadcasts (xc, a, b)
        // and betaincScalar gives NaN where a<=0 or b<=0. betainc does NOT
        // validate sizes (would OOB), so guard empties + size clash first.
        const size_t nx = a0[0].numel(), na = a.numel(), nb = b.numel();
        if (nx == 0 || na == 0 || nb == 0) {
            v = dist_empty_like(nx == 0 ? a0[0] : (na == 0 ? a : b), mr);
        } else {
            dist_match_numel({nx, na, nb}, "betacdf");
            Value xc = elementwise(a0[0], [](double xi) {
                if (xi <= 0.0) return 0.0;
                if (xi >= 1.0) return 1.0;
                return xi;
            }, mr);
            v = ::numkit::builtin::betainc(xc, a, b, mr);
        }
    }
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void betainv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("betainv: requires (p, a, b)", 0, 0, "betainv", "", "numkit:betainv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &p = args[0];
    const Value &a = args[1];
    const Value &b = args[2];
    if (a.isScalar() && b.isScalar()) {
        outs[0] = betainv(p, a.toScalar(), b.toScalar(), mr);   // unchanged fast path
        return;
    }
    // betaincinv broadcasts (p, a, b) and gives NaN for a<=0||b<=0 (no other
    // degenerate). It does NOT validate sizes (would OOB), so guard first.
    const size_t np = p.numel(), na = a.numel(), nb = b.numel();
    if (np == 0 || na == 0 || nb == 0) {
        outs[0] = dist_empty_like(np == 0 ? p : (na == 0 ? a : b), mr);
        return;
    }
    dist_match_numel({np, na, nb}, "betainv");
    outs[0] = ::numkit::builtin::betaincinv(p, a, b, mr);
}

void betarnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("betarnd: requires (a, b[, sz...])", 0, 0, "betarnd", "", "numkit:betarnd:nargin");
    const double a = args[0].toScalar();
    const double b = args[1].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 2, rows, cols);
    outs[0] = betarnd(a, b, rows, cols, ctx.engine->resource());
}

void betastat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx, "betastat",
                       [](double a, double b) { return betastat(a, b); });
}

} // namespace detail
} // namespace numkit::stats
