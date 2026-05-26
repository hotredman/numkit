// libs/stats/src/distributions/dist_fits.cpp
//
// MLE / moment fitters for continuous + count distributions:
//   gamfit  — Gamma(a, b) shape + scale  (Newton on profile)
//   wblfit  — Weibull(scale, shape)       (Newton on shape)
//   betafit — Beta(a, b)                  (2-D Newton on digamma system)
//   nbinfit — NegBin(r, p)                (Newton on profile)
//   evfit   — Type-I EV (Gumbel-min)      (1-D Newton, μ profiled)
//   gpfit   — Generalised Pareto          (PWM / Hosking-Wallis)

#include <numkit/stats/distributions/gamma_dist.hpp>
#include <numkit/stats/distributions/weibull.hpp>
#include <numkit/stats/distributions/beta.hpp>
#include <numkit/stats/distributions/negbin.hpp>
#include <numkit/stats/distributions/extreme_value.hpp>
#include <numkit/stats/distributions/gp.hpp>

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

Value evfit(const Value &x, std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    const std::size_t n = xv.size();
    if (n < 2)
        throw Error("evfit: need at least 2 observations",
                    0, 0, "evfit", "", "m:evfit:tooFewObs");
    for (double v : xv) {
        if (!std::isfinite(v))
            throw Error("evfit: observations must be finite",
                        0, 0, "evfit", "", "m:evfit:notFinite");
    }
    double sum = 0.0;
    for (double v : xv) sum += v;
    const double mean = sum / static_cast<double>(n);
    double var2 = 0.0;
    for (double v : xv) {
        const double d = v - mean;
        var2 += d * d;
    }
    var2 /= static_cast<double>(n);
    if (!(var2 > 0.0))
        throw Error("evfit: zero variance (data constant)",
                    0, 0, "evfit", "", "m:evfit:zeroVariance");

    // Initial guess: var = σ² · π²/6.
    double sigma = std::sqrt(6.0 * var2) / 3.141592653589793;
    if (!(sigma > 0.0) || !std::isfinite(sigma)) sigma = 1.0;

    double x_max = xv[0];
    for (double v : xv) if (v > x_max) x_max = v;

    // Newton on f(σ) = U/T - mean - σ = 0,
    // with U = Σ x_i e^{(x_i - x_max)/σ}, T = Σ e^{(x_i - x_max)/σ}.
    // f'(σ) = -Var_w(x)/σ² - 1   (Cauchy-Schwarz ⇒ always negative).
    double T = 0.0;
    for (int it = 0; it < 100; ++it) {
        T = 0.0; double U = 0.0, V = 0.0;
        for (double v : xv) {
            const double e = std::exp((v - x_max) / sigma);
            T += e;
            U += v * e;
            V += v * v * e;
        }
        const double meanW = U / T;
        const double varW  = V / T - meanW * meanW;       // ≥ 0
        const double f  = meanW - mean - sigma;
        const double fp = -varW / (sigma * sigma) - 1.0;
        if (!(std::fabs(fp) > 1e-300)) break;
        const double step = f / fp;
        const double newS = sigma - step;
        if (newS > 0.0) sigma = newS;
        else            sigma *= 0.5;
        if (std::fabs(step) < 1e-12 * std::max(sigma, 1.0)) break;
    }
    // Recompute T with final σ for μ.
    T = 0.0;
    for (double v : xv) T += std::exp((v - x_max) / sigma);
    const double mu = x_max + sigma * (std::log(T) - std::log(static_cast<double>(n)));

    auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    out.doubleDataMut()[0] = mu;
    out.doubleDataMut()[1] = sigma;
    return out;
}

// Generalised Pareto NLL helper. Returns +Inf if the support
// constraint (1 + k·x_i/σ > 0 for all i) is violated, allowing the
// optimiser to reject infeasible steps via the line-search.
static double gp_nll(double k, double sigma,
                     const std::vector<double> &xv)
{
    if (!(sigma > 0.0)) return std::numeric_limits<double>::infinity();
    const std::size_t n = xv.size();
    double nll = static_cast<double>(n) * std::log(sigma);
    if (std::fabs(k) < 1e-10) {
        // Exponential limit.
        double s = 0.0;
        for (double v : xv) s += v;
        return nll + s / sigma;
    }
    const double inv_sigma = 1.0 / sigma;
    for (double v : xv) {
        const double u = k * v * inv_sigma;
        if (u <= -1.0) return std::numeric_limits<double>::infinity();
    }
    double s = 0.0;
    for (double v : xv) s += std::log1p(k * v * inv_sigma);
    nll += (1.0 + 1.0 / k) * s;
    return nll;
}

// Central-finite-difference gradient of NLL w.r.t. (k, σ). Robust
// at k = 0 (where the analytical formula is removable but the limit
// requires a Taylor expansion); central FD captures the smooth surface
// uniformly across the parameter space.
static void gp_nll_grad(double k, double sigma,
                        const std::vector<double> &xv,
                        double &gk, double &gs)
{
    const double hk = std::max(std::fabs(k), 1.0) * 1e-7;
    const double hs = std::max(std::fabs(sigma), 1.0) * 1e-7;
    gk = (gp_nll(k + hk, sigma, xv) - gp_nll(k - hk, sigma, xv)) / (2.0 * hk);
    gs = (gp_nll(k, sigma + hs, xv) - gp_nll(k, sigma - hs, xv)) / (2.0 * hs);
}

Value gpfit(const Value &x, std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    const std::size_t n = xv.size();
    if (n < 2)
        throw Error("gpfit: need at least 2 observations",
                    0, 0, "gpfit", "", "m:gpfit:tooFewObs");
    for (double v : xv) {
        if (!std::isfinite(v) || v < 0.0)
            throw Error("gpfit: observations must be finite and ≥ 0 "
                        "(threshold θ = 0 assumed)",
                        0, 0, "gpfit", "", "m:gpfit:invalidData");
    }

    // ── Initial guess: PWM (Hosking-Wallis 1987, α-form) ──────────
    std::vector<double> xs = xv;
    std::sort(xs.begin(), xs.end());
    double sumX = 0.0;
    for (double v : xs) sumX += v;
    const double beta0 = sumX / static_cast<double>(n);
    double beta1 = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double F = (static_cast<double>(i + 1) - 0.35) / static_cast<double>(n);
        beta1 += (1.0 - F) * xs[i];
    }
    beta1 /= static_cast<double>(n);

    const double denom = beta0 - 2.0 * beta1;
    double k_hat, sigma_hat;
    if (std::fabs(denom) < 1e-300 || !std::isfinite(denom)) {
        k_hat = 0.0;
        sigma_hat = beta0;
    } else {
        k_hat = 2.0 - beta0 / denom;
        sigma_hat = 2.0 * beta0 * beta1 / denom;
        if (!(sigma_hat > 0.0) || !std::isfinite(sigma_hat)) {
            k_hat = 0.0;
            sigma_hat = beta0;
        }
    }

    // ── Newton-Raphson refinement (Grimshaw-style) ────────────────
    // 2-D Newton on (k, σ) with analytical gradient and central-FD
    // Hessian on the gradient. Line search guards against infeasible
    // or NLL-worsening steps.
    const double x_max = xs.back();
    double f0 = gp_nll(k_hat, sigma_hat, xv);
    if (!std::isfinite(f0)) {
        // PWM produced infeasible point — fall back to safe init.
        sigma_hat = beta0;
        k_hat = 0.0;
        f0 = gp_nll(k_hat, sigma_hat, xv);
    }

    for (int it = 0; it < 100; ++it) {
        double gk, gs;
        gp_nll_grad(k_hat, sigma_hat, xv, gk, gs);
        const double gnorm = std::sqrt(gk * gk + gs * gs);
        if (gnorm < 1e-12) break;

        // Numerical Hessian via central FD on gradient.
        const double hk = std::max(std::fabs(k_hat), 1.0) * 1e-6;
        const double hs = std::max(std::fabs(sigma_hat), 1.0) * 1e-6;
        double gk_kp, gs_kp, gk_km, gs_km, gk_sp, gs_sp, gk_sm, gs_sm;
        gp_nll_grad(k_hat + hk, sigma_hat, xv, gk_kp, gs_kp);
        gp_nll_grad(k_hat - hk, sigma_hat, xv, gk_km, gs_km);
        gp_nll_grad(k_hat, sigma_hat + hs, xv, gk_sp, gs_sp);
        gp_nll_grad(k_hat, sigma_hat - hs, xv, gk_sm, gs_sm);
        const double Hkk = (gk_kp - gk_km) / (2.0 * hk);
        const double Hss = (gs_sp - gs_sm) / (2.0 * hs);
        const double Hks = 0.5 * ((gk_sp - gk_sm) / (2.0 * hs)
                                + (gs_kp - gs_km) / (2.0 * hk));

        // Solve H · [dk; dσ] = -[gk; gs].
        const double det = Hkk * Hss - Hks * Hks;
        if (std::fabs(det) < 1e-300) break;
        double dk = (-gk * Hss + gs * Hks) / det;
        double ds = (-gs * Hkk + gk * Hks) / det;

        // Backtracking line search with feasibility guard.
        double step = 1.0;
        bool improved = false;
        for (int bt = 0; bt < 30; ++bt) {
            const double k_new = k_hat + step * dk;
            const double s_new = sigma_hat + step * ds;
            if (s_new > 0.0 && (k_new >= 0.0
                || -k_new * x_max / s_new < 1.0 - 1e-12)) {
                const double f_new = gp_nll(k_new, s_new, xv);
                if (std::isfinite(f_new) && f_new < f0 - 1e-15) {
                    k_hat = k_new;
                    sigma_hat = s_new;
                    f0 = f_new;
                    improved = true;
                    break;
                }
            }
            step *= 0.5;
        }
        if (!improved) break;
        if (std::fabs(step * dk) + std::fabs(step * ds)
            < 1e-12 * (std::fabs(k_hat) + std::fabs(sigma_hat) + 1.0)) break;
    }

    auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    out.doubleDataMut()[0] = k_hat;
    out.doubleDataMut()[1] = sigma_hat;
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

void evfit_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("evfit: requires (x)",
                    0, 0, "evfit", "", "m:evfit:nargin");
    outs[0] = evfit(args[0], ctx.engine->resource());
}

void gpfit_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gpfit: requires (x)",
                    0, 0, "gpfit", "", "m:gpfit:nargin");
    outs[0] = gpfit(args[0], ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
