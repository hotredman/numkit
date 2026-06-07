// libs/stats/src/distributions/fisher_f.cpp
//
// F-distribution. Composes betainc / betaincinv on
// (v1·x/(v1·x + v2), v1/2, v2/2) for cdf / icdf; rnd from two
// independent χ²-distributed samples.

#include <numkit/stats/distributions/fisher_f.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <cstring>
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

// Scalar pdf kernel for the parameter-broadcast path (vector v1/v2). Owns its
// per-element domain (v1<=0 or v2<=0 → NaN) and the x==0 boundary regimes.
// Mirrors the public fpdf bit-identically.
inline double fpdfK(double x, double v1, double v2)
{
    if (v1 <= 0.0 || v2 <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    const double a = 0.5 * v1;
    const double b = 0.5 * v2;
    const double lbeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    const double log_v1 = std::log(v1);
    const double log_v2 = std::log(v2);
    if (x < 0.0) return 0.0;
    if (x == 0.0) {
        if (v1 < 2.0) return std::numeric_limits<double>::infinity();
        if (v1 == 2.0)
            return std::exp(a * log_v1 + b * log_v2 - (a + b) * std::log(v2) - lbeta);
        return 0.0;
    }
    return std::exp(a * log_v1 + b * log_v2 + (a - 1.0) * std::log(x)
                    - (a + b) * std::log(v2 + v1 * x) - lbeta);
}

} // anonymous

Value fpdf(const Value &x, double v1, double v2, std::pmr::memory_resource *mr)
{
    if (v1 <= 0.0 || v2 <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    // log f(x; v1, v2) = (v1/2) log(v1) + (v2/2) log(v2)
    //                  + (v1/2 - 1) log x
    //                  - ((v1+v2)/2) log(v2 + v1 x)
    //                  - lbeta(v1/2, v2/2)
    // where lbeta(a, b) = lgamma(a) + lgamma(b) - lgamma(a+b).
    const double a = 0.5 * v1;
    const double b = 0.5 * v2;
    const double lbeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    const double log_v1 = std::log(v1);
    const double log_v2 = std::log(v2);
    return elementwise(x, [=](double xi) {
        if (xi < 0.0) return 0.0;
        if (xi == 0.0) {
            // Density at 0 has three regimes (limit of x^(v1/2 - 1) as x → 0+):
            //   v1 < 2  →  +Inf
            //   v1 == 2 →  finite, value computed without the (a-1)·log x term
            //   v1 > 2  →  0
            if (v1 <  2.0) return std::numeric_limits<double>::infinity();
            if (v1 == 2.0) {
                const double lp0 = a * log_v1 + b * log_v2
                                 - (a + b) * std::log(v2)
                                 - lbeta;
                return std::exp(lp0);
            }
            return 0.0;
        }
        const double lp = a * log_v1 + b * log_v2
                        + (a - 1.0) * std::log(xi)
                        - (a + b) * std::log(v2 + v1 * xi)
                        - lbeta;
        return std::exp(lp);
    }, mr);
}

Value fcdf(const Value &x, double v1, double v2, std::pmr::memory_resource *mr)
{
    if (v1 <= 0.0 || v2 <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    Value z = elementwise(x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        return (v1 * xi) / (v1 * xi + v2);
    }, mr);
    Value a = Value::scalar(0.5 * v1, mr);
    Value b = Value::scalar(0.5 * v2, mr);
    return ::numkit::builtin::betainc(z, a, b, mr);
}

Value finv(const Value &p, double v1, double v2, std::pmr::memory_resource *mr)
{
    if (v1 <= 0.0 || v2 <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    Value a = Value::scalar(0.5 * v1, mr);
    Value b = Value::scalar(0.5 * v2, mr);
    Value z = ::numkit::builtin::betaincinv(p, a, b, mr);
    // x = (v2 / v1) · z / (1 - z)
    return elementwise(z, [=](double zi){
        if (zi <= 0.0) return 0.0;
        if (zi >= 1.0) return std::numeric_limits<double>::infinity();
        return (v2 / v1) * zi / (1.0 - zi);
    }, mr);
}

Value frnd(double v1, double v2, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (v1 <= 0.0 || v2 <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::gamma_distribution<double> g1(0.5 * v1, 2.0); // χ²(v1)
    std::gamma_distribution<double> g2(0.5 * v2, 2.0); // χ²(v2)
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        const double x1 = g1(gen);
        const double x2 = g2(gen);
        od[i] = (x1 / v1) / (x2 / v2);
    }
    return out;
}

std::tuple<double, double> fstat(double v1, double v2)
{
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    // Invalid params ⇒ NaN/NaN (matches MATLAB R2025b).
    if (v1 <= 0.0 || v2 <= 0.0) return std::make_tuple(NaN, NaN);
    const double mean = (v2 > 2.0) ? (v2 / (v2 - 2.0)) : NaN;
    double var = NaN;
    if (v2 > 4.0) {
        const double num = 2.0 * v2 * v2 * (v1 + v2 - 2.0);
        const double den = v1 * (v2 - 2.0) * (v2 - 2.0) * (v2 - 4.0);
        var = num / den;
    }
    return std::make_tuple(mean, var);
}

// ── Noncentral F ────────────────────────────────────────────────────

namespace {

double ncfpdf_one(double x, double nu1, double nu2, double delta)
{
    if (!(nu1 > 0.0) || !(nu2 > 0.0) || delta < 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    if (x <= 0.0) return 0.0;
    if (delta == 0.0) {
        // Central F pdf in log-space.
        const double log_norm = std::lgamma(0.5 * (nu1 + nu2))
                              - std::lgamma(0.5 * nu1)
                              - std::lgamma(0.5 * nu2)
                              + 0.5 * nu1 * (std::log(nu1) - std::log(nu2));
        return std::exp(log_norm + (0.5 * nu1 - 1.0) * std::log(x)
                       - 0.5 * (nu1 + nu2) * std::log1p(nu1 * x / nu2));
    }
    const double L = 0.5 * delta;
    const double log_L = std::log(L);
    const double log_r = std::log(nu1 / nu2);
    const double log_x = std::log(x);
    const double log_one_plus_rx = std::log1p(nu1 * x / nu2);

    double sum = 0.0, abs_sum = 0.0;
    constexpr int kMax = 2000;
    for (int k = 0; k < kMax; ++k) {
        // log term_k
        const double a = 0.5 * nu1 + double(k);
        const double b = 0.5 * nu2;
        const double log_beta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
        const double log_term = -L + double(k) * log_L - std::lgamma(double(k) + 1.0)
                              + a * log_r
                              + (a - 1.0) * log_x
                              - (a + b) * log_one_plus_rx
                              - log_beta;
        const double t = std::exp(log_term);
        sum += t;
        abs_sum += t;
        if (k > 10 && t < 1e-16 * abs_sum) break;
    }
    return std::max(0.0, sum);
}

} // anonymous

Value ncfpdf(const Value &x, double nu1, double nu2, double delta,
             std::pmr::memory_resource *mr)
{
    return elementwise(x, [&](double xi) { return ncfpdf_one(xi, nu1, nu2, delta); }, mr);
}

namespace {

inline double betainc_scalar(double y, double a, double b,
                             std::pmr::memory_resource *mr)
{
    Value yv = Value::scalar(y, mr);
    Value av = Value::scalar(a, mr);
    Value bv = Value::scalar(b, mr);
    return ::numkit::builtin::betainc(yv, av, bv, mr).toScalar();
}

double ncfcdf_one(double x, double nu1, double nu2, double delta,
                  std::pmr::memory_resource *mr)
{
    if (!(nu1 > 0.0) || !(nu2 > 0.0) || delta < 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    if (x <= 0.0) return 0.0;
    const double y = (nu1 * x) / (nu1 * x + nu2);
    if (delta == 0.0)
        return betainc_scalar(y, 0.5 * nu1, 0.5 * nu2, mr);

    const double L = 0.5 * delta;
    double Pj = std::exp(-L);
    double sum = 0.0;
    constexpr int kMax = 2000;
    for (int k = 0; k < kMax; ++k) {
        const double I = betainc_scalar(y, 0.5 * nu1 + double(k), 0.5 * nu2, mr);
        const double t = Pj * I;
        sum += t;
        if (k > 5 && t < 1e-16 * (sum + 1e-300)) break;
        Pj *= L / double(k + 1);
    }
    if (sum > 1.0) sum = 1.0;
    if (sum < 0.0) sum = 0.0;
    return sum;
}

double ncfinv_one(double p, double nu1, double nu2, double delta,
                  std::pmr::memory_resource *mr)
{
    if (!(nu1 > 0.0) || !(nu2 > 0.0) || delta < 0.0
        || std::isnan(p) || p < 0.0 || p > 1.0)
        return std::numeric_limits<double>::quiet_NaN();
    if (p == 0.0) return 0.0;
    if (p == 1.0) return std::numeric_limits<double>::infinity();

    // Initial guess: central finv (or rough fallback for δ > 0).
    double x;
    {
        Value pv = Value::scalar(p, mr);
        x = finv(pv, nu1, nu2, mr).toScalar();
        if (!(x > 0.0) || !std::isfinite(x)) x = 1.0;
    }
    // Heuristic right-shift for δ > 0: noncentral F has larger mean.
    if (delta > 0.0) x *= (1.0 + delta / nu1);

    double lo = 0.0;
    double hi = std::max(x + 1.0, 50.0 * x + 50.0);
    while (ncfcdf_one(hi, nu1, nu2, delta, mr) < p) {
        hi *= 2.0;
        if (!std::isfinite(hi)) break;
    }

    for (int it = 0; it < 80; ++it) {
        const double F  = ncfcdf_one(x, nu1, nu2, delta, mr);
        const double f  = ncfpdf_one(x, nu1, nu2, delta);
        const double err = F - p;
        if (std::fabs(err) < 1e-14) return x;
        if (err > 0.0) hi = x; else lo = x;
        double x_new;
        if (f > 1e-300) {
            x_new = x - err / f;
            if (!std::isfinite(x_new) || x_new <= lo || x_new >= hi)
                x_new = 0.5 * (lo + hi);
        } else {
            x_new = 0.5 * (lo + hi);
        }
        if (std::fabs(x_new - x) < 1e-14 * std::max(1.0, std::fabs(x_new)))
            return x_new;
        x = x_new;
    }
    return x;
}

} // anonymous

Value ncfcdf(const Value &x, double nu1, double nu2, double delta,
             bool upper, std::pmr::memory_resource *mr)
{
    return elementwise(x, [&](double xi) {
        const double F = ncfcdf_one(xi, nu1, nu2, delta, mr);
        return upper ? 1.0 - F : F;
    }, mr);
}

Value ncfinv(const Value &p, double nu1, double nu2, double delta,
             std::pmr::memory_resource *mr)
{
    return elementwise(p, [&](double pi) { return ncfinv_one(pi, nu1, nu2, delta, mr); }, mr);
}

std::tuple<double, double> ncfstat(double nu1, double nu2, double delta)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (!(nu1 > 0.0) || !(nu2 > 0.0) || delta < 0.0)
        return {nan, nan};
    double m = nan, v = nan;
    if (nu2 > 2.0) {
        m = nu2 * (nu1 + delta) / (nu1 * (nu2 - 2.0));
    }
    if (nu2 > 4.0) {
        const double ratio = nu2 / nu1;
        const double num = (nu1 + delta) * (nu1 + delta)
                         + (nu1 + 2.0 * delta) * (nu2 - 2.0);
        const double den = (nu2 - 2.0) * (nu2 - 2.0) * (nu2 - 4.0);
        v = 2.0 * ratio * ratio * num / den;
    }
    return {m, v};
}

Value ncfrnd(double nu1, double nu2, double delta,
             std::size_t rows, std::size_t cols,
             std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (!(nu1 > 0.0) || !(nu2 > 0.0) || delta < 0.0 || rows * cols == 0)
        return out;
    double *od = out.doubleDataMut();
    const std::size_t n = rows * cols;
    std::poisson_distribution<int>   pd(0.5 * delta);
    std::gamma_distribution<double>  g2(0.5 * nu2, 2.0);   // χ²(ν₂)
    std::lock_guard<std::mutex> lk(mtx);
    for (std::size_t i = 0; i < n; ++i) {
        const int J = pd(gen);
        std::gamma_distribution<double> g1(0.5 * nu1 + static_cast<double>(J), 2.0);
        const double X1 = g1(gen);   // χ²(ν₁ + 2J) — noncentral χ² draw
        const double X2 = g2(gen);
        od[i] = (X1 / nu1) / (X2 / nu2);
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void fpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("fpdf: requires (x, v1, v2)", 0, 0, "fpdf", "", "numkit:fpdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &v1 = args[1];
    const Value &v2 = args[2];
    if (v1.isScalar() && v2.isScalar())
        outs[0] = fpdf(args[0], v1.toScalar(), v2.toScalar(), mr);
    else
        outs[0] = broadcast_dist3(args[0], v1, v2, mr, "fpdf", fpdfK);
}

void fcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a0 = args.subspan(0, stripUpperFlag(args, upper));
    if (a0.size() < 3)
        throw Error("fcdf: requires (x, v1, v2[, 'upper'])", 0, 0, "fcdf", "", "numkit:fcdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &v1 = a0[1];
    const Value &v2 = a0[2];
    Value v;
    if (v1.isScalar() && v2.isScalar()) {
        v = fcdf(a0[0], v1.toScalar(), v2.toScalar(), mr);
    } else {
        // F(x; v1, v2) = I_y(v1/2, v2/2), y = v1*x/(v1*x+v2). y broadcasts
        // (x,v1,v2) (v<=0 → NaN, x<=0 → 0); betainc broadcasts (y, v1/2, v2/2).
        const Value &x = a0[0];
        const size_t nx = x.numel(), n1 = v1.numel(), n2 = v2.numel();
        if (nx == 0 || n1 == 0 || n2 == 0) {
            v = dist_empty_like(nx == 0 ? x : (n1 == 0 ? v1 : v2), mr);
        } else {
            dist_match_numel({nx, n1, n2}, "fcdf");
            Value y = broadcast_dist3(x, v1, v2, mr, "fcdf", [](double xi, double d1, double d2) -> double {
                if (d1 <= 0.0 || d2 <= 0.0) return std::numeric_limits<double>::quiet_NaN();
                if (xi <= 0.0) return 0.0;
                return (d1 * xi) / (d1 * xi + d2);
            });
            Value a = elementwise(v1, [](double d) { return 0.5 * d; }, mr);
            Value b = elementwise(v2, [](double d) { return 0.5 * d; }, mr);
            v = ::numkit::builtin::betainc(y, a, b, mr);
        }
    }
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void finv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("finv: requires (p, v1, v2)", 0, 0, "finv", "", "numkit:finv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &v1 = args[1];
    const Value &v2 = args[2];
    if (v1.isScalar() && v2.isScalar()) {
        outs[0] = finv(args[0], v1.toScalar(), v2.toScalar(), mr);
        return;
    }
    // z = betaincinv(p, v1/2, v2/2); x = (v2/v1)·z/(1-z). Mirrors finv exactly
    // (z<=0 → 0, z>=1 → Inf, NaN z propagates); v<=0 → NaN per element.
    const Value &p = args[0];
    const size_t np = p.numel(), n1 = v1.numel(), n2 = v2.numel();
    if (np == 0 || n1 == 0 || n2 == 0) {
        outs[0] = dist_empty_like(np == 0 ? p : (n1 == 0 ? v1 : v2), mr);
        return;
    }
    const size_t N = dist_match_numel({np, n1, n2}, "finv");
    Value a = elementwise(v1, [](double d) { return 0.5 * d; }, mr);
    Value b = elementwise(v2, [](double d) { return 0.5 * d; }, mr);
    Value z = ::numkit::builtin::betaincinv(p, a, b, mr);
    const Value &ref = (n1 == N) ? v1 : (n2 == N ? v2 : p);
    Value out = dist_empty_like(ref, mr);
    double *od = out.doubleDataMut();
    const size_t nz = z.numel();
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    for (size_t i = 0; i < N; ++i) {
        const double d1 = v1.elemAsDouble(n1 == 1 ? 0 : i);
        const double d2 = v2.elemAsDouble(n2 == 1 ? 0 : i);
        if (d1 <= 0.0 || d2 <= 0.0) { od[i] = NaN; continue; }
        const double zi = z.elemAsDouble(nz == 1 ? 0 : i);
        if (zi <= 0.0) { od[i] = 0.0; continue; }
        if (zi >= 1.0) { od[i] = std::numeric_limits<double>::infinity(); continue; }
        od[i] = (d2 / d1) * zi / (1.0 - zi);
    }
    outs[0] = std::move(out);
}

void frnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("frnd: requires (v1, v2[, sz...])", 0, 0, "frnd", "", "numkit:frnd:nargin");
    const double v1 = args[0].toScalar();
    const double v2 = args[1].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 2, rows, cols);
    outs[0] = frnd(v1, v2, rows, cols, ctx.engine->resource());
}

void fstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx.engine->resource(), "fstat",
                       [](double v1, double v2) { return fstat(v1, v2); });
}

void ncfpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ncfpdf: requires (x, nu1, nu2, delta)",
                    0, 0, "ncfpdf", "", "numkit:ncfpdf:nargin");
    outs[0] = ncfpdf(args[0], args[1].toScalar(), args[2].toScalar(),
                     args[3].toScalar(), ctx.engine->resource());
}

void ncfcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 4)
        throw Error("ncfcdf: requires (x, nu1, nu2, delta[, 'upper'])",
                    0, 0, "ncfcdf", "", "numkit:ncfcdf:nargin");
    outs[0] = ncfcdf(args[0], args[1].toScalar(), args[2].toScalar(),
                     args[3].toScalar(), upper, ctx.engine->resource());
}

void ncfinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ncfinv: requires (p, nu1, nu2, delta)",
                    0, 0, "ncfinv", "", "numkit:ncfinv:nargin");
    outs[0] = ncfinv(args[0], args[1].toScalar(), args[2].toScalar(),
                     args[3].toScalar(), ctx.engine->resource());
}

void ncfstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ncfstat: requires (nu1, nu2, delta)",
                    0, 0, "ncfstat", "", "numkit:ncfstat:nargin");
    auto [m, v] = ncfstat(args[0].toScalar(), args[1].toScalar(), args[2].toScalar());
    outs[0] = Value::scalar(m, ctx.engine->resource());
    if (nargout >= 2)
        outs[1] = Value::scalar(v, ctx.engine->resource());
}

void ncfrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ncfrnd: requires (nu1, nu2, delta[, sz...])",
                    0, 0, "ncfrnd", "", "numkit:ncfrnd:nargin");
    const double nu1 = args[0].toScalar();
    const double nu2 = args[1].toScalar();
    const double delta = args[2].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 3, rows, cols);
    outs[0] = ncfrnd(nu1, nu2, delta, rows, cols, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
