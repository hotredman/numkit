// libs/stats/src/distributions/students_t.cpp
//
// Student's t-distribution. Composes betainc / betaincinv for cdf /
// icdf; rnd uses Z / sqrt(X/ν) with Z ~ N(0,1), X ~ χ²(ν).

#include <numkit/stats/distributions/students_t.hpp>

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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

// Read a Value's first element as scalar (for inputs we know are
// scalar Values produced by the helpers). Avoids extra copies.
double scalarOf(const Value &v) { return v.toScalar(); }

} // anonymous

Value tpdf(const Value &x, double nu, std::pmr::memory_resource *mr)
{
    if (!(nu > 0.0))  // nu <= 0 or NaN
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    // Gaussian limit: as nu -> Inf, t-PDF -> N(0, 1) PDF.
    if (std::isinf(nu)) {
        const double inv = 1.0 / std::sqrt(2.0 * M_PI);
        return elementwise(x, [inv](double xi){
            return inv * std::exp(-0.5 * xi * xi);
        }, mr);
    }
    // log f(x) = lgamma((ν+1)/2) - lgamma(ν/2) - 0.5*log(ν π)
    //          - ((ν+1)/2) * log(1 + x²/ν)
    const double log_norm = std::lgamma(0.5 * (nu + 1.0))
                          - std::lgamma(0.5 * nu)
                          - 0.5 * std::log(nu * M_PI);
    const double exponent = -0.5 * (nu + 1.0);
    return elementwise(x, [=](double xi) {
        return std::exp(log_norm + exponent * std::log1p(xi * xi / nu));
    }, mr);
}

Value tcdf(const Value &x, double nu, std::pmr::memory_resource *mr)
{
    if (nu <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    // Build z[i] = ν / (ν + x[i]²); pass through betainc with (ν/2, 1/2).
    // The cdf is then 1 - ½·I_z(ν/2, ½) for x ≥ 0 and ½·I_z otherwise.
    const size_t n = x.numel();
    Value z = elementwise(x, [=](double xi){ return nu / (nu + xi * xi); }, mr);
    Value a = Value::scalar(0.5 * nu, mr);
    Value b = Value::scalar(0.5, mr);
    Value Iz = ::numkit::builtin::betainc(z, a, b, mr);

    // Walk x and Iz in parallel, picking branch by sign of x.
    auto out = Value::matrix(x.dims().rows(), x.dims().cols(), ValueType::DOUBLE, mr);
    if (x.dims().is3D())
        out = Value::matrix3d(x.dims().rows(), x.dims().cols(), x.dims().pages(), ValueType::DOUBLE, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i) {
        const double xi = x.elemAsDouble(i);
        const double Ii = Iz.elemAsDouble(i);
        od[i] = (xi >= 0.0) ? 1.0 - 0.5 * Ii : 0.5 * Ii;
    }
    return out;
}

Value tinv(const Value &p, double nu, std::pmr::memory_resource *mr)
{
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double PINF = std::numeric_limits<double>::infinity();
    const double NINF = -PINF;
    if (!(nu > 0.0))  // nu <= 0 or NaN
        return elementwise(p, [NaN](double){ return NaN; }, mr);
    // Gaussian limit: as nu -> Inf, t-distribution -> N(0, 1), so tinv(p, Inf) = norminv(p).
    if (std::isinf(nu)) {
        return elementwise(p, [NaN, PINF, NINF](double pi){
            if (std::isnan(pi) || pi < 0.0 || pi > 1.0) return NaN;
            if (pi == 0.0) return NINF;
            if (pi == 1.0) return PINF;
            // norminv(p) = sqrt(2)·erfinv(2p - 1); local Winitzki + 2 Newton steps.
            const double y = 2.0 * pi - 1.0;
            if (y >= 1.0) return PINF;
            if (y <= -1.0) return NINF;
            const double a = 0.147;
            const double ln1m = std::log(1.0 - y * y);
            const double term = 2.0 / (M_PI * a) + 0.5 * ln1m;
            double xv = std::copysign(std::sqrt(std::sqrt(term * term - ln1m / a) - term), y);
            for (int it = 0; it < 2; ++it) {
                const double e  = std::erf(xv) - y;
                const double de = 2.0 * std::exp(-xv * xv) / std::sqrt(M_PI);
                xv -= e / de;
            }
            return std::sqrt(2.0) * xv;
        }, mr);
    }
    // Use betaincinv: from p ↔ z via I_z(ν/2, 1/2) = q where:
    //   p ≥ 0.5: q = 2(1-p), x = +sqrt(ν · (1/z - 1))
    //   p < 0.5: q = 2p,     x = -sqrt(ν · (1/z - 1))
    const size_t n = p.numel();
    auto qv = elementwise(p, [](double pi){
        // Out-of-range / NaN handled later; clamp to a valid betaincinv arg here.
        if (std::isnan(pi) || pi < 0.0 || pi > 1.0) return 0.5;
        return (pi >= 0.5) ? 2.0 * (1.0 - pi) : 2.0 * pi;
    }, mr);
    Value a = Value::scalar(0.5 * nu, mr);
    Value b = Value::scalar(0.5, mr);
    Value zv = ::numkit::builtin::betaincinv(qv, a, b, mr);

    auto out = Value::matrix(p.dims().rows(), p.dims().cols(), ValueType::DOUBLE, mr);
    if (p.dims().is3D())
        out = Value::matrix3d(p.dims().rows(), p.dims().cols(), p.dims().pages(), ValueType::DOUBLE, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i) {
        const double pi = p.elemAsDouble(i);
        if (std::isnan(pi) || pi < 0.0 || pi > 1.0) { od[i] = NaN; continue; }
        if (pi == 0.0) { od[i] = NINF; continue; }
        if (pi == 1.0) { od[i] = PINF; continue; }
        const double zi = zv.elemAsDouble(i);
        if (zi <= 0.0) { od[i] = (pi >= 0.5) ? PINF : NINF; continue; }
        const double mag = std::sqrt(nu * (1.0 / zi - 1.0));
        od[i] = (pi >= 0.5) ? mag : -mag;
    }
    return out;
}

Value trnd(double nu, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (nu <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::normal_distribution<double> nd(0.0, 1.0);
    std::gamma_distribution<double>  gd(0.5 * nu, 2.0);  // χ²(ν) = Gamma(ν/2, 2)
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        const double Z = nd(gen);
        const double X = gd(gen);
        od[i] = Z / std::sqrt(X / nu);
    }
    return out;
}

std::tuple<double, double> tstat(double nu)
{
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    if (nu <= 0.0) return std::make_tuple(NaN, NaN);
    // mean defined only for nu > 1; variance only for nu > 2.
    const double m = (nu > 1.0) ? 0.0 : NaN;
    const double v = (nu > 2.0) ? (nu / (nu - 2.0)) : NaN;
    return std::make_tuple(m, v);
}

// ── Noncentral t ────────────────────────────────────────────────────

namespace {

// Standard normal CDF.
inline double phiCdf(double z)
{
    return 0.5 * (1.0 + std::erf(z / std::sqrt(2.0)));
}

inline double betainc_scalar(double y, double a, double b, std::pmr::memory_resource *mr)
{
    Value yv = Value::scalar(y, mr);
    Value av = Value::scalar(a, mr);
    Value bv = Value::scalar(b, mr);
    return ::numkit::builtin::betainc(yv, av, bv, mr).toScalar();
}

double nctpdf_one(double x, double nu, double delta,
                  std::pmr::memory_resource *mr)
{
    if (!(nu > 0.0))
        return std::numeric_limits<double>::quiet_NaN();
    // δ = 0 → central tpdf.
    if (delta == 0.0) {
        const double log_norm = std::lgamma(0.5 * (nu + 1.0))
                              - std::lgamma(0.5 * nu)
                              - 0.5 * std::log(nu * M_PI);
        return std::exp(log_norm - 0.5 * (nu + 1.0) * std::log1p(x * x / nu));
    }
    if (x == 0.0) {
        // f(0; ν, δ) = Γ((ν+1)/2) / [√(νπ) · Γ(ν/2)] · exp(-δ²/2)
        const double log_v = std::lgamma(0.5 * (nu + 1.0))
                           - 0.5 * std::log(nu * M_PI)
                           - std::lgamma(0.5 * nu)
                           - 0.5 * delta * delta;
        return std::exp(log_v);
    }
    const double npx2 = nu + x * x;
    // log-prefactor: (ν/2)·log(ν) - δ²/2 - 0.5·log(π) - lgamma(ν/2)
    //              - (ν+1)/2 · log(ν + x²)
    const double log_pref = 0.5 * nu * std::log(nu)
                          - 0.5 * delta * delta
                          - 0.5 * std::log(M_PI)
                          - std::lgamma(0.5 * nu)
                          - 0.5 * (nu + 1.0) * std::log(npx2);
    const double xd = x * delta;
    const int sign_factor = (xd >= 0.0) ? 1 : -1;
    const double log_t = std::log(std::fabs(xd) * std::sqrt(2.0));   // log(|xδ|√2)
    const double half_log_npx2 = 0.5 * std::log(npx2);

    double sum = 0.0, abs_sum = 0.0;
    int sign_k = 1;
    constexpr int kMax = 2000;
    for (int k = 0; k < kMax; ++k) {
        const double log_termk = std::lgamma(0.5 * (nu + double(k) + 1.0))
                              + double(k) * log_t
                              - double(k) * half_log_npx2
                              - std::lgamma(double(k) + 1.0);
        const double abs_term = std::exp(log_termk);
        const double contrib = sign_k * abs_term;
        sum += contrib;
        abs_sum += abs_term;
        if (k > 10 && abs_term < 1e-16 * abs_sum) break;
        sign_k *= sign_factor;
    }
    double v = std::exp(log_pref) * sum;
    if (!std::isfinite(v) || v < 0.0) v = std::max(v, 0.0);
    return v;
}

double nctcdf_one(double x, double nu, double delta,
                  std::pmr::memory_resource *mr)
{
    if (!(nu > 0.0))
        return std::numeric_limits<double>::quiet_NaN();
    if (delta == 0.0) {
        // Central tcdf branch.
        const double z = nu / (nu + x * x);
        const double I = betainc_scalar(z, 0.5 * nu, 0.5, mr);
        return (x >= 0.0) ? 1.0 - 0.5 * I : 0.5 * I;
    }
    // Symmetry for negative x.
    if (x < 0.0) return 1.0 - nctcdf_one(-x, nu, -delta, mr);
    // At x = 0: F = Φ(-δ).
    if (x == 0.0) return phiCdf(-delta);

    const double y = (x * x) / (x * x + nu);
    const double z = 0.5 * delta * delta;
    const double phi_neg_d = phiCdf(-delta);

    // Series 1 (P_k coefficient): Poisson pmf with mean z.
    // Series 2 (Q_k coefficient): (δ/(2√(2π))) · (z^k / Γ(k+3/2)) · e^{-z}.
    const double e_neg_z = std::exp(-z);
    double alpha = e_neg_z;                                    // P_0
    double beta  = (2.0 / std::sqrt(M_PI)) * e_neg_z;          // (z^0 / Γ(3/2)) · e^{-z}

    double sum1 = 0.0, sum2 = 0.0;
    constexpr int kMax = 2000;
    for (int k = 0; k < kMax; ++k) {
        const double I1 = betainc_scalar(y, double(k) + 0.5, 0.5 * nu, mr);
        const double I2 = betainc_scalar(y, double(k) + 1.0, 0.5 * nu, mr);
        const double t1 = alpha * I1;
        const double t2 = beta  * I2;
        sum1 += t1;
        sum2 += t2;
        if (k > 10 && t1 < 1e-16 * (sum1 + 1e-300)
                  && t2 < 1e-16 * (sum2 + 1e-300)) break;
        // Recurrences.
        alpha *= z / double(k + 1);
        beta  *= z / (double(k) + 1.5);
    }

    double F = phi_neg_d + 0.5 * sum1 + (delta / (2.0 * std::sqrt(2.0))) * sum2;
    if (F < 0.0) F = 0.0;
    if (F > 1.0) F = 1.0;
    return F;
}

} // anonymous

Value nctpdf(const Value &x, double nu, double delta,
             std::pmr::memory_resource *mr)
{
    return elementwise(x, [&](double xi) { return nctpdf_one(xi, nu, delta, mr); }, mr);
}

Value nctcdf(const Value &x, double nu, double delta, bool upper,
             std::pmr::memory_resource *mr)
{
    return elementwise(x, [&](double xi) {
        double F = nctcdf_one(xi, nu, delta, mr);
        return upper ? 1.0 - F : F;
    }, mr);
}

namespace {

// Central tinv (scalar): inverse of central t-cdf via betaincinv.
double tinv_scalar(double p, double nu, std::pmr::memory_resource *mr)
{
    if (p <= 0.0) return -std::numeric_limits<double>::infinity();
    if (p >= 1.0) return  std::numeric_limits<double>::infinity();
    if (p == 0.5) return 0.0;
    // betaincinv(2·min(p,1-p), ν/2, ½) → y; then x = sign·sqrt(ν(1/y - 1)).
    const bool lower_half = (p < 0.5);
    const double tail = lower_half ? 2.0 * p : 2.0 * (1.0 - p);
    Value tv = Value::scalar(tail, mr);
    Value av = Value::scalar(0.5 * nu, mr);
    Value bv = Value::scalar(0.5, mr);
    const double y = ::numkit::builtin::betaincinv(tv, av, bv, mr).toScalar();
    if (y <= 0.0) return lower_half ? -std::numeric_limits<double>::infinity()
                                     :  std::numeric_limits<double>::infinity();
    if (y >= 1.0) return 0.0;
    const double x = std::sqrt(nu * (1.0 / y - 1.0));
    return lower_half ? -x : x;
}

double nctinv_one(double p, double nu, double delta,
                  std::pmr::memory_resource *mr)
{
    if (!(nu > 0.0)) return std::numeric_limits<double>::quiet_NaN();
    if (std::isnan(p) || p < 0.0 || p > 1.0)
        return std::numeric_limits<double>::quiet_NaN();
    if (p == 0.0) return -std::numeric_limits<double>::infinity();
    if (p == 1.0) return  std::numeric_limits<double>::infinity();
    if (delta == 0.0) return tinv_scalar(p, nu, mr);

    // Initial guess: central tinv shifted by δ.
    double x = tinv_scalar(p, nu, mr) + delta;
    if (!std::isfinite(x)) x = delta;

    // Establish a bracket [lo, hi] for fallback bisection.
    // Find any x_lo with cdf < p and any x_hi with cdf > p.
    double lo = std::min(x - 1.0, delta - 50.0);
    double hi = std::max(x + 1.0, delta + 50.0);
    while (nctcdf_one(lo, nu, delta, mr) > p) {
        lo -= std::max(1.0, std::fabs(lo));
        if (!std::isfinite(lo)) break;
    }
    while (nctcdf_one(hi, nu, delta, mr) < p) {
        hi += std::max(1.0, std::fabs(hi));
        if (!std::isfinite(hi)) break;
    }

    // Newton + bisection guard, ≤ 60 iterations.
    for (int it = 0; it < 60; ++it) {
        const double F  = nctcdf_one(x, nu, delta, mr);
        const double f  = nctpdf_one(x, nu, delta, mr);
        const double err = F - p;
        if (std::fabs(err) < 1e-14) return x;
        // Bisect endpoint update.
        if (err > 0.0) hi = x; else lo = x;
        double x_new;
        if (f > 1e-300) {
            x_new = x - err / f;
            // Reject Newton step outside bracket — bisect instead.
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

Value nctinv(const Value &p, double nu, double delta,
             std::pmr::memory_resource *mr)
{
    return elementwise(p, [&](double pi) { return nctinv_one(pi, nu, delta, mr); }, mr);
}

std::tuple<double, double> nctstat(double nu, double delta)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (!(nu > 0.0)) return {nan, nan};
    double m = nan, v = nan;
    if (nu > 1.0) {
        // m = δ · sqrt(ν/2) · Γ((ν-1)/2) / Γ(ν/2)
        const double log_ratio = std::lgamma(0.5 * (nu - 1.0)) - std::lgamma(0.5 * nu);
        m = delta * std::sqrt(0.5 * nu) * std::exp(log_ratio);
    }
    if (nu > 2.0) {
        v = nu * (1.0 + delta * delta) / (nu - 2.0) - m * m;
    }
    return {m, v};
}

Value nctrnd(double nu, double delta, std::size_t rows, std::size_t cols,
             std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (!(nu > 0.0) || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const std::size_t n = rows * cols;
    std::normal_distribution<double> nd(0.0, 1.0);
    std::gamma_distribution<double>  gd(0.5 * nu, 2.0);   // χ²(ν) = Gamma(ν/2, 2)
    std::lock_guard<std::mutex> lk(mtx);
    for (std::size_t i = 0; i < n; ++i) {
        const double Z = nd(gen);
        const double X = gd(gen);
        od[i] = (Z + delta) / std::sqrt(X / nu);
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void tpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tpdf: requires (x, nu)", 0, 0, "tpdf", "", "numkit:tpdf:nargin");
    outs[0] = tpdf(args[0], args[1].toScalar(), ctx.engine->resource());
}

void tcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 2)
        throw Error("tcdf: requires (x, nu[, 'upper'])", 0, 0, "tcdf", "", "numkit:tcdf:nargin");
    Value v = tcdf(args[0], args[1].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void tinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tinv: requires (p, nu)", 0, 0, "tinv", "", "numkit:tinv:nargin");
    outs[0] = tinv(args[0], args[1].toScalar(), ctx.engine->resource());
}

void trnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("trnd: requires nu[, sz...]", 0, 0, "trnd", "", "numkit:trnd:nargin");
    const double nu = args[0].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 1, rows, cols);
    outs[0] = trnd(nu, rows, cols, ctx.engine->resource());
}

void tstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx, "tstat",
                       [](double nu) { return tstat(nu); });
}

void nctpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nctpdf: requires (x, nu, delta)",
                    0, 0, "nctpdf", "", "numkit:nctpdf:nargin");
    outs[0] = nctpdf(args[0], args[1].toScalar(), args[2].toScalar(),
                     ctx.engine->resource());
}

void nctcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 3)
        throw Error("nctcdf: requires (x, nu, delta[, 'upper'])",
                    0, 0, "nctcdf", "", "numkit:nctcdf:nargin");
    outs[0] = nctcdf(args[0], args[1].toScalar(), args[2].toScalar(), upper,
                     ctx.engine->resource());
}

void nctinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nctinv: requires (p, nu, delta)",
                    0, 0, "nctinv", "", "numkit:nctinv:nargin");
    outs[0] = nctinv(args[0], args[1].toScalar(), args[2].toScalar(),
                     ctx.engine->resource());
}

void nctstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("nctstat: requires (nu, delta)",
                    0, 0, "nctstat", "", "numkit:nctstat:nargin");
    auto [m, v] = nctstat(args[0].toScalar(), args[1].toScalar());
    outs[0] = Value::scalar(m, ctx.engine->resource());
    if (nargout >= 2)
        outs[1] = Value::scalar(v, ctx.engine->resource());
}

void nctrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("nctrnd: requires (nu, delta[, sz...])",
                    0, 0, "nctrnd", "", "numkit:nctrnd:nargin");
    const double nu = args[0].toScalar();
    const double delta = args[1].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 2, rows, cols);
    outs[0] = nctrnd(nu, delta, rows, cols, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
