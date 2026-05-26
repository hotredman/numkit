// libs/stats/src/distributions/dist_fits.cpp
//
// MLE fitters for continuous distributions:
//   gamfit — Gamma(a, b) shape + scale
//   wblfit — Weibull(scale, shape)

#include <numkit/stats/distributions/gamma_dist.hpp>
#include <numkit/stats/distributions/weibull.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace numkit::stats {

namespace {

// Local digamma ψ(x) via Bernoulli asymptotic + recurrence to x ≥ 6.
double digamma(double x)
{
    double s = 0.0;
    while (x < 6.0) {
        s -= 1.0 / x;
        x += 1.0;
    }
    const double x2 = 1.0 / (x * x);
    double r = std::log(x) - 0.5 / x;
    r -= x2 * (1.0 / 12.0
              - x2 * (1.0 / 120.0
                     - x2 * (1.0 / 252.0)));
    return r + s;
}

// Trigamma ψ'(x) via same recurrence path.
double trigamma(double x)
{
    double s = 0.0;
    while (x < 6.0) {
        s += 1.0 / (x * x);
        x += 1.0;
    }
    const double x2 = 1.0 / (x * x);
    double r = 1.0 / x + 0.5 / (x * x);
    r += (x2 / x) * (1.0 / 6.0
                    - x2 * (1.0 / 30.0
                           - x2 * (1.0 / 42.0)));
    return r + s;
}

std::vector<double> toFlat(const Value &x)
{
    std::vector<double> v(x.numel());
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = x.elemAsDouble(i);
    return v;
}

} // namespace

Value gamfit(const Value &x, std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    const std::size_t n = xv.size();
    if (n < 2)
        throw Error("gamfit: need at least 2 observations",
                    0, 0, "gamfit", "", "m:gamfit:tooFewObs");
    for (double v : xv) {
        if (!(v > 0.0))
            throw Error("gamfit: all observations must be positive",
                        0, 0, "gamfit", "", "m:gamfit:notPositive");
    }

    double sum = 0.0, sumLog = 0.0;
    for (double v : xv) { sum += v; sumLog += std::log(v); }
    const double mean = sum / static_cast<double>(n);
    const double meanLog = sumLog / static_cast<double>(n);
    const double s = std::log(mean) - meanLog;  // ≥ 0 in general
    if (!(s > 0.0)) {
        // All values identical → shape undefined; return MoM fallback.
        auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        out.doubleDataMut()[0] = std::numeric_limits<double>::infinity();
        out.doubleDataMut()[1] = mean;
        return out;
    }

    // Minka 2002 initial guess.
    double a = (3.0 - s + std::sqrt((s - 3.0) * (s - 3.0) + 24.0 * s))
               / (12.0 * s);

    // Newton iteration on  f(a) = log a - ψ(a) - s.
    for (int it = 0; it < 50; ++it) {
        const double fa = std::log(a) - digamma(a) - s;
        const double fpa = 1.0 / a - trigamma(a);
        const double step = fa / fpa;
        const double newA = a - step;
        if (newA > 0.0) a = newA;
        else            a *= 0.5;       // safeguard against bad step
        if (std::fabs(step) < 1e-10 * std::max(a, 1.0)) break;
    }
    const double b = mean / a;

    auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    out.doubleDataMut()[0] = a;
    out.doubleDataMut()[1] = b;
    return out;
}

Value wblfit(const Value &x, std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    const std::size_t n = xv.size();
    if (n < 2)
        throw Error("wblfit: need at least 2 observations",
                    0, 0, "wblfit", "", "m:wblfit:tooFewObs");
    for (double v : xv) {
        if (!(v > 0.0))
            throw Error("wblfit: all observations must be positive",
                        0, 0, "wblfit", "", "m:wblfit:notPositive");
    }

    // Pre-compute log(x) once.
    std::vector<double> lx(n);
    double sumLog = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        lx[i] = std::log(xv[i]);
        sumLog += lx[i];
    }
    const double meanLog = sumLog / static_cast<double>(n);

    // Newton iteration for shape b. Implicit MLE equation:
    //   g(b) = 1/b + meanLog - Σ x^b · log x / Σ x^b = 0
    // Initial guess: b₀ = π / (std(log x) · sqrt(6)) (asymptotic).
    double v2 = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double d = lx[i] - meanLog;
        v2 += d * d;
    }
    const double sdLog = std::sqrt(v2 / static_cast<double>(n));
    double b = 3.141592653589793 / (sdLog * std::sqrt(6.0) + 1e-12);
    if (!(b > 0.1) || !std::isfinite(b)) b = 1.0;

    for (int it = 0; it < 100; ++it) {
        double S0 = 0.0, S1 = 0.0, S2 = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double w = std::pow(xv[i], b);     // x^b
            S0 += w;
            S1 += w * lx[i];
            S2 += w * lx[i] * lx[i];
        }
        const double R = S1 / S0;
        const double g = 1.0 / b + meanLog - R;
        // dR/db = (S2 · S0 - S1²) / S0²
        const double dR = (S2 * S0 - S1 * S1) / (S0 * S0);
        const double dg = -1.0 / (b * b) - dR;
        const double step = g / dg;
        const double newB = b - step;
        if (newB > 0.0) b = newB;
        else            b *= 0.5;
        if (std::fabs(step) < 1e-10 * std::max(b, 1.0)) break;
    }

    // Scale a from MLE: a = (Σ x^b / n)^{1/b}.
    double S = 0.0;
    for (double v : xv) S += std::pow(v, b);
    const double a = std::pow(S / static_cast<double>(n), 1.0 / b);

    auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    out.doubleDataMut()[0] = a;
    out.doubleDataMut()[1] = b;
    return out;
}

// ── Adapters ─────────────────────────────────────────────────────────
namespace detail {

void gamfit_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gamfit: requires (x)",
                    0, 0, "gamfit", "", "m:gamfit:nargin");
    outs[0] = gamfit(args[0], ctx.engine->resource());
}

void wblfit_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("wblfit: requires (x)",
                    0, 0, "wblfit", "", "m:wblfit:nargin");
    outs[0] = wblfit(args[0], ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
