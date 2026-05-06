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

Value exppdf(std::pmr::memory_resource *mr, const Value &x, double mu)
{
    if (mu <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    const double inv_mu = 1.0 / mu;
    return elementwise(mr, x, [=](double xi) {
        if (xi < 0.0) return 0.0;
        return inv_mu * std::exp(-xi * inv_mu);
    });
}

Value expcdf(std::pmr::memory_resource *mr, const Value &x, double mu)
{
    if (mu <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    const double inv_mu = 1.0 / mu;
    return elementwise(mr, x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        return -std::expm1(-xi * inv_mu);
    });
}

Value expinv(std::pmr::memory_resource *mr, const Value &p, double mu)
{
    if (mu <= 0.0)
        return elementwise(mr, p, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, p, [=](double pi) {
        if (pi < 0.0 || pi > 1.0) return std::numeric_limits<double>::quiet_NaN();
        if (pi >= 1.0) return std::numeric_limits<double>::infinity();
        return -mu * std::log1p(-pi);
    });
}

Value exprnd(std::pmr::memory_resource *mr, double mu, size_t rows, size_t cols)
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
    if (args.size() < 2)
        throw Error("exppdf: requires (x, mu)", 0, 0, "exppdf", "", "m:exppdf:nargin");
    outs[0] = exppdf(ctx.engine->resource(), args[0], args[1].toScalar());
}

void expcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 2)
        throw Error("expcdf: requires (x, mu[, 'upper'])", 0, 0, "expcdf", "", "m:expcdf:nargin");
    Value v = expcdf(ctx.engine->resource(), args[0], args[1].toScalar());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void expinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("expinv: requires (p, mu)", 0, 0, "expinv", "", "m:expinv:nargin");
    outs[0] = expinv(ctx.engine->resource(), args[0], args[1].toScalar());
}

void exprnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("exprnd: requires mu[, m, n]", 0, 0, "exprnd", "", "m:exprnd:nargin");
    const double mu = args[0].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 2 && !args[1].isEmpty()) rows = static_cast<size_t>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty()) cols = static_cast<size_t>(args[2].toScalar());
    else if (args.size() >= 2) cols = rows;
    outs[0] = exprnd(ctx.engine->resource(), mu, rows, cols);
}

void expstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("expstat: requires mu", 0, 0, "expstat", "", "m:expstat:nargin");
    auto [m, v] = expstat(args[0].toScalar());
    outs[0] = Value::scalar(m, ctx.engine->resource());
    if (nargout > 1) outs[1] = Value::scalar(v, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
