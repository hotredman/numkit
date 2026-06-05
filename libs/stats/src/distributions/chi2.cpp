// libs/stats/src/distributions/chi2.cpp
//
// Chi-squared distribution. Composes the existing gammainc /
// gammaincinv on (k/2, x/2) for cdf / icdf.

#include <numkit/stats/distributions/chi2.hpp>

#include <numkit/builtin/math/random/rng.hpp>      // sharedEngine, rngMutex
#include <numkit/builtin/math/special/special.hpp> // gammainc, gammaincinv

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <cstring>
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

// Scalar pdf kernel for the parameter-broadcast path (vector k). Owns its
// per-element domain: k<0 → NaN, k==0 → 0 (degenerate Chi²(0)). Mirrors the
// public chi2pdf formula exactly.
inline double chi2pdfK(double x, double k)
{
    if (k < 0.0) return std::numeric_limits<double>::quiet_NaN();
    if (k == 0.0) return 0.0;
    if (x < 0.0) return 0.0;
    const double half_k = 0.5 * k;
    const double log_norm = -half_k * std::log(2.0) - std::lgamma(half_k);
    if (x == 0.0)
        return (k == 2.0) ? 0.5 : (k > 2.0 ? 0.0 : std::numeric_limits<double>::infinity());
    return std::exp(log_norm + (half_k - 1.0) * std::log(x) - 0.5 * x);
}

} // anonymous

Value chi2pdf(const Value &x, double k, std::pmr::memory_resource *mr)
{
    // MATLAB convention: k < 0 ⇒ NaN; k == 0 ⇒ 0 (degenerate Chi²(0)
    // has all mass at 0, so density is 0 almost everywhere).
    if (k < 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    if (k == 0.0)
        return elementwise(x, [](double){ return 0.0; }, mr);
    // pdf(x; k) = (1/(2^(k/2) Γ(k/2))) x^(k/2-1) e^(-x/2), x ≥ 0
    // Compute log-pdf and exp to avoid overflow on small/large k.
    const double half_k = 0.5 * k;
    const double log_norm = -half_k * std::log(2.0) - std::lgamma(half_k);
    return elementwise(x, [=](double xi) {
        if (xi < 0.0) return 0.0;
        if (xi == 0.0) return (k == 2.0) ? 0.5 : (k > 2.0 ? 0.0 : std::numeric_limits<double>::infinity());
        const double lp = log_norm + (half_k - 1.0) * std::log(xi) - 0.5 * xi;
        return std::exp(lp);
    }, mr);
}

Value chi2cdf(const Value &x, double k, std::pmr::memory_resource *mr)
{
    if (k <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    // F(x; k) = gammainc(x/2, k/2) (regularized lower).
    // builtin::gammainc takes Values for x and a — call elementwise.
    auto out = elementwise(x, [](double xi){ return std::max(0.0, 0.5 * xi); }, mr);
    Value ar = Value::scalar(0.5 * k, mr);
    return ::numkit::builtin::gammainc(out, ar, mr);
}

Value chi2inv(const Value &p, double k, std::pmr::memory_resource *mr)
{
    // MATLAB convention: k < 0 ⇒ NaN; k == 0 ⇒ degenerate, quantile is 0
    // for any p in [0, 1] (out-of-range p still NaN).
    if (k < 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    if (k == 0.0)
        return elementwise(p, [](double pi){
            return (pi >= 0.0 && pi <= 1.0) ? 0.0
                                            : std::numeric_limits<double>::quiet_NaN();
        }, mr);
    Value ar = Value::scalar(0.5 * k, mr);
    Value q = ::numkit::builtin::gammaincinv(p, ar, mr);
    // x = 2 * gammaincinv(p, k/2)
    return elementwise(q, [](double v){ return 2.0 * v; }, mr);
}

Value chi2rnd(double k, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (k <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    // Sample from Gamma(k/2, 2) — std::gamma_distribution(shape, scale).
    std::gamma_distribution<double> gd(0.5 * k, 2.0);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) od[i] = gd(gen);
    return out;
}

std::tuple<double, double> chi2stat(double k)
{
    // MATLAB convention: k <= 0 ⇒ moments NaN (degenerate distribution).
    if (k <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(k, 2.0 * k);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void chi2pdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("chi2pdf: requires (x, k)", 0, 0, "chi2pdf", "", "numkit:chi2pdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &k = args[1];
    // Scalar k: hoisted fast path (unchanged). Vector k: broadcast.
    if (k.isScalar())
        outs[0] = chi2pdf(args[0], k.toScalar(), mr);
    else
        outs[0] = broadcast_dist2(args[0], k, mr, "chi2pdf", chi2pdfK);
}

void chi2cdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    if (a.size() < 2)
        throw Error("chi2cdf: requires (x, k[, 'upper'])", 0, 0, "chi2cdf", "", "numkit:chi2cdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &k = a[1];
    Value v;
    if (k.isScalar()) {
        v = chi2cdf(a[0], k.toScalar(), mr);
    } else {
        // F(x; k) = gammainc(max(0,x/2), k/2); gammainc broadcasts (xs, k/2)
        // and gammaincScalar gives NaN where k/2<=0 (matches k<=0 → NaN).
        Value xs = elementwise(a[0], [](double xi) { return std::max(0.0, 0.5 * xi); }, mr);
        Value half_k = elementwise(k, [](double ki) { return 0.5 * ki; }, mr);
        v = ::numkit::builtin::gammainc(xs, half_k, mr);
    }
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void chi2inv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("chi2inv: requires (p, k)", 0, 0, "chi2inv", "", "numkit:chi2inv:nargin");
    outs[0] = chi2inv(args[0], args[1].toScalar(), ctx.engine->resource());
}

void chi2rnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("chi2rnd: requires k[, sz...]", 0, 0, "chi2rnd", "", "numkit:chi2rnd:nargin");
    const double k = args[0].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 1, rows, cols);
    outs[0] = chi2rnd(k, rows, cols, ctx.engine->resource());
}

void chi2stat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx, "chi2stat",
                       [](double k) { return chi2stat(k); });
}

} // namespace detail
} // namespace numkit::stats
