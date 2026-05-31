// libs/stats/src/distributions/negbin.cpp

#include <numkit/stats/distributions/negbin.hpp>

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

// log f(k; r, p) = lgamma(k + r) - lgamma(k+1) - lgamma(r)
//                + r·log(p) + k·log(1-p)
inline double nbin_pmf(double k, double r, double p) {
    if (k < 0.0 || std::floor(k) != k) return 0.0;
    if (p == 1.0) return (k == 0.0) ? 1.0 : 0.0;
    const double lc = std::lgamma(k + r) - std::lgamma(k + 1.0) - std::lgamma(r);
    return std::exp(lc + r * std::log(p) + k * std::log1p(-p));
}

inline double nbin_cdf_scalar(double k, double r, double p, std::pmr::memory_resource *mr) {
    if (k < 0.0) return 0.0;
    if (p == 1.0) return 1.0;
    const double kf = std::floor(k);
    // F(k; r, p) = I_p(r, ⌊k⌋ + 1)
    Value xv = Value::scalar(p, mr);
    Value av = Value::scalar(r, mr);
    Value bv = Value::scalar(kf + 1.0, mr);
    Value rv = ::numkit::builtin::betainc(xv, av, bv, mr);
    return rv.toScalar();
}

} // anonymous

Value nbinpdf(const Value &k, double r, double p, std::pmr::memory_resource *mr)
{
    if (r <= 0.0 || p <= 0.0 || p > 1.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki){ return nbin_pmf(ki, r, p); }, mr);
}

Value nbincdf(const Value &k, double r, double p, std::pmr::memory_resource *mr)
{
    if (r <= 0.0 || p <= 0.0 || p > 1.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki){ return nbin_cdf_scalar(ki, r, p, mr); }, mr);
}

Value nbininv(const Value &q, double r, double p, std::pmr::memory_resource *mr)
{
    if (r <= 0.0 || p <= 0.0 || p > 1.0)
        return elementwise(q, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(q, [=](double qi) {
        if (!(qi >= 0.0 && qi <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
        if (qi == 0.0) return 0.0;
        if (qi >= 1.0) return std::numeric_limits<double>::infinity();
        if (p == 1.0) return 0.0;
        // pmf-recurrence walk: pmf(k+1) / pmf(k) = (k+r)/(k+1) · (1-p).
        const double q1m = 1.0 - p;
        double pmf = std::pow(p, r);   // pmf(0)
        double cdf = pmf;
        const double tol = std::max(1e-13, qi * 1e-13);
        if (cdf >= qi - tol) return 0.0;
        for (double k = 0.0; k < 1e12; k += 1.0) {
            pmf *= (k + r) / (k + 1.0) * q1m;
            cdf += pmf;
            if (cdf >= qi - tol) return k + 1.0;
        }
        return std::numeric_limits<double>::infinity();
    }, mr);
}

Value nbinrnd(double r, double p, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (r <= 0.0 || p <= 0.0 || p > 1.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t cnt = rows * cols;
    // For real (non-integer) r, std::negative_binomial_distribution requires
    // integer k; sample via Gamma-Poisson mixture: λ ~ Gamma(r, (1-p)/p),
    // K | λ ~ Poisson(λ).
    std::gamma_distribution<double> gd(r, (1.0 - p) / p);
    std::poisson_distribution<int>  pd_dummy(1.0); (void)pd_dummy;
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < cnt; ++i) {
        const double lam = gd(gen);
        std::poisson_distribution<int> pd(lam <= 0.0 ? 0.0 : lam);
        od[i] = static_cast<double>(pd(gen));
    }
    return out;
}

std::tuple<double, double> nbinstat(double r, double p)
{
    if (r <= 0.0 || p <= 0.0 || p > 1.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double mean = r * (1.0 - p) / p;
    const double var  = mean / p;
    return std::make_tuple(mean, var);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void nbinpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nbinpdf: requires (k, r, p)", 0, 0, "nbinpdf", "", "numkit:nbinpdf:nargin");
    outs[0] = nbinpdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void nbincdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 3)
        throw Error("nbincdf: requires (k, r, p[, 'upper'])", 0, 0, "nbincdf", "", "numkit:nbincdf:nargin");
    Value v = nbincdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void nbininv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nbininv: requires (q, r, p)", 0, 0, "nbininv", "", "numkit:nbininv:nargin");
    outs[0] = nbininv(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void nbinrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("nbinrnd: requires (r, p[, m, n])", 0, 0, "nbinrnd", "", "numkit:nbinrnd:nargin");
    const double r = args[0].toScalar();
    const double p = args[1].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty()) cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3) cols = rows;
    outs[0] = nbinrnd(r, p, rows, cols, ctx.engine->resource());
}

void nbinstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx, "nbinstat",
                       [](double r, double p) { return nbinstat(r, p); });
}

} // namespace detail
} // namespace numkit::stats
