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

// (r, p) valid iff r>0 (r may be non-integer) and p ∈ (0, 1]. Else → NaN.
inline bool nbin_params_ok(double r, double p) {
    return r > 0.0 && p > 0.0 && p <= 1.0;
}

inline double nbinpdfK(double k, double r, double p) {
    if (!nbin_params_ok(r, p)) return std::numeric_limits<double>::quiet_NaN();
    return nbin_pmf(k, r, p);
}

// Smallest integer k with F(k; r, p) ≥ qival, via pmf-recurrence
// pmf(k+1)/pmf(k) = (k+r)/(k+1)·(1-p). (r, p) assumed valid.
inline double nbin_inv_scalar(double qival, double r, double p) {
    if (!(qival >= 0.0 && qival <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
    if (qival == 0.0) return 0.0;
    if (qival >= 1.0) return std::numeric_limits<double>::infinity();
    if (p == 1.0) return 0.0;
    const double q1m = 1.0 - p;
    double pmf = std::pow(p, r);   // pmf(0)
    double cdf = pmf;
    const double tol = std::max(1e-13, qival * 1e-13);
    if (cdf >= qival - tol) return 0.0;
    for (double k = 0.0; k < 1e12; k += 1.0) {
        pmf *= (k + r) / (k + 1.0) * q1m;
        cdf += pmf;
        if (cdf >= qival - tol) return k + 1.0;
    }
    return std::numeric_limits<double>::infinity();
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
    if (!nbin_params_ok(r, p))
        return elementwise(q, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(q, [=](double qi) { return nbin_inv_scalar(qi, r, p); }, mr);
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
    auto *mr = ctx.engine->resource();
    const Value &r = args[1];
    const Value &p = args[2];
    if (r.isScalar() && p.isScalar())
        outs[0] = nbinpdf(args[0], r.toScalar(), p.toScalar(), mr);
    else
        outs[0] = broadcast_dist3(args[0], r, p, mr, "nbinpdf", nbinpdfK);
}

void nbincdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    if (a.size() < 3)
        throw Error("nbincdf: requires (k, r, p[, 'upper'])", 0, 0, "nbincdf", "", "numkit:nbincdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &r = a[1];
    const Value &p = a[2];
    Value v;
    if (r.isScalar() && p.isScalar()) {
        v = nbincdf(a[0], r.toScalar(), p.toScalar(), mr);
    } else {
        // Per-element F(k_i; r_i, p_i) via nbin_cdf_scalar (betainc); invalid →
        // NaN. Same per-element-betainc cost as the scalar nbincdf over a vec k.
        const Value &k = a[0];
        const size_t nk = k.numel(), nr = r.numel(), np = p.numel();
        if (nk == 0 || nr == 0 || np == 0) {
            v = dist_empty_like(nk == 0 ? k : (nr == 0 ? r : p), mr);
        } else {
            const size_t N = dist_match_numel({nk, nr, np}, "nbincdf");
            const Value &ref = (nr == N) ? r : (np == N ? p : k);
            v = dist_empty_like(ref, mr);
            double *od = v.doubleDataMut();
            const double NaN = std::numeric_limits<double>::quiet_NaN();
            for (size_t i = 0; i < N; ++i) {
                const double ri = r.elemAsDouble(nr == 1 ? 0 : i);
                const double pi = p.elemAsDouble(np == 1 ? 0 : i);
                const double ki = k.elemAsDouble(nk == 1 ? 0 : i);
                od[i] = nbin_params_ok(ri, pi) ? nbin_cdf_scalar(ki, ri, pi, mr) : NaN;
            }
        }
    }
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void nbininv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nbininv: requires (q, r, p)", 0, 0, "nbininv", "", "numkit:nbininv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &r = args[1];
    const Value &p = args[2];
    if (r.isScalar() && p.isScalar()) {
        outs[0] = nbininv(args[0], r.toScalar(), p.toScalar(), mr);
        return;
    }
    const Value &q = args[0];
    const size_t nq = q.numel(), nr = r.numel(), np = p.numel();
    if (nq == 0 || nr == 0 || np == 0) {
        outs[0] = dist_empty_like(nq == 0 ? q : (nr == 0 ? r : p), mr);
        return;
    }
    const size_t N = dist_match_numel({nq, nr, np}, "nbininv");
    const Value &ref = (nr == N) ? r : (np == N ? p : q);
    Value out = dist_empty_like(ref, mr);
    double *od = out.doubleDataMut();
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    for (size_t i = 0; i < N; ++i) {
        const double ri = r.elemAsDouble(nr == 1 ? 0 : i);
        const double pi = p.elemAsDouble(np == 1 ? 0 : i);
        const double qi = q.elemAsDouble(nq == 1 ? 0 : i);
        od[i] = nbin_params_ok(ri, pi) ? nbin_inv_scalar(qi, ri, pi) : NaN;
    }
    outs[0] = std::move(out);
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
    emit_vec_stat_2arg(args, nargout, outs, ctx.engine->resource(), "nbinstat",
                       [](double r, double p) { return nbinstat(r, p); });
}

} // namespace detail
} // namespace numkit::stats
