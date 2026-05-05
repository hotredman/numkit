// libs/stats/src/fit/fit.cpp
//
// Distribution MLE fitters with confidence intervals.

#include <numkit/stats/fit/fit.hpp>

#include <numkit/stats/distributions/chi2.hpp>
#include <numkit/stats/distributions/students_t.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace numkit::stats {

namespace {

void mean_var(const Value &x, double &mean, double &var, size_t &N) {
    N = x.numel();
    if (N == 0) { mean = 0.0; var = 0.0; return; }
    double s = 0.0;
    for (size_t i = 0; i < N; ++i) s += x.elemAsDouble(i);
    mean = s / double(N);
    if (N < 2) { var = 0.0; return; }
    double sq = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double d = x.elemAsDouble(i) - mean;
        sq += d * d;
    }
    var = sq / double(N - 1);
}

Value rowCI(std::pmr::memory_resource *mr, double lo, double hi) {
    Value v = Value::matrix(2, 1, ValueType::DOUBLE, mr);
    double *d = v.doubleDataMut();
    d[0] = lo;
    d[1] = hi;
    return v;
}

double tinv_scalar(std::pmr::memory_resource *mr, double p, double nu) {
    Value pv = Value::scalar(p, mr);
    return tinv(mr, pv, nu).toScalar();
}

double chi2inv_scalar(std::pmr::memory_resource *mr, double p, double k) {
    Value pv = Value::scalar(p, mr);
    return chi2inv(mr, pv, k).toScalar();
}

} // anonymous

std::tuple<Value, Value, Value, Value>
normfit(std::pmr::memory_resource *mr, const Value &x, double alpha)
{
    double mean = 0, var = 0;
    size_t N = 0;
    mean_var(x, mean, var, N);
    const double sd = std::sqrt(var);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (N < 2) {
        return {Value::scalar(N == 1 ? mean : nan, mr),
                Value::scalar(N == 1 ? 0.0 : nan, mr),
                rowCI(mr, nan, nan), rowCI(mr, nan, nan)};
    }
    const double t = tinv_scalar(mr, 1.0 - alpha / 2.0, double(N - 1));
    const double sem = sd / std::sqrt(double(N));
    const double mu_lo = mean - t * sem;
    const double mu_hi = mean + t * sem;
    // sigma CI from chi² on (N-1)*var.
    const double chiU = chi2inv_scalar(mr, 1.0 - alpha / 2.0, double(N - 1));
    const double chiL = chi2inv_scalar(mr,       alpha / 2.0, double(N - 1));
    const double s_lo = std::sqrt(double(N - 1) * var / chiU);
    const double s_hi = std::sqrt(double(N - 1) * var / chiL);
    return {Value::scalar(mean, mr),
            Value::scalar(sd,   mr),
            rowCI(mr, mu_lo, mu_hi),
            rowCI(mr, s_lo,  s_hi)};
}

std::tuple<Value, Value>
poissfit(std::pmr::memory_resource *mr, const Value &x, double alpha)
{
    const size_t N = x.numel();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (N == 0) return {Value::scalar(nan, mr), rowCI(mr, nan, nan)};
    double S = 0.0;
    for (size_t i = 0; i < N; ++i) S += x.elemAsDouble(i);
    const double lambda = S / double(N);
    // Exact CI via chi² inversion (Garwood).
    const double lo = (S == 0.0) ? 0.0
                                 : chi2inv_scalar(mr, alpha / 2.0,       2.0 * S)       / (2.0 * N);
    const double hi = chi2inv_scalar(mr, 1.0 - alpha / 2.0, 2.0 * (S + 1.0)) / (2.0 * N);
    return {Value::scalar(lambda, mr), rowCI(mr, lo, hi)};
}

std::tuple<Value, Value>
expfit(std::pmr::memory_resource *mr, const Value &x, double alpha)
{
    const size_t N = x.numel();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (N == 0) return {Value::scalar(nan, mr), rowCI(mr, nan, nan)};
    double S = 0.0;
    for (size_t i = 0; i < N; ++i) S += x.elemAsDouble(i);
    const double mu = S / double(N);
    // Exact CI: 2·N·muhat ~ μ·χ²(2N).
    const double chiU = chi2inv_scalar(mr, 1.0 - alpha / 2.0, 2.0 * double(N));
    const double chiL = chi2inv_scalar(mr,       alpha / 2.0, 2.0 * double(N));
    const double lo = 2.0 * double(N) * mu / chiU;
    const double hi = 2.0 * double(N) * mu / chiL;
    return {Value::scalar(mu, mr), rowCI(mr, lo, hi)};
}

std::tuple<Value, Value, Value, Value>
unifit(std::pmr::memory_resource *mr, const Value &x, double alpha)
{
    const size_t N = x.numel();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (N == 0) {
        return {Value::scalar(nan, mr), Value::scalar(nan, mr),
                rowCI(mr, nan, nan), rowCI(mr, nan, nan)};
    }
    double mn = x.elemAsDouble(0), mx = mn;
    for (size_t i = 1; i < N; ++i) {
        const double v = x.elemAsDouble(i);
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    const double range = mx - mn;
    const double delta = range * (std::pow(alpha, -1.0 / double(N)) - 1.0);
    return {Value::scalar(mn, mr),
            Value::scalar(mx, mr),
            rowCI(mr, mn - delta, mn),
            rowCI(mr, mx, mx + delta)};
}

// ── Negative log-likelihoods ──────────────────────────────────────────

namespace {
constexpr double kLog2Pi = 1.8378770664093454835606594728112352;
}

double normlike(std::pmr::memory_resource * /*mr*/, double mu, double sigma,
                const Value &x)
{
    const size_t N = x.numel();
    if (N == 0 || sigma <= 0.0) return std::numeric_limits<double>::infinity();
    double ss = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double d = x.elemAsDouble(i) - mu;
        ss += d * d;
    }
    return double(N) * std::log(sigma) + 0.5 * double(N) * kLog2Pi
         + ss / (2.0 * sigma * sigma);
}

double explike(std::pmr::memory_resource * /*mr*/, double mu, const Value &x)
{
    const size_t N = x.numel();
    if (N == 0 || mu <= 0.0) return std::numeric_limits<double>::infinity();
    double sx = 0.0;
    for (size_t i = 0; i < N; ++i) sx += x.elemAsDouble(i);
    return double(N) * std::log(mu) + sx / mu;
}

double lognlike(std::pmr::memory_resource * /*mr*/, double mu, double sigma,
                const Value &x)
{
    const size_t N = x.numel();
    if (N == 0 || sigma <= 0.0) return std::numeric_limits<double>::infinity();
    double sumLogX = 0.0, ss = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        if (xi <= 0.0) return std::numeric_limits<double>::infinity();
        const double lx = std::log(xi);
        sumLogX += lx;
        const double d = lx - mu;
        ss += d * d;
    }
    return sumLogX + double(N) * std::log(sigma)
         + 0.5 * double(N) * kLog2Pi + ss / (2.0 * sigma * sigma);
}

double gamlike(std::pmr::memory_resource * /*mr*/, double a, double b,
               const Value &x)
{
    const size_t N = x.numel();
    if (N == 0 || a <= 0.0 || b <= 0.0)
        return std::numeric_limits<double>::infinity();
    double sumLogX = 0.0, sx = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        if (xi <= 0.0) return std::numeric_limits<double>::infinity();
        sumLogX += std::log(xi);
        sx += xi;
    }
    return -(a - 1.0) * sumLogX + sx / b
         + double(N) * a * std::log(b) + double(N) * std::lgamma(a);
}

double betalike(std::pmr::memory_resource * /*mr*/, double a, double b,
                const Value &x)
{
    const size_t N = x.numel();
    if (N == 0 || a <= 0.0 || b <= 0.0)
        return std::numeric_limits<double>::infinity();
    double sumLog = 0.0, sumLog1m = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        if (xi <= 0.0 || xi >= 1.0)
            return std::numeric_limits<double>::infinity();
        sumLog   += std::log(xi);
        sumLog1m += std::log1p(-xi);
    }
    const double logBeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    return -(a - 1.0) * sumLog - (b - 1.0) * sumLog1m + double(N) * logBeta;
}

double wbllike(std::pmr::memory_resource * /*mr*/, double scale, double shape,
               const Value &x)
{
    const size_t N = x.numel();
    if (N == 0 || scale <= 0.0 || shape <= 0.0)
        return std::numeric_limits<double>::infinity();
    double sumLogX = 0.0, sumPow = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        if (xi <= 0.0) return std::numeric_limits<double>::infinity();
        sumLogX += std::log(xi);
        sumPow  += std::pow(xi / scale, shape);
    }
    return -double(N) * std::log(shape) + double(N) * shape * std::log(scale)
         - (shape - 1.0) * sumLogX + sumPow;
}

double evlike(std::pmr::memory_resource * /*mr*/, double mu, double sigma,
              const Value &x)
{
    const size_t N = x.numel();
    if (N == 0 || sigma <= 0.0) return std::numeric_limits<double>::infinity();
    double sLin = 0.0, sExp = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double t = (x.elemAsDouble(i) - mu) / sigma;
        sLin += t;
        sExp += std::exp(t);
    }
    return double(N) * std::log(sigma) - sLin + sExp;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

static double parse_alpha_arg(Span<const Value> args, size_t pos, double def) {
    if (pos >= args.size() || args[pos].isEmpty()) return def;
    return args[pos].toScalar();
}

void normfit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("normfit: requires X[, alpha]",
                    0, 0, "normfit", "", "m:normfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    auto [mu, sd, muci, sdci] = normfit(ctx.engine->resource(), args[0], alpha);
    outs[0] = std::move(mu);
    if (nargout > 1) outs[1] = std::move(sd);
    if (nargout > 2) outs[2] = std::move(muci);
    if (nargout > 3) outs[3] = std::move(sdci);
}

void poissfit_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("poissfit: requires X[, alpha]",
                    0, 0, "poissfit", "", "m:poissfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    auto [lam, ci] = poissfit(ctx.engine->resource(), args[0], alpha);
    outs[0] = std::move(lam);
    if (nargout > 1) outs[1] = std::move(ci);
}

void expfit_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("expfit: requires X[, alpha]",
                    0, 0, "expfit", "", "m:expfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    auto [mu, ci] = expfit(ctx.engine->resource(), args[0], alpha);
    outs[0] = std::move(mu);
    if (nargout > 1) outs[1] = std::move(ci);
}

void unifit_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("unifit: requires X[, alpha]",
                    0, 0, "unifit", "", "m:unifit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    auto [a, b, aci, bci] = unifit(ctx.engine->resource(), args[0], alpha);
    outs[0] = std::move(a);
    if (nargout > 1) outs[1] = std::move(b);
    if (nargout > 2) outs[2] = std::move(aci);
    if (nargout > 3) outs[3] = std::move(bci);
}

// ─── *like adapters ───────────────────────────────────────────────────

static void like2_reg(const char *fn,
                      double (*impl)(std::pmr::memory_resource *,
                                     double, double, const Value &),
                      Span<const Value> args, Span<Value> outs,
                      CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error(std::string(fn) + ": requires (params[2], data)",
                    0, 0, fn, "", "m:like:nargin");
    const double p0 = args[0].elemAsDouble(0);
    const double p1 = args[0].elemAsDouble(1);
    const double nL = impl(ctx.engine->resource(), p0, p1, args[1]);
    outs[0] = Value::scalar(nL, ctx.engine->resource());
}

void normlike_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{ like2_reg("normlike", &normlike, args, outs, ctx); }

void lognlike_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{ like2_reg("lognlike", &lognlike, args, outs, ctx); }

void gamlike_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{ like2_reg("gamlike", &gamlike, args, outs, ctx); }

void betalike_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{ like2_reg("betalike", &betalike, args, outs, ctx); }

void wbllike_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{ like2_reg("wbllike", &wbllike, args, outs, ctx); }

void evlike_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{ like2_reg("evlike", &evlike, args, outs, ctx); }

void explike_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("explike: requires (mu, data)",
                    0, 0, "explike", "", "m:explike:nargin");
    const double mu = args[0].toScalar();
    const double nL = explike(ctx.engine->resource(), mu, args[1]);
    outs[0] = Value::scalar(nL, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
