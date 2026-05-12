// libs/stats/src/distributions/gamma_dist.cpp
//
// Gamma distribution. Uses MATLAB convention: gampdf(x, a, b) interprets a
// as shape and b as scale (NOT rate). So f(x) = x^(a-1) exp(-x/b) /
// (b^a · Γ(a)). cdf composes gammainc on x/b; icdf uses gammaincinv;
// rnd uses std::gamma_distribution directly.

#include <numkit/stats/distributions/gamma_dist.hpp>

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

} // anonymous

Value gampdf(const Value &x, double a, double b, std::pmr::memory_resource *mr)
{
    // MATLAB: a<0 or b<=0 → NaN; a==0 → 0 (degenerate, all mass at 0).
    if (a < 0.0 || b <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    if (a == 0.0)
        return elementwise(x, [](double){ return 0.0; }, mr);
    // log f(x) = (a-1) log x - x/b - a log b - lgamma(a)
    const double log_b = std::log(b);
    const double lga   = std::lgamma(a);
    return elementwise(x, [=](double xi) {
        if (xi < 0.0) return 0.0;
        if (xi == 0.0) {
            if (a < 1.0) return std::numeric_limits<double>::infinity();
            if (a > 1.0) return 0.0;
            return std::exp(-lga - log_b); // a == 1 → 1/b
        }
        const double lp = (a - 1.0) * std::log(xi)
                        - xi / b
                        - a * log_b
                        - lga;
        return std::exp(lp);
    }, mr);
}

Value gamcdf(const Value &x, double a, double b, std::pmr::memory_resource *mr)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    Value xs = elementwise(x, [=](double xi) {
        return (xi <= 0.0) ? 0.0 : xi / b;
    }, mr);
    Value av = Value::scalar(a, mr);
    return ::numkit::builtin::gammainc(mr, xs, av);
}

Value gaminv(const Value &p, double a, double b, std::pmr::memory_resource *mr)
{
    // MATLAB: a<0 / b<=0 → NaN; a==0 → 0 for p∈[0,1] (degenerate quantile = 0).
    if (a < 0.0 || b <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    if (a == 0.0)
        return elementwise(p, [](double pi) {
            return (pi >= 0.0 && pi <= 1.0) ? 0.0
                                            : std::numeric_limits<double>::quiet_NaN();
        }, mr);
    Value av = Value::scalar(a, mr);
    Value q  = ::numkit::builtin::gammaincinv(mr, p, av);
    return elementwise(q, [=](double qi){ return b * qi; }, mr);
}

Value gamrnd(double a, double b, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (a <= 0.0 || b <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::gamma_distribution<double> gd(a, b);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) od[i] = gd(gen);
    return out;
}

std::tuple<double, double> gamstat(double a, double b)
{
    if (a <= 0.0 || b <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(a * b, a * b * b);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void gampdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("gampdf: requires (x, a, b)", 0, 0, "gampdf", "", "m:gampdf:nargin");
    outs[0] = gampdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void gamcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 3)
        throw Error("gamcdf: requires (x, a, b[, 'upper'])", 0, 0, "gamcdf", "", "m:gamcdf:nargin");
    Value v = gamcdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void gaminv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("gaminv: requires (p, a, b)", 0, 0, "gaminv", "", "m:gaminv:nargin");
    outs[0] = gaminv(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void gamrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("gamrnd: requires (a, b[, sz...])", 0, 0, "gamrnd", "", "m:gamrnd:nargin");
    const double a = args[0].toScalar();
    const double b = args[1].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 2, rows, cols);
    outs[0] = gamrnd(a, b, rows, cols, ctx.engine->resource());
}

void gamstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx, "gamstat",
                       [](double a, double b) { return gamstat(a, b); });
}

} // namespace detail
} // namespace numkit::stats
