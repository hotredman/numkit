// libs/stats/src/distributions/exponential.cpp

#include <numkit/stats/distributions/exponential.hpp>

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

Value exppdf(const Value &x, double mu, std::pmr::memory_resource *mr)
{
    if (mu <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_mu = 1.0 / mu;
    return elementwise(x, [=](double xi) {
        if (xi < 0.0) return 0.0;
        return inv_mu * std::exp(-xi * inv_mu);
    }, mr);
}

Value expcdf(const Value &x, double mu, std::pmr::memory_resource *mr)
{
    if (mu <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_mu = 1.0 / mu;
    return elementwise(x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        return -std::expm1(-xi * inv_mu);
    }, mr);
}

Value expinv(const Value &p, double mu, std::pmr::memory_resource *mr)
{
    if (mu <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p, [=](double pi) {
        if (pi < 0.0 || pi > 1.0) return std::numeric_limits<double>::quiet_NaN();
        if (pi >= 1.0) return std::numeric_limits<double>::infinity();
        return -mu * std::log1p(-pi);
    }, mr);
}

Value exprnd(double mu, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (mu <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::exponential_distribution<double> ed(1.0 / mu);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) od[i] = ed(gen);
    return out;
}

std::tuple<double, double> expstat(double mu)
{
    if (mu <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(mu, mu * mu);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void exppdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("exppdf: requires (x[, mu])", 0, 0, "exppdf", "", "numkit:exppdf:nargin");
    // MATLAB default: exppdf(x) ≡ exppdf(x, 1).
    const double mu = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    outs[0] = exppdf(args[0], mu, ctx.engine->resource());
}

void expcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 2)
        throw Error("expcdf: requires (x, mu[, 'upper'])", 0, 0, "expcdf", "", "numkit:expcdf:nargin");
    Value v = expcdf(args[0], args[1].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void expinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("expinv: requires (p[, mu])", 0, 0, "expinv", "", "numkit:expinv:nargin");
    // MATLAB default: expinv(p) ≡ expinv(p, 1).
    const double mu = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    outs[0] = expinv(args[0], mu, ctx.engine->resource());
}

void exprnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("exprnd: requires mu[, sz...]", 0, 0, "exprnd", "", "numkit:exprnd:nargin");
    const double mu = args[0].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 1, rows, cols);
    outs[0] = exprnd(mu, rows, cols, ctx.engine->resource());
}

void expstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx, "expstat",
                       [](double mu) { return expstat(mu); });
}

} // namespace detail
} // namespace numkit::stats
