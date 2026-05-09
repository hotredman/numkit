// libs/stats/src/distributions/gp.cpp

#include <numkit/stats/distributions/gp.hpp>

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

// Sample inverse-cdf: x = theta + sigma · ((1-u)^(-k) - 1)/k for k≠0,
// x = theta − sigma·log1p(−u) for k=0.
inline double gp_inv_one(double u, double k, double sigma, double theta) {
    if (!(u >= 0.0 && u <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
    if (u == 0.0) return theta;
    if (u == 1.0) return (k < 0) ? theta - sigma / k
                                 :  std::numeric_limits<double>::infinity();
    if (k == 0.0) return theta - sigma * std::log1p(-u);
    return theta + sigma * (std::pow(1.0 - u, -k) - 1.0) / k;
}

} // anonymous

Value gppdf(std::pmr::memory_resource *mr, const Value &x,
            double k, double sigma, double theta)
{
    if (sigma <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    const double inv_s = 1.0 / sigma;
    return elementwise(mr, x, [=](double xi) {
        const double z = (xi - theta) * inv_s;
        if (z < 0.0) return 0.0;
        if (k == 0.0) return inv_s * std::exp(-z);
        const double t = 1.0 + k * z;
        if (t <= 0.0) return 0.0;
        return inv_s * std::pow(t, -1.0 / k - 1.0);
    });
}

Value gpcdf(std::pmr::memory_resource *mr, const Value &x,
            double k, double sigma, double theta)
{
    if (sigma <= 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    const double inv_s = 1.0 / sigma;
    return elementwise(mr, x, [=](double xi) {
        const double z = (xi - theta) * inv_s;
        if (z <= 0.0) return 0.0;
        if (k == 0.0) return -std::expm1(-z);
        const double t = 1.0 + k * z;
        if (t <= 0.0) return (k > 0) ? 0.0 : 1.0;
        return 1.0 - std::pow(t, -1.0 / k);
    });
}

Value gpinv(std::pmr::memory_resource *mr, const Value &p,
            double k, double sigma, double theta)
{
    if (sigma <= 0.0)
        return elementwise(mr, p, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, p, [=](double pi) {
        return gp_inv_one(pi, k, sigma, theta);
    });
}

Value gprnd(std::pmr::memory_resource *mr, double k, double sigma, double theta,
            size_t rows, size_t cols)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (sigma <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        // genRes53 -- MATLAB-canonical 53-bit uniform.
        // gprnd uses u DIRECTLY (not 1-u) per MATLAB convention:
        //   x = theta + sigma * (u^(-k) - 1) / k       for k != 0
        //   x = theta - sigma * log(u)                  for k == 0
        // (gpinv(p) uses 1-p which is the standard ICDF; sampling
        // from rand() with the swapped-u form gives the same
        // distribution but matches MATLAB's specific bit sequence.)
        double u = gen.genRes53();
        if (u <= 0.0) u = std::numeric_limits<double>::min();
        if (k == 0.0) {
            od[i] = theta - sigma * std::log(u);
        } else {
            od[i] = theta + sigma * (std::pow(u, -k) - 1.0) / k;
        }
    }
    return out;
}

std::tuple<double, double> gpstat(double k, double sigma, double theta)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (sigma <= 0.0) return std::make_tuple(nan, nan);
    if (!(k < 1.0))
        return std::make_tuple(std::numeric_limits<double>::infinity(), nan);
    const double mean = theta + sigma / (1.0 - k);
    if (!(k < 0.5))
        return std::make_tuple(mean, std::numeric_limits<double>::infinity());
    const double var = sigma * sigma / ((1.0 - k) * (1.0 - k) * (1.0 - 2.0 * k));
    return std::make_tuple(mean, var);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void gppdf_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("gppdf: requires (x, k, sigma, theta)",
                    0, 0, "gppdf", "", "m:gppdf:nargin");
    outs[0] = gppdf(ctx.engine->resource(), args[0],
                    args[1].toScalar(), args[2].toScalar(), args[3].toScalar());
}

void gpcdf_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 4)
        throw Error("gpcdf: requires (x, k, sigma, theta[, 'upper'])",
                    0, 0, "gpcdf", "", "m:gpcdf:nargin");
    Value v = gpcdf(ctx.engine->resource(), args[0],
                    args[1].toScalar(), args[2].toScalar(), args[3].toScalar());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void gpinv_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("gpinv: requires (p, k, sigma, theta)",
                    0, 0, "gpinv", "", "m:gpinv:nargin");
    outs[0] = gpinv(ctx.engine->resource(), args[0],
                    args[1].toScalar(), args[2].toScalar(), args[3].toScalar());
}

void gprnd_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("gprnd: requires (k, sigma, theta[, m, n])",
                    0, 0, "gprnd", "", "m:gprnd:nargin");
    const double k     = args[0].toScalar();
    const double sigma = args[1].toScalar();
    const double theta = args[2].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 4 && !args[3].isEmpty()) rows = static_cast<size_t>(args[3].toScalar());
    if (args.size() >= 5 && !args[4].isEmpty()) cols = static_cast<size_t>(args[4].toScalar());
    else if (args.size() >= 4) cols = rows;
    outs[0] = gprnd(ctx.engine->resource(), k, sigma, theta, rows, cols);
}

void gpstat_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_3arg(args, nargout, outs, ctx, "gpstat",
                       [](double k, double sigma, double theta) {
                           return gpstat(k, sigma, theta);
                       });
}

} // namespace detail
} // namespace numkit::stats
