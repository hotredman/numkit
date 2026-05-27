// libs/stats/src/distributions/nakagami.cpp

#include <numkit/stats/distributions/nakagami.hpp>

#include <numkit/builtin/math/special/special.hpp>   // gammainc, gammaincinv
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

inline double gammainc_scalar(double xx, double a, std::pmr::memory_resource *mr)
{
    Value xv = Value::scalar(xx, mr);
    Value av = Value::scalar(a,  mr);
    return ::numkit::builtin::gammainc(xv, av, mr).toScalar();
}

inline double gammaincinv_scalar(double p, double a, std::pmr::memory_resource *mr)
{
    Value pv = Value::scalar(p, mr);
    Value av = Value::scalar(a, mr);
    return ::numkit::builtin::gammaincinv(pv, av, mr).toScalar();
}

} // anonymous

Value nakapdf(const Value &x, double mu, double omega, std::pmr::memory_resource *mr)
{
    if (mu <= 0.0 || omega <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    // Pre-compute constants
    const double C = 2.0 * std::pow(mu, mu) / (std::tgamma(mu) * std::pow(omega, mu));
    const double k = mu / omega;
    return elementwise(x, [=](double xi) {
        if (xi < 0.0) return 0.0;
        if (xi == 0.0) return (mu < 0.5) ? std::numeric_limits<double>::infinity()
                                         : (mu == 0.5 ? C : 0.0);
        return C * std::pow(xi, 2.0 * mu - 1.0) * std::exp(-k * xi * xi);
    }, mr);
}

Value nakacdf(const Value &x, double mu, double omega, std::pmr::memory_resource *mr)
{
    if (mu <= 0.0 || omega <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double k = mu / omega;
    return elementwise(x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        return gammainc_scalar(k * xi * xi, mu, mr);
    }, mr);
}

Value nakainv(const Value &p, double mu, double omega, std::pmr::memory_resource *mr)
{
    if (mu <= 0.0 || omega <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double s = std::sqrt(omega / mu);
    return elementwise(p, [=](double pi) {
        if (!(pi >= 0.0 && pi <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 0.0;
        if (pi >= 1.0) return std::numeric_limits<double>::infinity();
        const double q = gammaincinv_scalar(pi, mu, mr);
        return s * std::sqrt(q);
    }, mr);
}

Value nakarnd(double mu, double omega, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (mu <= 0.0 || omega <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    // X² ~ Gamma(shape=mu, scale=omega/mu) ⇒ X = √Gamma.
    std::gamma_distribution<double> gd(mu, omega / mu);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) od[i] = std::sqrt(gd(gen));
    return out;
}

std::tuple<double, double> nakastat(double mu, double omega)
{
    if (mu <= 0.0 || omega <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double r = std::tgamma(mu + 0.5) / std::tgamma(mu);
    const double mean = std::sqrt(omega / mu) * r;
    const double var  = omega * (1.0 - r * r / mu);
    return std::make_tuple(mean, var);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void nakapdf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nakapdf: requires (x, mu, omega)",
                    0, 0, "nakapdf", "", "numkit:nakapdf:nargin");
    outs[0] = nakapdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void nakacdf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 3)
        throw Error("nakacdf: requires (x, mu, omega[, 'upper'])",
                    0, 0, "nakacdf", "", "numkit:nakacdf:nargin");
    Value v = nakacdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void nakainv_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nakainv: requires (p, mu, omega)",
                    0, 0, "nakainv", "", "numkit:nakainv:nargin");
    outs[0] = nakainv(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void nakarnd_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("nakarnd: requires (mu, omega[, m, n])",
                    0, 0, "nakarnd", "", "numkit:nakarnd:nargin");
    const double mu    = args[0].toScalar();
    const double omega = args[1].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty()) cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3) cols = rows;
    outs[0] = nakarnd(mu, omega, rows, cols, ctx.engine->resource());
}

void nakastat_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx, "nakastat",
                       [](double mu, double omega) { return nakastat(mu, omega); });
}

} // namespace detail
} // namespace numkit::stats
