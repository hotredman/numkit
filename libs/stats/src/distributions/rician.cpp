// libs/stats/src/distributions/rician.cpp

#include <numkit/stats/distributions/rician.hpp>

#include <numkit/builtin/math/special/special.hpp>   // besseli
#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/comm/channel/channel.hpp>           // marcumq

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

inline double besseli_scalar(std::pmr::memory_resource *mr, double nu, double xx)
{
    Value nv = Value::scalar(nu, mr);
    Value xv = Value::scalar(xx, mr);
    return ::numkit::builtin::besseli(mr, nv, xv).toScalar();
}

inline double marcumq_scalar(std::pmr::memory_resource *mr, double a, double b)
{
    Value av = Value::scalar(a, mr);
    Value bv = Value::scalar(b, mr);
    return ::numkit::comm::marcumq(mr, av, bv, 1).toScalar();
}

} // anonymous

Value ricepdf(std::pmr::memory_resource *mr, const Value &x, double s, double sigma)
{
    if (sigma <= 0.0 || s < 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    const double s2 = s * s;
    const double sg2 = sigma * sigma;
    return elementwise(mr, x, [&](double xi) {
        if (xi < 0.0) return 0.0;
        if (xi == 0.0) {
            // Limit: at x=0 the PDF is 0 unless s=0 (Rayleigh case).
            return (s == 0.0) ? 0.0 : 0.0;
        }
        const double arg = xi * s / sg2;
        const double i0  = besseli_scalar(mr, 0.0, arg);
        return (xi / sg2) * std::exp(-(xi * xi + s2) / (2.0 * sg2)) * i0;
    });
}

Value ricecdf(std::pmr::memory_resource *mr, const Value &x, double s, double sigma)
{
    if (sigma <= 0.0 || s < 0.0)
        return elementwise(mr, x, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, x, [&](double xi) {
        if (xi <= 0.0) return 0.0;
        return 1.0 - marcumq_scalar(mr, s / sigma, xi / sigma);
    });
}

Value riceinv(std::pmr::memory_resource *mr, const Value &p, double s, double sigma)
{
    if (sigma <= 0.0 || s < 0.0)
        return elementwise(mr, p, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, p, [&](double pi) {
        if (!(pi >= 0.0 && pi <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 0.0;
        if (pi >= 1.0) return std::numeric_limits<double>::infinity();
        // Bracket then bisect: F(0) = 0, F(s + 8σ) ≈ 1 (Gaussian-tail bound).
        double lo = 0.0;
        double hi = s + 8.0 * sigma;
        // Expand `hi` if needed.
        for (int iter = 0; iter < 50; ++iter) {
            const double Fhi = 1.0 - marcumq_scalar(mr, s / sigma, hi / sigma);
            if (Fhi >= pi) break;
            hi *= 2.0;
        }
        // Bisection to ~1e-12 relative tolerance.
        for (int iter = 0; iter < 80; ++iter) {
            const double mid = 0.5 * (lo + hi);
            if (mid == lo || mid == hi) return mid;
            const double F = 1.0 - marcumq_scalar(mr, s / sigma, mid / sigma);
            if (F < pi) lo = mid; else hi = mid;
        }
        return 0.5 * (lo + hi);
    });
}

Value ricernd(std::pmr::memory_resource *mr, double s, double sigma,
              size_t rows, size_t cols)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (sigma <= 0.0 || s < 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::normal_distribution<double> nd1(s, sigma);
    std::normal_distribution<double> nd2(0.0, sigma);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        const double a = nd1(gen);
        const double b = nd2(gen);
        od[i] = std::sqrt(a * a + b * b);
    }
    return out;
}

std::tuple<double, double> ricestat(std::pmr::memory_resource *mr,
                                    double s, double sigma)
{
    if (sigma <= 0.0 || s < 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    if (s == 0.0) {
        return std::make_tuple(sigma * std::sqrt(M_PI / 2.0),
                               sigma * sigma * (2.0 - M_PI / 2.0));
    }
    const double s2 = s * s;
    const double sg2 = sigma * sigma;
    const double z = -s2 / (2.0 * sg2);
    const double half = s2 / (4.0 * sg2);
    const double i0 = besseli_scalar(mr, 0.0, half);
    const double i1 = besseli_scalar(mr, 1.0, half);
    const double L = std::exp(z / 2.0) * ((1.0 - z) * i0 - z * i1);
    const double mean = sigma * std::sqrt(M_PI / 2.0) * L;
    const double var  = 2.0 * sg2 + s2 - mean * mean;
    return std::make_tuple(mean, var);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void ricepdf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ricepdf: requires (x, s, sigma)",
                    0, 0, "ricepdf", "", "m:ricepdf:nargin");
    outs[0] = ricepdf(ctx.engine->resource(), args[0],
                      args[1].toScalar(), args[2].toScalar());
}

void ricecdf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ricecdf: requires (x, s, sigma)",
                    0, 0, "ricecdf", "", "m:ricecdf:nargin");
    outs[0] = ricecdf(ctx.engine->resource(), args[0],
                      args[1].toScalar(), args[2].toScalar());
}

void riceinv_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("riceinv: requires (p, s, sigma)",
                    0, 0, "riceinv", "", "m:riceinv:nargin");
    outs[0] = riceinv(ctx.engine->resource(), args[0],
                      args[1].toScalar(), args[2].toScalar());
}

void ricernd_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ricernd: requires (s, sigma[, m, n])",
                    0, 0, "ricernd", "", "m:ricernd:nargin");
    const double s     = args[0].toScalar();
    const double sigma = args[1].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty()) cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3) cols = rows;
    outs[0] = ricernd(ctx.engine->resource(), s, sigma, rows, cols);
}

void ricestat_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    emit_vec_stat_2arg(args, nargout, outs, ctx, "ricestat",
                       [mr](double s, double sigma) { return ricestat(mr, s, sigma); });
}

} // namespace detail
} // namespace numkit::stats
