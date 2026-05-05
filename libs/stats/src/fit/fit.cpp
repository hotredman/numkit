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

} // namespace detail
} // namespace numkit::stats
