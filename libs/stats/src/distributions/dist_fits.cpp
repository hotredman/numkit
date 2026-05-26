// libs/stats/src/distributions/dist_fits.cpp
//
// MLE fitters for continuous distributions:
//   gamfit — Gamma(a, b) shape + scale
//   wblfit — Weibull(scale, shape)

#include <numkit/stats/distributions/gamma_dist.hpp>
#include <numkit/stats/distributions/weibull.hpp>
#include <numkit/stats/distributions/beta.hpp>
#include <numkit/stats/distributions/negbin.hpp>

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

Value betafit(const Value &x, std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    const std::size_t n = xv.size();
    if (n < 2)
        throw Error("betafit: need at least 2 observations",
                    0, 0, "betafit", "", "m:betafit:tooFewObs");
    for (double v : xv) {
        if (!(v > 0.0 && v < 1.0))
            throw Error("betafit: all observations must be in (0, 1)",
                        0, 0, "betafit", "", "m:betafit:outOfRange");
    }
    double sumX = 0.0, sum1mX = 0.0, sumLogX = 0.0, sumLog1mX = 0.0;
    for (double v : xv) {
        sumX     += v;
        sum1mX   += (1.0 - v);
        sumLogX  += std::log(v);
        sumLog1mX += std::log(1.0 - v);
    }
    const double meanX = sumX / static_cast<double>(n);
    const double varX  = (sum1mX * sumX) / (static_cast<double>(n) * static_cast<double>(n));
    // MoM initial guess: a = μ·(μ(1-μ)/σ² - 1), b = (1-μ)·(...)
    double v0 = 0.0;
    for (double v : xv) {
        const double d = v - meanX;
        v0 += d * d;
    }
    v0 /= static_cast<double>(n);
    if (!(v0 > 0.0) || v0 >= meanX * (1.0 - meanX))
        v0 = meanX * (1.0 - meanX) * 0.5;   // safeguard for MoM
    const double common = meanX * (1.0 - meanX) / v0 - 1.0;
    double a = meanX * common;
    double b = (1.0 - meanX) * common;
    if (!(a > 0.0)) a = 1.0;
    if (!(b > 0.0)) b = 1.0;

    const double sLogX  = sumLogX  / static_cast<double>(n);
    const double sLog1X = sumLog1mX / static_cast<double>(n);

    // Newton on (a, b). 2x2 system with diagonal trigamma minus
    // ψ'(a+b) on both off-diagonal entries.
    for (int it = 0; it < 100; ++it) {
        const double psiAB = digamma(a + b);
        const double f1 = digamma(a) - psiAB - sLogX;
        const double f2 = digamma(b) - psiAB - sLog1X;
        const double tAB = trigamma(a + b);
        const double J11 = trigamma(a) - tAB;
        const double J22 = trigamma(b) - tAB;
        const double J12 = -tAB;
        const double det = J11 * J22 - J12 * J12;
        if (std::fabs(det) < 1e-300) break;
        const double da = (J22 * f1 - J12 * f2) / det;
        const double db = (J11 * f2 - J12 * f1) / det;
        const double newA = a - da;
        const double newB = b - db;
        if (newA > 0.0) a = newA; else a *= 0.5;
        if (newB > 0.0) b = newB; else b *= 0.5;
        if (std::fabs(da) + std::fabs(db)
            < 1e-10 * (std::fabs(a) + std::fabs(b) + 1.0)) break;
    }

    auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    out.doubleDataMut()[0] = a;
    out.doubleDataMut()[1] = b;
    return out;
}

Value nbinfit(const Value &x, std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    const std::size_t n = xv.size();
    if (n < 2)
        throw Error("nbinfit: need at least 2 observations",
                    0, 0, "nbinfit", "", "m:nbinfit:tooFewObs");
    for (double v : xv) {
        if (v < 0.0 || std::fabs(v - std::round(v)) > 1e-9)
            throw Error("nbinfit: observations must be non-negative integers",
                        0, 0, "nbinfit", "", "m:nbinfit:notInteger");
    }
    double sum = 0.0;
    for (double v : xv) sum += v;
    const double meanX = sum / static_cast<double>(n);
    double v2 = 0.0;
    for (double v : xv) {
        const double d = v - meanX;
        v2 += d * d;
    }
    const double varX = v2 / static_cast<double>(n - 1);
    if (varX <= meanX)
        throw Error("nbinfit: sample is under-dispersed (var ≤ mean); "
                    "MLE undefined for negative binomial",
                    0, 0, "nbinfit", "", "m:nbinfit:underDispersed");
    // MoM: r = μ² / (σ² - μ), p = μ / σ².
    double r = (meanX * meanX) / (varX - meanX);
    if (!(r > 0.0)) r = 1.0;

    // Newton on the profile log-likelihood in r (p = r/(r + meanX)).
    //   d/dr log L = sum [ψ(x_i + r) - ψ(r)] + n [log(r/(r + meanX))]
    for (int it = 0; it < 100; ++it) {
        const double frac = r / (r + meanX);
        double S1 = 0.0, S2 = 0.0;
        for (double v : xv) {
            S1 += digamma(v + r) - digamma(r);
            S2 += trigamma(v + r) - trigamma(r);
        }
        const double f = S1 + static_cast<double>(n) * std::log(frac);
        const double fp = S2 + static_cast<double>(n) * meanX / (r * (r + meanX));
        if (std::fabs(fp) < 1e-300) break;
        const double step = f / fp;
        const double newR = r - step;
        if (newR > 0.0) r = newR;
        else            r *= 0.5;
        if (std::fabs(step) < 1e-10 * std::max(r, 1.0)) break;
    }
    const double p = r / (r + meanX);

    auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    out.doubleDataMut()[0] = r;
    out.doubleDataMut()[1] = p;
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

void betafit_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("betafit: requires (x)",
                    0, 0, "betafit", "", "m:betafit:nargin");
    outs[0] = betafit(args[0], ctx.engine->resource());
}

void nbinfit_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nbinfit: requires (x)",
                    0, 0, "nbinfit", "", "m:nbinfit:nargin");
    outs[0] = nbinfit(args[0], ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
