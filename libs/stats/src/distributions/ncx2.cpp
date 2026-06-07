// libs/stats/src/distributions/ncx2.cpp

#include <numkit/stats/distributions/ncx2.hpp>

#include <numkit/builtin/math/special/special.hpp>   // besseli, gammainc
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

inline double besseli_scalar(double nu, double xx, std::pmr::memory_resource *mr)
{
    Value nv = Value::scalar(nu, mr);
    Value xv = Value::scalar(xx, mr);
    return ::numkit::builtin::besseli(nv, xv, mr).toScalar();
}

inline double gammainc_scalar(double xx, double a, std::pmr::memory_resource *mr)
{
    Value xv = Value::scalar(xx, mr);
    Value av = Value::scalar(a,  mr);
    return ::numkit::builtin::gammainc(xv, av, mr).toScalar();
}

double ncx2cdf_one(double x, double k, double lambda, std::pmr::memory_resource *mr)
{
    if (x <= 0.0) return 0.0;
    if (lambda == 0.0) return gammainc_scalar(x / 2.0, k / 2.0, mr);
    const double halfL = lambda / 2.0;
    const double halfX = x / 2.0;
    double Pj = std::exp(-halfL);
    double sum = 0.0;
    const int maxIter = 2000;
    for (int j = 0; j < maxIter; ++j) {
        const double cdfTerm = gammainc_scalar(halfX, k / 2.0 + double(j), mr);
        const double contrib = Pj * cdfTerm;
        sum += contrib;
        if (j > 5 && contrib < 1e-16 * (sum + 1e-300)) break;
        Pj *= halfL / double(j + 1);
    }
    return std::min(1.0, sum);
}

} // anonymous

Value ncx2pdf(const Value &x, double k, double lambda, std::pmr::memory_resource *mr)
{
    if (k <= 0.0 || lambda < 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    if (lambda == 0.0) {
        // Reduce to chi²(k).
        const double C = 1.0 / (std::pow(2.0, k / 2.0) * std::tgamma(k / 2.0));
        return elementwise(x, [=](double xi) {
            if (xi <= 0.0) return (k == 2.0 && xi == 0.0) ? 0.5 : 0.0;
            return C * std::pow(xi, k / 2.0 - 1.0) * std::exp(-xi / 2.0);
        }, mr);
    }
    const double nu = (k - 2.0) / 2.0;
    return elementwise(x, [&](double xi) {
        if (xi < 0.0) return 0.0;
        if (xi == 0.0) return 0.0;
        const double sqrtLx = std::sqrt(lambda * xi);
        const double iv = besseli_scalar(nu, sqrtLx, mr);
        return 0.5 * std::exp(-(xi + lambda) / 2.0)
             * std::pow(xi / lambda, (k - 2.0) / 4.0)
             * iv;
    }, mr);
}

Value ncx2cdf(const Value &x, double k, double lambda, std::pmr::memory_resource *mr)
{
    if (k <= 0.0 || lambda < 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(x, [&](double xi) {
        return ncx2cdf_one(xi, k, lambda, mr);
    }, mr);
}

Value ncx2inv(const Value &p, double k, double lambda, std::pmr::memory_resource *mr)
{
    if (k <= 0.0 || lambda < 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p, [&](double pi) {
        if (!(pi >= 0.0 && pi <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 0.0;
        if (pi >= 1.0) return std::numeric_limits<double>::infinity();
        // Bracket on [0, mean + 10·sqrt(var)] then bisect.
        const double mean = k + lambda;
        const double var  = 2.0 * (k + 2.0 * lambda);
        double lo = 0.0;
        double hi = mean + 10.0 * std::sqrt(var) + 1.0;
        for (int iter = 0; iter < 50; ++iter) {
            if (ncx2cdf_one(hi, k, lambda, mr) >= pi) break;
            hi *= 2.0;
        }
        for (int iter = 0; iter < 80; ++iter) {
            const double mid = 0.5 * (lo + hi);
            if (mid == lo || mid == hi) return mid;
            const double F = ncx2cdf_one(mid, k, lambda, mr);
            if (F < pi) lo = mid; else hi = mid;
        }
        return 0.5 * (lo + hi);
    }, mr);
}

Value ncx2rnd(double k, double lambda, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (k <= 0.0 || lambda < 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    // Draw J ~ Poisson(λ/2), then X ~ chi²(k + 2J) = Gamma(shape=k/2+J, scale=2).
    std::poisson_distribution<int> pd(lambda / 2.0);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        const int J = pd(gen);
        std::gamma_distribution<double> gd(k / 2.0 + double(J), 2.0);
        od[i] = gd(gen);
    }
    return out;
}

std::tuple<double, double> ncx2stat(double k, double lambda)
{
    if (k <= 0.0 || lambda < 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(k + lambda, 2.0 * (k + 2.0 * lambda));
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void ncx2pdf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ncx2pdf: requires (x, k, lambda)",
                    0, 0, "ncx2pdf", "", "numkit:ncx2pdf:nargin");
    outs[0] = ncx2pdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void ncx2cdf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 3)
        throw Error("ncx2cdf: requires (x, k, lambda[, 'upper'])",
                    0, 0, "ncx2cdf", "", "numkit:ncx2cdf:nargin");
    Value v = ncx2cdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void ncx2inv_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ncx2inv: requires (p, k, lambda)",
                    0, 0, "ncx2inv", "", "numkit:ncx2inv:nargin");
    outs[0] = ncx2inv(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void ncx2rnd_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ncx2rnd: requires (k, lambda[, m, n])",
                    0, 0, "ncx2rnd", "", "numkit:ncx2rnd:nargin");
    const double k      = args[0].toScalar();
    const double lambda = args[1].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty()) cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3) cols = rows;
    outs[0] = ncx2rnd(k, lambda, rows, cols, ctx.engine->resource());
}

void ncx2stat_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx.engine->resource(), "ncx2stat",
                       [](double k, double lambda) { return ncx2stat(k, lambda); });
}

} // namespace detail
} // namespace numkit::stats
