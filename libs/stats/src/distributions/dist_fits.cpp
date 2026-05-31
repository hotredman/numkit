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
#include <numkit/stats/distributions/gev.hpp>

#include <numkit/builtin/math/special/special.hpp>   // gammainc (used by gamfit_full)

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

// ── Generic 2-D Wald CI helper for MLE fitters ──────────────────────
//
// Given an NLL functor f(t1, t2) and the MLE point (t1_hat, t2_hat),
// computes the 2×2 confidence matrix by:
//   1. Central-FD Hessian of NLL → observed Fisher information I.
//   2. V = I^{-1}; SE_lin_j = sqrt(V[j,j]).
//   3. Per-parameter Wald CI on the appropriate transformed scale
//      (LINEAR / LOG / LOGIT — MATLAB conventions), then inverse-
//      transform back. z_{α/2} from `norminv(1 - α/2)`.
//
// Returns a 2×2 matrix in MATLAB convention: [lo_row; hi_row] with
// parameters as columns.

enum class CITransform { LINEAR, LOG, LOGIT };

template <typename NllFn>
Value wald_ci_2d(NllFn &&nll, double t1, double t2,
                 CITransform tr1, CITransform tr2,
                 double alpha, std::pmr::memory_resource *mr)
{
    // FD step: eps^{1/3} ≈ 6e-6 is theoretically optimal for 2nd-deriv
    // central FD; relax to 1e-4 for distributions with curved score
    // functions (digamma/trigamma) to suppress roundoff noise.
    const double h1 = std::max(std::fabs(t1), 1.0) * 1e-4;
    const double h2 = std::max(std::fabs(t2), 1.0) * 1e-4;
    const double f00   = nll(t1, t2);
    const double f1p   = nll(t1 + h1, t2);
    const double f1m   = nll(t1 - h1, t2);
    const double f2p   = nll(t1, t2 + h2);
    const double f2m   = nll(t1, t2 - h2);
    const double f12pp = nll(t1 + h1, t2 + h2);
    const double f12mm = nll(t1 - h1, t2 - h2);
    const double f12pm = nll(t1 + h1, t2 - h2);
    const double f12mp = nll(t1 - h1, t2 + h2);
    const double H11 = (f1p - 2.0 * f00 + f1m) / (h1 * h1);
    const double H22 = (f2p - 2.0 * f00 + f2m) / (h2 * h2);
    const double H12 = (f12pp - f12pm - f12mp + f12mm) / (4.0 * h1 * h2);
    const double det = H11 * H22 - H12 * H12;
    Value out = Value::matrix(2, 2, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    if (std::fabs(det) < 1e-300 || H11 < 0.0 || H22 < 0.0) {
        for (int i = 0; i < 4; ++i) od[i] = std::numeric_limits<double>::quiet_NaN();
        return out;
    }
    const double V11 = H22 / det;
    const double V22 = H11 / det;
    const double se1 = std::sqrt(std::max(V11, 0.0));
    const double se2 = std::sqrt(std::max(V22, 0.0));

    // z_{α/2} from norminv(1 - α/2). Series via std::erf inversion is
    // unnecessary — use the closed-form via erfc^{-1} approximation:
    // For α = 0.05, z = 1.95996398454005423552.
    auto z_alpha = [](double a) -> double {
        // Wichura AS 241: high-accuracy norm-inv for p = 1 - a/2.
        const double p = 1.0 - 0.5 * a;
        const double q = p - 0.5;
        if (std::fabs(q) <= 0.425) {
            const double r = 0.180625 - q * q;
            return q * (((((((2.509080928730122727e3 * r
                + 3.343057558358812877e4) * r + 6.726577092700870006e4) * r
                + 4.592195393154987305e4) * r + 1.373169376550946215e4) * r
                + 1.971590950306227941e3) * r + 1.331416678917643998e2) * r
                + 3.387132872796366608) /
                (((((((5.226495278852854561e3 * r + 2.872908573572194738e4) * r
                + 3.930789580009271061e4) * r + 2.121379430158659746e4) * r
                + 5.394196021424751670e3) * r + 6.871159355626076150e2) * r
                + 4.231333070160091088e1) * r + 1.0);
        }
        double r = (q < 0.0) ? p : 1.0 - p;
        r = std::sqrt(-std::log(r));
        double v;
        if (r <= 5.0) {
            r -= 1.6;
            v = (((((((7.74545014278341407640e-4 * r
                + 2.27238449892691845833e-2) * r + 2.41780725177450611770e-1) * r
                + 1.27045825245236838258e0) * r + 3.64784832476320460504e0) * r
                + 5.76949722146069140550e0) * r + 4.63033784615654529590e0) * r
                + 1.42343711074968357734e0) /
                (((((((1.05075007164441684324e-9 * r
                + 5.47593808499534494600e-4) * r + 1.51986665636164571966e-2) * r
                + 1.48103976427480074590e-1) * r + 6.89767334985100004550e-1) * r
                + 1.67638483018380384940e0) * r + 2.05319162663775882187e0) * r
                + 1.0);
        } else {
            r -= 5.0;
            v = (((((((2.01033439929228813265e-7 * r
                + 2.71155556874348757815e-5) * r + 1.24266094738807843860e-3) * r
                + 2.65321895265761230930e-2) * r + 2.96560571828504891230e-1) * r
                + 1.78482653991729133580e0) * r + 5.46378491116411436990e0) * r
                + 6.65790464350110377720e0) /
                (((((((2.04426310338993978564e-15 * r
                + 1.42151175831644588870e-7) * r + 1.84631831751005468180e-5) * r
                + 7.86869131145613259100e-4) * r + 1.48753612908506148525e-2) * r
                + 1.36929880988350204800e-1) * r + 5.99832206555887937690e-1) * r
                + 1.0);
        }
        return (q < 0.0) ? -v : v;
    };
    const double z = z_alpha(alpha);

    auto apply_ci = [z](double t, double se, CITransform tr) -> std::pair<double, double> {
        switch (tr) {
            case CITransform::LINEAR:
                return {t - z * se, t + z * se};
            case CITransform::LOG: {
                if (!(t > 0.0))
                    return {std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::quiet_NaN()};
                const double se_log = se / t;
                return {t * std::exp(-z * se_log), t * std::exp(z * se_log)};
            }
            case CITransform::LOGIT: {
                if (!(t > 0.0 && t < 1.0))
                    return {std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::quiet_NaN()};
                const double se_logit = se / (t * (1.0 - t));
                const double logit = std::log(t / (1.0 - t));
                const double lo = logit - z * se_logit;
                const double hi = logit + z * se_logit;
                return {1.0 / (1.0 + std::exp(-lo)),
                        1.0 / (1.0 + std::exp(-hi))};
            }
        }
        return {t, t};
    };
    auto [lo1, hi1] = apply_ci(t1, se1, tr1);
    auto [lo2, hi2] = apply_ci(t2, se2, tr2);
    // 2×2 column-major: [lo1; hi1] [lo2; hi2]
    od[0] = lo1; od[1] = hi1; od[2] = lo2; od[3] = hi2;
    return out;
}

} // namespace

// Generalised Gamma NLL with right-censoring and frequency weights.
//   log f(x; a, b) = (a-1) log x - x/b - a log b - log Γ(a)
//   log S(x; a, b) = log(1 - gammainc(x/b, a))
// (gammainc(x, a) is the regularised LOWER incomplete gamma; survival
//  is the upper tail = 1 - lower.)
static double nll_gam_full(double a, double b,
                           const std::vector<double> &xv,
                           const std::vector<double> &cens,
                           const std::vector<double> &freq,
                           std::pmr::memory_resource *mr)
{
    if (!(a > 0.0) || !(b > 0.0)) return std::numeric_limits<double>::infinity();
    const std::size_t n = xv.size();
    double nuw = 0.0, lxw = 0.0, sxw = 0.0, S_cens = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double f = freq[i];
        const double u = 1.0 - cens[i];
        nuw += f * u;
        lxw += f * u * std::log(xv[i]);
        sxw += f * u * xv[i];
        if (cens[i] > 0.5 && f > 0.0) {
            Value xv_arg = Value::scalar(xv[i] / b, mr);
            Value av_arg = Value::scalar(a, mr);
            const double P = ::numkit::builtin::gammainc(xv_arg, av_arg, mr).toScalar();
            const double Q = 1.0 - P;
            if (!(Q > 0.0)) return std::numeric_limits<double>::infinity();
            S_cens -= f * std::log(Q);
        }
    }
    return nuw * (std::lgamma(a) + a * std::log(b))
         - (a - 1.0) * lxw + sxw / b + S_cens;
}

Value gamfit(const Value &x, std::pmr::memory_resource *mr)
{
    return gamfit(x, Value::Empty, Value::Empty, mr);
}

Value gamfit(const Value &x, const Value &censoring, const Value &freq,
             std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    const std::size_t n = xv.size();
    if (n < 2)
        throw Error("gamfit: need at least 2 observations",
                    0, 0, "gamfit", "", "numkit:gamfit:tooFewObs");
    for (double v : xv) {
        if (!(v > 0.0))
            throw Error("gamfit: all observations must be positive",
                        0, 0, "gamfit", "", "numkit:gamfit:notPositive");
    }
    std::vector<double> cens(n, 0.0), wf(n, 1.0);
    if (!censoring.isEmpty()) {
        if (censoring.numel() != n)
            throw Error("gamfit: censoring length mismatch",
                        0, 0, "gamfit", "", "numkit:gamfit:censLen");
        for (std::size_t i = 0; i < n; ++i)
            cens[i] = censoring.elemAsDouble(i) ? 1.0 : 0.0;
    }
    if (!freq.isEmpty()) {
        if (freq.numel() != n)
            throw Error("gamfit: freq length mismatch",
                        0, 0, "gamfit", "", "numkit:gamfit:freqLen");
        for (std::size_t i = 0; i < n; ++i) {
            wf[i] = freq.elemAsDouble(i);
            if (!(wf[i] >= 0.0))
                throw Error("gamfit: freq must be non-negative",
                            0, 0, "gamfit", "", "numkit:gamfit:freqNeg");
        }
    }

    bool freq_trivial = true;
    for (std::size_t i = 0; i < n; ++i)
        if (wf[i] != 1.0) { freq_trivial = false; break; }
    bool has_cens = false;
    for (std::size_t i = 0; i < n; ++i)
        if (cens[i] != 0.0) { has_cens = true; break; }

    if (!has_cens && freq_trivial) {
        // Existing closed-form path.
        double sum = 0.0, sumLog = 0.0;
        for (double v : xv) { sum += v; sumLog += std::log(v); }
        const double mean = sum / static_cast<double>(n);
        const double meanLog = sumLog / static_cast<double>(n);
        const double s = std::log(mean) - meanLog;
        if (!(s > 0.0)) {
            auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
            out.doubleDataMut()[0] = std::numeric_limits<double>::infinity();
            out.doubleDataMut()[1] = mean;
            return out;
        }
        double a = (3.0 - s + std::sqrt((s - 3.0) * (s - 3.0) + 24.0 * s))
                   / (12.0 * s);
        for (int it = 0; it < 50; ++it) {
            const double fa = std::log(a) - digamma(a) - s;
            const double fpa = 1.0 / a - trigamma(a);
            const double step = fa / fpa;
            const double newA = a - step;
            if (newA > 0.0) a = newA; else a *= 0.5;
            if (std::fabs(step) < 1e-10 * std::max(a, 1.0)) break;
        }
        const double b = mean / a;
        auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        out.doubleDataMut()[0] = a;
        out.doubleDataMut()[1] = b;
        return out;
    }

    // Weighted/censored path: 2-D Newton on (a, b) via FD on
    // nll_gam_full. Initial guess from weighted uncensored moments.
    double nuw = 0.0, sx = 0.0, slx = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double u = 1.0 - cens[i];
        nuw += wf[i] * u;
        sx  += wf[i] * u * xv[i];
        slx += wf[i] * u * std::log(xv[i]);
    }
    if (!(nuw >= 2.0))
        throw Error("gamfit: need at least 2 uncensored observations",
                    0, 0, "gamfit", "", "numkit:gamfit:tooFewUncens");
    const double mean_uw = sx / nuw;
    const double meanLog_uw = slx / nuw;
    const double s_init = std::log(mean_uw) - meanLog_uw;
    double a, b;
    if (s_init > 0.0) {
        a = (3.0 - s_init + std::sqrt((s_init - 3.0) * (s_init - 3.0) + 24.0 * s_init))
            / (12.0 * s_init);
        b = mean_uw / a;
    } else {
        a = 1.0;
        b = mean_uw;
    }

    auto nll = [&](double aa, double bb) { return nll_gam_full(aa, bb, xv, cens, wf, mr); };
    double f_cur = nll(a, b);
    for (int it = 0; it < 100; ++it) {
        const double ha = std::max(std::fabs(a), 1.0) * 1e-5;
        const double hb = std::max(std::fabs(b), 1.0) * 1e-5;
        const double ga = (nll(a + ha, b) - nll(a - ha, b)) / (2.0 * ha);
        const double gb = (nll(a, b + hb) - nll(a, b - hb)) / (2.0 * hb);
        if (std::sqrt(ga * ga + gb * gb) < 1e-12) break;
        const double Haa = (nll(a + ha, b) - 2.0 * f_cur + nll(a - ha, b)) / (ha * ha);
        const double Hbb = (nll(a, b + hb) - 2.0 * f_cur + nll(a, b - hb)) / (hb * hb);
        const double Hab = (nll(a + ha, b + hb) - nll(a + ha, b - hb)
                         - nll(a - ha, b + hb) + nll(a - ha, b - hb))
                          / (4.0 * ha * hb);
        const double det = Haa * Hbb - Hab * Hab;
        if (std::fabs(det) < 1e-300) break;
        const double da = (-ga * Hbb + gb * Hab) / det;
        const double db = (-gb * Haa + ga * Hab) / det;
        double sc = 1.0;
        bool ok = false;
        for (int bt = 0; bt < 30; ++bt) {
            const double an = a + sc * da;
            const double bn = b + sc * db;
            if (an > 0.0 && bn > 0.0) {
                const double f_new = nll(an, bn);
                if (std::isfinite(f_new) && f_new < f_cur - 1e-15) {
                    a = an; b = bn; f_cur = f_new; ok = true;
                    break;
                }
            }
            sc *= 0.5;
        }
        if (!ok) break;
        if (std::fabs(sc * da) + std::fabs(sc * db)
            < 1e-12 * (std::fabs(a) + std::fabs(b) + 1.0)) break;
    }

    auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    out.doubleDataMut()[0] = a;
    out.doubleDataMut()[1] = b;
    return out;
}

// Generalised Weibull NLL with right-censoring and frequency weights.
//   log f(x; a, b) = log b - b log a + (b - 1) log x - (x/a)^b
//   log S(x; a, b) = -(x/a)^b
// Weighted-censored NLL:
//   Σ f_i u_i · [-log b + b log a - (b-1) log x_i] + Σ f_i (x_i/a)^b
static double nll_wbl_full(double a, double b,
                           const std::vector<double> &xv,
                           const std::vector<double> &cens,
                           const std::vector<double> &freq)
{
    if (!(a > 0.0) || !(b > 0.0)) return std::numeric_limits<double>::infinity();
    const std::size_t n = xv.size();
    double nuw = 0.0, lxw = 0.0, sumW = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double f = freq[i];
        const double u = 1.0 - cens[i];
        nuw  += f * u;
        lxw  += f * u * std::log(xv[i]);
        sumW += f * std::pow(xv[i] / a, b);
    }
    return nuw * (-std::log(b) + b * std::log(a)) - (b - 1.0) * lxw + sumW;
}

Value wblfit(const Value &x, const Value &censoring, const Value &freq,
             std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    const std::size_t n = xv.size();
    if (n < 2)
        throw Error("wblfit: need at least 2 observations",
                    0, 0, "wblfit", "", "numkit:wblfit:tooFewObs");
    for (double v : xv) {
        if (!(v > 0.0))
            throw Error("wblfit: all observations must be positive",
                        0, 0, "wblfit", "", "numkit:wblfit:notPositive");
    }
    std::vector<double> cens(n, 0.0), wf(n, 1.0);
    if (!censoring.isEmpty()) {
        if (censoring.numel() != n)
            throw Error("wblfit: censoring length mismatch",
                        0, 0, "wblfit", "", "numkit:wblfit:censLen");
        for (std::size_t i = 0; i < n; ++i)
            cens[i] = censoring.elemAsDouble(i) ? 1.0 : 0.0;
    }
    if (!freq.isEmpty()) {
        if (freq.numel() != n)
            throw Error("wblfit: freq length mismatch",
                        0, 0, "wblfit", "", "numkit:wblfit:freqLen");
        for (std::size_t i = 0; i < n; ++i) {
            wf[i] = freq.elemAsDouble(i);
            if (!(wf[i] >= 0.0))
                throw Error("wblfit: freq must be non-negative",
                            0, 0, "wblfit", "", "numkit:wblfit:freqNeg");
        }
    }

    // Detect trivial case → existing closed-form path.
    bool freq_trivial = true;
    for (std::size_t i = 0; i < n; ++i)
        if (wf[i] != 1.0) { freq_trivial = false; break; }
    bool has_cens = false;
    for (std::size_t i = 0; i < n; ++i)
        if (cens[i] != 0.0) { has_cens = true; break; }

    if (!has_cens && freq_trivial) {
        // Fast path: existing Newton on shape via profile σ.
        std::vector<double> lx(n);
        double sumLog = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            lx[i] = std::log(xv[i]);
            sumLog += lx[i];
        }
        const double meanLog = sumLog / static_cast<double>(n);
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
                const double w = std::pow(xv[i], b);
                S0 += w;
                S1 += w * lx[i];
                S2 += w * lx[i] * lx[i];
            }
            const double R = S1 / S0;
            const double g = 1.0 / b + meanLog - R;
            const double dR = (S2 * S0 - S1 * S1) / (S0 * S0);
            const double dg = -1.0 / (b * b) - dR;
            const double step = g / dg;
            const double newB = b - step;
            if (newB > 0.0) b = newB; else b *= 0.5;
            if (std::fabs(step) < 1e-10 * std::max(b, 1.0)) break;
        }
        double S = 0.0;
        for (double v : xv) S += std::pow(v, b);
        const double a = std::pow(S / static_cast<double>(n), 1.0 / b);
        auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        out.doubleDataMut()[0] = a;
        out.doubleDataMut()[1] = b;
        return out;
    }

    // Weighted/censored path: profile a given b.
    // For each b: a(b) = (Σ f x^b / Σ f u)^{1/b}.
    // Then 1-D Newton on b via score d(NLL)/db = 0 evaluated with a(b).
    // Initial b from naïve closed form on uncensored subset.
    double nuw_init = 0.0;
    double sum_lx_uncens = 0.0;
    double sum_lx_uncens_w = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double u = 1.0 - cens[i];
        nuw_init += wf[i] * u;
        sum_lx_uncens_w += wf[i] * u * std::log(xv[i]);
    }
    const double meanLog_w = (nuw_init > 0.0) ? sum_lx_uncens_w / nuw_init : 0.0;
    double v2 = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double u = 1.0 - cens[i];
        const double d = std::log(xv[i]) - meanLog_w;
        v2 += wf[i] * u * d * d;
    }
    const double sdLog_w = (nuw_init > 0.0) ? std::sqrt(v2 / nuw_init) : 1.0;
    double bcur = 3.141592653589793 / (sdLog_w * std::sqrt(6.0) + 1e-12);
    if (!(bcur > 0.05) || !std::isfinite(bcur)) bcur = 1.0;

    auto profile_a = [&](double b) -> double {
        double S = 0.0, U = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            S += wf[i] * std::pow(xv[i], b);
            U += wf[i] * (1.0 - cens[i]);
        }
        if (!(U > 0.0)) return std::numeric_limits<double>::quiet_NaN();
        return std::pow(S / U, 1.0 / b);
    };

    // Score for b under cens+freq: solve
    //   ∂NLL/∂b = -nuw/b + Σ f u log x_i - log a · nuw
    //           + Σ f (x/a)^b · log(x/a) - log a · nuw_uncens correction
    // Easier: 1-D Newton via FD on `nll_wbl_full(profile_a(b), b)`.
    auto nll_at_b = [&](double b) -> double {
        const double a = profile_a(b);
        return nll_wbl_full(a, b, xv, cens, wf);
    };
    double f_cur = nll_at_b(bcur);
    for (int it = 0; it < 100; ++it) {
        const double h = std::max(std::fabs(bcur), 1.0) * 1e-6;
        const double g = (nll_at_b(bcur + h) - nll_at_b(bcur - h)) / (2.0 * h);
        const double H = (nll_at_b(bcur + h) - 2.0 * f_cur + nll_at_b(bcur - h))
                          / (h * h);
        if (std::fabs(H) < 1e-300) break;
        const double step = g / H;
        double bn = bcur - step;
        // Backtracking.
        double sc = 1.0;
        bool ok = false;
        for (int bt = 0; bt < 30; ++bt) {
            bn = bcur - sc * step;
            if (bn > 0.0) {
                const double f_new = nll_at_b(bn);
                if (std::isfinite(f_new) && f_new < f_cur - 1e-15) {
                    bcur = bn; f_cur = f_new; ok = true; break;
                }
            }
            sc *= 0.5;
        }
        if (!ok) break;
        if (std::fabs(sc * step) < 1e-12 * std::max(bcur, 1.0)) break;
    }
    const double a_final = profile_a(bcur);

    auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    out.doubleDataMut()[0] = a_final;
    out.doubleDataMut()[1] = bcur;
    return out;
}

Value wblfit(const Value &x, std::pmr::memory_resource *mr)
{
    return wblfit(x, Value::Empty, Value::Empty, mr);
}

Value betafit(const Value &x, std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    const std::size_t n = xv.size();
    if (n < 2)
        throw Error("betafit: need at least 2 observations",
                    0, 0, "betafit", "", "numkit:betafit:tooFewObs");
    for (double v : xv) {
        if (!(v > 0.0 && v < 1.0))
            throw Error("betafit: all observations must be in (0, 1)",
                        0, 0, "betafit", "", "numkit:betafit:outOfRange");
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
                    0, 0, "nbinfit", "", "numkit:nbinfit:tooFewObs");
    for (double v : xv) {
        if (v < 0.0 || std::fabs(v - std::round(v)) > 1e-9)
            throw Error("nbinfit: observations must be non-negative integers",
                        0, 0, "nbinfit", "", "numkit:nbinfit:notInteger");
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
                    0, 0, "nbinfit", "", "numkit:nbinfit:underDispersed");
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

// Generalised EV-min NLL with right-censoring and frequency weights.
// Censoring indicator: cens_i = 1 if x_i is right-censored.
// Weighted contribution per observation:
//   uncens: f_i · [log σ − t_i + exp(t_i)]
//   cens  : f_i · [exp(t_i)]
// where t_i = (x_i − μ)/σ. Combined:
//   NLL = (Σf_i (1−c_i)) · log σ − Σf_i (1−c_i) t_i + Σf_i exp(t_i).
static double nll_ev_full(double mu, double sigma,
                          const std::vector<double> &xv,
                          const std::vector<double> &cens,
                          const std::vector<double> &freq)
{
    if (!(sigma > 0.0)) return std::numeric_limits<double>::infinity();
    const std::size_t n = xv.size();
    // Shift by max(t_i) to keep exp from overflowing.
    double t_max = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < n; ++i) {
        const double t = (xv[i] - mu) / sigma;
        if (t > t_max) t_max = t;
    }
    double nuw = 0.0;        // Σ f_i (1 - c_i)
    double Suw = 0.0;        // Σ f_i (1 - c_i) t_i
    double Wexp = 0.0;       // Σ f_i · exp(t_i - t_max)
    for (std::size_t i = 0; i < n; ++i) {
        const double t = (xv[i] - mu) / sigma;
        const double f = freq[i];
        const double u = 1.0 - cens[i];
        nuw  += f * u;
        Suw  += f * u * t;
        Wexp += f * std::exp(t - t_max);
    }
    // Adding back the shift: Σ f exp(t) = exp(t_max) · Wexp.
    // log σ · nuw - Suw + exp(t_max) · Wexp.
    return nuw * std::log(sigma) - Suw + std::exp(t_max) * Wexp;
}

Value evfit(const Value &x, std::pmr::memory_resource *mr)
{
    return evfit(x, Value::Empty, Value::Empty, mr);
}

Value evfit(const Value &x, const Value &censoring, const Value &freq,
            std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    const std::size_t n = xv.size();
    if (n < 2)
        throw Error("evfit: need at least 2 observations",
                    0, 0, "evfit", "", "numkit:evfit:tooFewObs");
    for (double v : xv) {
        if (!std::isfinite(v))
            throw Error("evfit: observations must be finite",
                        0, 0, "evfit", "", "numkit:evfit:notFinite");
    }
    // Parse censoring (zeros if empty) and freq (ones if empty).
    std::vector<double> cens(n, 0.0), wf(n, 1.0);
    if (!censoring.isEmpty()) {
        if (censoring.numel() != n)
            throw Error("evfit: censoring must match data length",
                        0, 0, "evfit", "", "numkit:evfit:censLen");
        for (std::size_t i = 0; i < n; ++i)
            cens[i] = censoring.elemAsDouble(i) ? 1.0 : 0.0;
    }
    if (!freq.isEmpty()) {
        if (freq.numel() != n)
            throw Error("evfit: freq must match data length",
                        0, 0, "evfit", "", "numkit:evfit:freqLen");
        for (std::size_t i = 0; i < n; ++i) {
            wf[i] = freq.elemAsDouble(i);
            if (!(wf[i] >= 0.0))
                throw Error("evfit: freq must be non-negative",
                            0, 0, "evfit", "", "numkit:evfit:freqNeg");
        }
    }

    // Effective weights / counts.
    double sum_fw = 0.0, sum_uw = 0.0;
    double sum_fwx = 0.0, sum_uwx = 0.0;
    double sum_uwxx = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double f = wf[i];
        const double u = 1.0 - cens[i];
        sum_fw   += f;
        sum_uw   += f * u;
        sum_fwx  += f * xv[i];
        sum_uwx  += f * u * xv[i];
        sum_uwxx += f * u * xv[i] * xv[i];
    }
    if (sum_uw < 2.0)
        throw Error("evfit: need at least 2 uncensored observations",
                    0, 0, "evfit", "", "numkit:evfit:tooFewUncens");
    const double mean_uw = sum_uwx / sum_uw;
    double var_uw = sum_uwxx / sum_uw - mean_uw * mean_uw;
    // Constant uncensored data is degenerate (no info about σ).
    if (!(var_uw > 0.0)) {
        // Treat truly constant data (no freq weighting or all-equal
        // uncensored values) as an error to match MATLAB's behaviour
        // on degenerate input.
        bool freq_trivial = true;
        for (std::size_t i = 0; i < n; ++i)
            if (wf[i] != 1.0) { freq_trivial = false; break; }
        const bool has_cens_local = (sum_uw < sum_fw);
        if (!has_cens_local && freq_trivial)
            throw Error("evfit: zero variance (data constant)",
                        0, 0, "evfit", "", "numkit:evfit:zeroVariance");
        var_uw = 1.0;
    }

    // Initial guess: σ from weighted/uncensored variance moment.
    // For Gumbel-min, E[X] = μ - γ_E·σ ⇒ μ = E[X] + γ_E·σ.
    double sigma = std::sqrt(6.0 * var_uw) / 3.141592653589793;
    if (!(sigma > 0.0) || !std::isfinite(sigma)) sigma = 1.0;
    double mu = mean_uw + 0.57721566490153286 * sigma;

    // Check if censoring or non-trivial freq is present — if not,
    // dispatch to the existing fast closed-form path.
    const bool has_cens = (sum_uw < sum_fw);
    bool freq_trivial = true;
    for (std::size_t i = 0; i < n; ++i)
        if (wf[i] != 1.0) { freq_trivial = false; break; }

    if (!has_cens && freq_trivial) {
        // Closed-form (existing path): profile μ via log-sum-exp.
        double sum = 0.0;
        for (double v : xv) sum += v;
        const double mean = sum / static_cast<double>(n);
        double x_max = xv[0];
        for (double v : xv) if (v > x_max) x_max = v;
        double T = 0.0;
        for (int it = 0; it < 100; ++it) {
            T = 0.0; double U = 0.0, V = 0.0;
            for (double v : xv) {
                const double e = std::exp((v - x_max) / sigma);
                T += e;  U += v * e;  V += v * v * e;
            }
            const double meanW = U / T;
            const double varW  = V / T - meanW * meanW;
            const double f  = meanW - mean - sigma;
            const double fp = -varW / (sigma * sigma) - 1.0;
            if (!(std::fabs(fp) > 1e-300)) break;
            const double step = f / fp;
            const double newS = sigma - step;
            if (newS > 0.0) sigma = newS;
            else            sigma *= 0.5;
            if (std::fabs(step) < 1e-12 * std::max(sigma, 1.0)) break;
        }
        T = 0.0;
        for (double v : xv) T += std::exp((v - x_max) / sigma);
        mu = x_max + sigma * (std::log(T) - std::log(static_cast<double>(n)));
    } else {
        // 2-D Newton with FD gradient/Hessian.
        auto grad = [&](double m, double s, double &gm, double &gs) {
            const double hm = std::max(std::fabs(m), 1.0) * 1e-6;
            const double hs = std::max(std::fabs(s), 1.0) * 1e-6;
            gm = (nll_ev_full(m + hm, s, xv, cens, wf)
                - nll_ev_full(m - hm, s, xv, cens, wf)) / (2 * hm);
            gs = (nll_ev_full(m, s + hs, xv, cens, wf)
                - nll_ev_full(m, s - hs, xv, cens, wf)) / (2 * hs);
        };
        double f_cur = nll_ev_full(mu, sigma, xv, cens, wf);
        for (int it = 0; it < 100; ++it) {
            double gm, gs;
            grad(mu, sigma, gm, gs);
            if (std::sqrt(gm * gm + gs * gs) < 1e-12) break;
            // FD-Hessian on gradient.
            const double hm = std::max(std::fabs(mu), 1.0) * 1e-5;
            const double hs = std::max(std::fabs(sigma), 1.0) * 1e-5;
            double gm_mp, gs_mp, gm_mm, gs_mm, gm_sp, gs_sp, gm_sm, gs_sm;
            grad(mu + hm, sigma, gm_mp, gs_mp);
            grad(mu - hm, sigma, gm_mm, gs_mm);
            grad(mu, sigma + hs, gm_sp, gs_sp);
            grad(mu, sigma - hs, gm_sm, gs_sm);
            const double Hmm = (gm_mp - gm_mm) / (2 * hm);
            const double Hss = (gs_sp - gs_sm) / (2 * hs);
            const double Hms = 0.5 * ((gm_sp - gm_sm) / (2 * hs)
                                    + (gs_mp - gs_mm) / (2 * hm));
            const double det = Hmm * Hss - Hms * Hms;
            if (std::fabs(det) < 1e-300) break;
            const double dm = (-gm * Hss + gs * Hms) / det;
            const double ds = (-gs * Hmm + gm * Hms) / det;
            double step = 1.0;
            bool ok = false;
            for (int bt = 0; bt < 30; ++bt) {
                const double mu_new = mu + step * dm;
                const double s_new  = sigma + step * ds;
                if (s_new > 0.0) {
                    const double f_new = nll_ev_full(mu_new, s_new, xv, cens, wf);
                    if (std::isfinite(f_new) && f_new < f_cur - 1e-15) {
                        mu = mu_new;  sigma = s_new;  f_cur = f_new;
                        ok = true;
                        break;
                    }
                }
                step *= 0.5;
            }
            if (!ok) break;
            if (std::fabs(step * dm) + std::fabs(step * ds)
                < 1e-12 * (std::fabs(mu) + std::fabs(sigma) + 1.0)) break;
        }
    }

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
                    0, 0, "gpfit", "", "numkit:gpfit:tooFewObs");
    for (double v : xv) {
        if (!std::isfinite(v) || v < 0.0)
            throw Error("gpfit: observations must be finite and ≥ 0 "
                        "(threshold θ = 0 assumed)",
                        0, 0, "gpfit", "", "numkit:gpfit:invalidData");
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

// ════════════════════════════════════════════════════════════════════
// Confidence intervals (Wald, observed Fisher information)
// ════════════════════════════════════════════════════════════════════

namespace {

// NLL definitions for each fit. Used by FD-Hessian Wald CI.

double nll_gam(double a, double b, const std::vector<double> &xv)
{
    if (!(a > 0.0) || !(b > 0.0)) return std::numeric_limits<double>::infinity();
    const double N = static_cast<double>(xv.size());
    double sx = 0.0, slx = 0.0;
    for (double v : xv) { sx += v; slx += std::log(v); }
    return N * std::lgamma(a) + N * a * std::log(b)
         - (a - 1.0) * slx + sx / b;
}

double nll_wbl(double a, double b, const std::vector<double> &xv)
{
    if (!(a > 0.0) || !(b > 0.0)) return std::numeric_limits<double>::infinity();
    const double N = static_cast<double>(xv.size());
    double slx = 0.0, sw = 0.0;
    for (double v : xv) {
        slx += std::log(v);
        sw  += std::pow(v / a, b);
    }
    return -N * std::log(b) + N * b * std::log(a)
         - (b - 1.0) * slx + sw;
}

double nll_beta(double a, double b, const std::vector<double> &xv)
{
    if (!(a > 0.0) || !(b > 0.0)) return std::numeric_limits<double>::infinity();
    const double N = static_cast<double>(xv.size());
    double slx = 0.0, sl1mx = 0.0;
    for (double v : xv) {
        slx   += std::log(v);
        sl1mx += std::log(1.0 - v);
    }
    return N * (std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b))
         - (a - 1.0) * slx - (b - 1.0) * sl1mx;
}

double nll_nbin(double r, double p, const std::vector<double> &xv)
{
    if (!(r > 0.0) || !(p > 0.0) || !(p < 1.0))
        return std::numeric_limits<double>::infinity();
    const double N = static_cast<double>(xv.size());
    double term = 0.0;
    double sx = 0.0;
    for (double v : xv) {
        term -= std::lgamma(v + r) - std::lgamma(r) - std::lgamma(v + 1.0);
        sx   += v;
    }
    return term - N * r * std::log(p) - sx * std::log(1.0 - p);
}

double nll_ev(double mu, double sigma, const std::vector<double> &xv)
{
    if (!(sigma > 0.0)) return std::numeric_limits<double>::infinity();
    const double N = static_cast<double>(xv.size());
    // log f(x) = -log σ + t - exp(t), t = (x-μ)/σ. NLL is sum of -log f.
    double st = 0.0, set = 0.0;
    for (double v : xv) {
        const double t = (v - mu) / sigma;
        st  += t;
        set += std::exp(t);
    }
    return N * std::log(sigma) - st + set;
}

// gpfit NLL is already defined as gp_nll above.

} // anonymous

Value gamfit_ci(const Value &x, double alpha, std::pmr::memory_resource *mr)
{
    return gamfit_ci(x, alpha, Value::Empty, Value::Empty, mr);
}

Value gamfit_ci(const Value &x, double alpha,
                const Value &censoring, const Value &freq,
                std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    const std::size_t n = xv.size();
    std::vector<double> cens(n, 0.0), wf(n, 1.0);
    if (!censoring.isEmpty()) {
        for (std::size_t i = 0; i < n; ++i)
            cens[i] = censoring.elemAsDouble(i) ? 1.0 : 0.0;
    }
    if (!freq.isEmpty()) {
        for (std::size_t i = 0; i < n; ++i) wf[i] = freq.elemAsDouble(i);
    }
    Value parm = gamfit(x, censoring, freq, mr);
    const double a_hat = parm.elemAsDouble(0);
    const double b_hat = parm.elemAsDouble(1);
    bool trivial = censoring.isEmpty() && freq.isEmpty();
    if (trivial) {
        return wald_ci_2d([&](double a, double b) { return nll_gam(a, b, xv); },
                          a_hat, b_hat, CITransform::LOG, CITransform::LOG,
                          alpha, mr);
    }
    return wald_ci_2d([&](double a, double b) {
                          return nll_gam_full(a, b, xv, cens, wf, mr);
                      },
                      a_hat, b_hat, CITransform::LOG, CITransform::LOG,
                      alpha, mr);
}

Value wblfit_ci(const Value &x, double alpha, std::pmr::memory_resource *mr)
{
    return wblfit_ci(x, alpha, Value::Empty, Value::Empty, mr);
}

Value wblfit_ci(const Value &x, double alpha,
                const Value &censoring, const Value &freq,
                std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    const std::size_t n = xv.size();
    std::vector<double> cens(n, 0.0), wf(n, 1.0);
    if (!censoring.isEmpty()) {
        for (std::size_t i = 0; i < n; ++i)
            cens[i] = censoring.elemAsDouble(i) ? 1.0 : 0.0;
    }
    if (!freq.isEmpty()) {
        for (std::size_t i = 0; i < n; ++i) wf[i] = freq.elemAsDouble(i);
    }
    Value parm = wblfit(x, censoring, freq, mr);
    const double a_hat = parm.elemAsDouble(0);
    const double b_hat = parm.elemAsDouble(1);
    // For trivial case use the simple NLL; for cens/freq use the
    // generalised NLL.
    bool trivial = censoring.isEmpty() && freq.isEmpty();
    if (trivial) {
        return wald_ci_2d([&](double a, double b) { return nll_wbl(a, b, xv); },
                          a_hat, b_hat, CITransform::LOG, CITransform::LOG,
                          alpha, mr);
    }
    return wald_ci_2d([&](double a, double b) {
                          return nll_wbl_full(a, b, xv, cens, wf);
                      },
                      a_hat, b_hat, CITransform::LOG, CITransform::LOG,
                      alpha, mr);
}

// Specialised CI using analytical Hessian (digamma-based NLLs).
// Generic FD-Hessian introduces ~1% noise on these likelihoods, so we
// drop down to closed-form to keep CI bit-equal with MATLAB.

namespace {

// Build 2×2 Wald CI from analytical Fisher information (H = Hessian
// of NLL at MLE).
Value wald_ci_from_hessian(double t1, double t2,
                           double H11, double H22, double H12,
                           CITransform tr1, CITransform tr2,
                           double alpha, std::pmr::memory_resource *mr)
{
    Value out = Value::matrix(2, 2, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const double det = H11 * H22 - H12 * H12;
    if (std::fabs(det) < 1e-300 || H11 < 0.0 || H22 < 0.0) {
        for (int i = 0; i < 4; ++i) od[i] = std::numeric_limits<double>::quiet_NaN();
        return out;
    }
    const double V11 = H22 / det;
    const double V22 = H11 / det;
    const double se1 = std::sqrt(std::max(V11, 0.0));
    const double se2 = std::sqrt(std::max(V22, 0.0));
    // z_{α/2} — at default α=0.05 returns 1.959963984540054.
    const double z = [](double a) -> double {
        const double p = 1.0 - 0.5 * a;
        const double q = p - 0.5;
        if (std::fabs(q) <= 0.425) {
            const double r = 0.180625 - q * q;
            return q * (((((((2.509080928730122727e3 * r
                + 3.343057558358812877e4) * r + 6.726577092700870006e4) * r
                + 4.592195393154987305e4) * r + 1.373169376550946215e4) * r
                + 1.971590950306227941e3) * r + 1.331416678917643998e2) * r
                + 3.387132872796366608) /
                (((((((5.226495278852854561e3 * r + 2.872908573572194738e4) * r
                + 3.930789580009271061e4) * r + 2.121379430158659746e4) * r
                + 5.394196021424751670e3) * r + 6.871159355626076150e2) * r
                + 4.231333070160091088e1) * r + 1.0);
        }
        double r = (q < 0.0) ? p : 1.0 - p;
        r = std::sqrt(-std::log(r));
        double v;
        if (r <= 5.0) {
            r -= 1.6;
            v = (((((((7.74545014278341407640e-4 * r
                + 2.27238449892691845833e-2) * r + 2.41780725177450611770e-1) * r
                + 1.27045825245236838258e0) * r + 3.64784832476320460504e0) * r
                + 5.76949722146069140550e0) * r + 4.63033784615654529590e0) * r
                + 1.42343711074968357734e0) /
                (((((((1.05075007164441684324e-9 * r
                + 5.47593808499534494600e-4) * r + 1.51986665636164571966e-2) * r
                + 1.48103976427480074590e-1) * r + 6.89767334985100004550e-1) * r
                + 1.67638483018380384940e0) * r + 2.05319162663775882187e0) * r
                + 1.0);
        } else {
            r -= 5.0;
            v = (((((((2.01033439929228813265e-7 * r
                + 2.71155556874348757815e-5) * r + 1.24266094738807843860e-3) * r
                + 2.65321895265761230930e-2) * r + 2.96560571828504891230e-1) * r
                + 1.78482653991729133580e0) * r + 5.46378491116411436990e0) * r
                + 6.65790464350110377720e0) /
                (((((((2.04426310338993978564e-15 * r
                + 1.42151175831644588870e-7) * r + 1.84631831751005468180e-5) * r
                + 7.86869131145613259100e-4) * r + 1.48753612908506148525e-2) * r
                + 1.36929880988350204800e-1) * r + 5.99832206555887937690e-1) * r
                + 1.0);
        }
        return (q < 0.0) ? -v : v;
    }(alpha);
    auto apply_ci = [z](double t, double se, CITransform tr) -> std::pair<double, double> {
        switch (tr) {
            case CITransform::LINEAR:
                return {t - z * se, t + z * se};
            case CITransform::LOG: {
                if (!(t > 0.0))
                    return {std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::quiet_NaN()};
                const double se_log = se / t;
                return {t * std::exp(-z * se_log), t * std::exp(z * se_log)};
            }
            case CITransform::LOGIT: {
                if (!(t > 0.0 && t < 1.0))
                    return {std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::quiet_NaN()};
                const double se_logit = se / (t * (1.0 - t));
                const double logit = std::log(t / (1.0 - t));
                const double lo = logit - z * se_logit;
                const double hi = logit + z * se_logit;
                return {1.0 / (1.0 + std::exp(-lo)),
                        1.0 / (1.0 + std::exp(-hi))};
            }
        }
        return {t, t};
    };
    auto [lo1, hi1] = apply_ci(t1, se1, tr1);
    auto [lo2, hi2] = apply_ci(t2, se2, tr2);
    od[0] = lo1; od[1] = hi1; od[2] = lo2; od[3] = hi2;
    return out;
}

} // anonymous

Value betafit_ci(const Value &x, double alpha, std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    Value parm = betafit(x, mr);
    const double a = parm.elemAsDouble(0);
    const double b = parm.elemAsDouble(1);
    // Analytical observed Fisher information:
    //   I_aa = n[ψ'(a) − ψ'(a+b)]
    //   I_bb = n[ψ'(b) − ψ'(a+b)]
    //   I_ab = −n·ψ'(a+b)
    const double N = static_cast<double>(xv.size());
    const double tab = trigamma(a + b);
    const double H11 = N * (trigamma(a) - tab);
    const double H22 = N * (trigamma(b) - tab);
    const double H12 = -N * tab;
    return wald_ci_from_hessian(a, b, H11, H22, H12,
                                CITransform::LOG, CITransform::LOG,
                                alpha, mr);
}

Value nbinfit_ci(const Value &x, double alpha, std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    Value parm = nbinfit(x, mr);
    const double r = parm.elemAsDouble(0);
    const double p = parm.elemAsDouble(1);
    // Analytical observed Fisher info:
    //   I_rr = -Σ ψ'(x_i + r) + n·ψ'(r)
    //   I_pp = n·r/p² + (Σx)/(1−p)²
    //   I_rp = −n/p
    const double N = static_cast<double>(xv.size());
    double sxh = 0.0;
    double sx = 0.0;
    for (double v : xv) {
        sxh += trigamma(v + r);
        sx  += v;
    }
    const double H11 = -sxh + N * trigamma(r);
    const double H22 = N * r / (p * p) + sx / ((1.0 - p) * (1.0 - p));
    const double H12 = -N / p;
    return wald_ci_from_hessian(r, p, H11, H22, H12,
                                CITransform::LOG, CITransform::LOGIT,
                                alpha, mr);
}

Value evfit_ci(const Value &x, double alpha, std::pmr::memory_resource *mr)
{
    return evfit_ci(x, alpha, Value::Empty, Value::Empty, mr);
}

Value evfit_ci(const Value &x, double alpha,
               const Value &censoring, const Value &freq,
               std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    const std::size_t n = xv.size();
    std::vector<double> cens(n, 0.0), wf(n, 1.0);
    if (!censoring.isEmpty()) {
        for (std::size_t i = 0; i < n; ++i)
            cens[i] = censoring.elemAsDouble(i) ? 1.0 : 0.0;
    }
    if (!freq.isEmpty()) {
        for (std::size_t i = 0; i < n; ++i)
            wf[i] = freq.elemAsDouble(i);
    }
    Value parm = evfit(x, censoring, freq, mr);
    const double mu_hat    = parm.elemAsDouble(0);
    const double sigma_hat = parm.elemAsDouble(1);
    return wald_ci_2d([&](double mu, double sigma) {
                          return nll_ev_full(mu, sigma, xv, cens, wf);
                      },
                      mu_hat, sigma_hat, CITransform::LINEAR, CITransform::LOG,
                      alpha, mr);
}

Value gpfit_ci(const Value &x, double alpha, std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    Value parm = gpfit(x, mr);
    const double k_hat     = parm.elemAsDouble(0);
    const double sigma_hat = parm.elemAsDouble(1);
    return wald_ci_2d([&](double k, double sigma) { return gp_nll(k, sigma, xv); },
                      k_hat, sigma_hat, CITransform::LINEAR, CITransform::LOG,
                      alpha, mr);
}

// ── gevfit (3-param GEV MLE) ─────────────────────────────────────────

// GEV NLL: log f(x; k, σ, μ).
//   For k ≠ 0: t = 1 + k(x-μ)/σ;  f = (1/σ) t^{-1/k - 1} exp(-t^{-1/k})
//     log f = -log σ + (-1 - 1/k) log t - t^{-1/k}
//   For k → 0 (Gumbel-max limit): z = (x-μ)/σ
//     log f = -log σ - z - exp(-z)
// Returns +Inf if any support is violated.
static double nll_gev(double k, double sigma, double mu,
                      const std::vector<double> &xv)
{
    if (!(sigma > 0.0)) return std::numeric_limits<double>::infinity();
    const std::size_t n = xv.size();
    double nll = static_cast<double>(n) * std::log(sigma);
    if (std::fabs(k) < 1e-10) {
        // Gumbel-max limit.
        for (double x : xv) {
            const double z = (x - mu) / sigma;
            nll += z + std::exp(-z);
        }
    } else {
        for (double x : xv) {
            const double t = 1.0 + k * (x - mu) / sigma;
            if (!(t > 0.0)) return std::numeric_limits<double>::infinity();
            const double log_t = std::log(t);
            // t^{-1/k} = exp(-log_t/k)
            const double t_pow = std::exp(-log_t / k);
            nll += (1.0 + 1.0 / k) * log_t + t_pow;
        }
    }
    return nll;
}

Value gevfit(const Value &x, std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    const std::size_t n = xv.size();
    if (n < 3)
        throw Error("gevfit: need at least 3 observations",
                    0, 0, "gevfit", "", "numkit:gevfit:tooFewObs");
    for (double v : xv) {
        if (!std::isfinite(v))
            throw Error("gevfit: observations must be finite",
                        0, 0, "gevfit", "", "numkit:gevfit:notFinite");
    }

    // PWM initial guess (Hosking-Wallis-Wood 1985).
    std::vector<double> xs = xv;
    std::sort(xs.begin(), xs.end());
    double b0 = 0.0, b1 = 0.0, b2 = 0.0;
    const double dN = static_cast<double>(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double dI = static_cast<double>(i + 1);
        // PWM using plotting positions p_i = (i - 0.35) / n.
        const double Fhat = (dI - 0.35) / dN;
        b0 += xs[i];
        b1 += xs[i] * Fhat;
        b2 += xs[i] * Fhat * Fhat;
    }
    b0 /= dN; b1 /= dN; b2 /= dN;
    // Hosking et al. 1985 approximation for k:
    //   c = (2 b1 - b0) / (3 b2 - b0) - log(2) / log(3)
    //   k ≈ 7.8590·c + 2.9554·c²
    const double LN2_LN3 = 0.6309297535714574;  // ln(2)/ln(3)
    const double denom = 3.0 * b2 - b0;
    double k_init = 0.0;
    if (std::fabs(denom) > 1e-300) {
        const double c = (2.0 * b1 - b0) / denom - LN2_LN3;
        k_init = 7.8590 * c + 2.9554 * c * c;
    }
    // σ via:
    //   σ = k·(2 b1 - b0) / (Γ(1+k) · (1 - 2^{-k})), with k → 0 limit.
    double sigma_init;
    if (std::fabs(k_init) < 1e-6) {
        sigma_init = (2.0 * b1 - b0) / std::log(2.0);
    } else {
        const double gam_1pk = std::tgamma(1.0 + k_init);
        const double one_m_2nk = 1.0 - std::pow(2.0, -k_init);
        sigma_init = k_init * (2.0 * b1 - b0) / (gam_1pk * one_m_2nk);
    }
    if (!(sigma_init > 0.0) || !std::isfinite(sigma_init))
        sigma_init = std::max(b0 * 0.5, 1.0);
    // μ via:
    //   μ = b0 - σ · (1 - Γ(1+k)) / k, k → 0 limit: μ = b0 - σ · γ_E
    double mu_init;
    if (std::fabs(k_init) < 1e-6) {
        mu_init = b0 - sigma_init * 0.57721566490153286;
    } else {
        const double gam_1pk = std::tgamma(1.0 + k_init);
        mu_init = b0 - sigma_init * (1.0 - gam_1pk) / k_init;
    }

    // Feasibility check on initial guess; if it violates support, shift.
    auto feasible = [&](double k, double sigma, double mu) -> bool {
        if (!(sigma > 0.0)) return false;
        if (std::fabs(k) < 1e-10) return true;
        for (double v : xv) {
            if (1.0 + k * (v - mu) / sigma <= 0.0) return false;
        }
        return true;
    };
    if (!feasible(k_init, sigma_init, mu_init)) {
        // Fall back to safe Gumbel init.
        k_init = 0.0;
        sigma_init = std::sqrt(6.0 * 1.0) / 3.141592653589793;
        double mean = 0.0;
        for (double v : xv) mean += v;
        mean /= dN;
        mu_init = mean - 0.57721566490153286 * sigma_init;
    }

    // 3-D Newton on NLL with FD gradient + FD Hessian.
    double k = k_init, sigma = sigma_init, mu = mu_init;
    double f_cur = nll_gev(k, sigma, mu, xv);
    if (!std::isfinite(f_cur)) {
        k = 0.0; sigma = 1.0; mu = 0.0;
        f_cur = nll_gev(k, sigma, mu, xv);
    }

    auto nll = [&](double K, double S, double M) { return nll_gev(K, S, M, xv); };
    for (int it = 0; it < 200; ++it) {
        const double hk = std::max(std::fabs(k), 0.5) * 1e-5;
        const double hs = std::max(std::fabs(sigma), 1.0) * 1e-5;
        const double hm = std::max(std::fabs(mu), 1.0) * 1e-5;
        // Gradient.
        const double gk = (nll(k + hk, sigma, mu) - nll(k - hk, sigma, mu)) / (2.0 * hk);
        const double gs = (nll(k, sigma + hs, mu) - nll(k, sigma - hs, mu)) / (2.0 * hs);
        const double gm = (nll(k, sigma, mu + hm) - nll(k, sigma, mu - hm)) / (2.0 * hm);
        const double gn = std::sqrt(gk * gk + gs * gs + gm * gm);
        if (gn < 1e-12) break;
        // Hessian (symmetric).
        const double Hkk = (nll(k + hk, sigma, mu) - 2.0 * f_cur + nll(k - hk, sigma, mu)) / (hk * hk);
        const double Hss = (nll(k, sigma + hs, mu) - 2.0 * f_cur + nll(k, sigma - hs, mu)) / (hs * hs);
        const double Hmm = (nll(k, sigma, mu + hm) - 2.0 * f_cur + nll(k, sigma, mu - hm)) / (hm * hm);
        const double Hks = (nll(k + hk, sigma + hs, mu) - nll(k + hk, sigma - hs, mu)
                         -  nll(k - hk, sigma + hs, mu) + nll(k - hk, sigma - hs, mu))
                          / (4.0 * hk * hs);
        const double Hkm = (nll(k + hk, sigma, mu + hm) - nll(k + hk, sigma, mu - hm)
                         -  nll(k - hk, sigma, mu + hm) + nll(k - hk, sigma, mu - hm))
                          / (4.0 * hk * hm);
        const double Hsm = (nll(k, sigma + hs, mu + hm) - nll(k, sigma + hs, mu - hm)
                         -  nll(k, sigma - hs, mu + hm) + nll(k, sigma - hs, mu - hm))
                          / (4.0 * hs * hm);
        // Solve 3x3 H · d = -g (Cramer's rule for small system).
        const double H11 = Hkk, H12 = Hks, H13 = Hkm;
        const double H21 = Hks, H22 = Hss, H23 = Hsm;
        const double H31 = Hkm, H32 = Hsm, H33 = Hmm;
        const double det = H11 * (H22 * H33 - H23 * H32)
                         - H12 * (H21 * H33 - H23 * H31)
                         + H13 * (H21 * H32 - H22 * H31);
        if (std::fabs(det) < 1e-300) break;
        // -g = b
        const double b1n = -gk, b2n = -gs, b3n = -gm;
        const double dk = (b1n * (H22 * H33 - H23 * H32)
                         - H12 * (b2n * H33 - H23 * b3n)
                         + H13 * (b2n * H32 - H22 * b3n)) / det;
        const double ds = (H11 * (b2n * H33 - H23 * b3n)
                         - b1n * (H21 * H33 - H23 * H31)
                         + H13 * (H21 * b3n - b2n * H31)) / det;
        const double dm = (H11 * (H22 * b3n - b2n * H32)
                         - H12 * (H21 * b3n - b2n * H31)
                         + b1n * (H21 * H32 - H22 * H31)) / det;
        // Backtracking line search.
        double sc = 1.0;
        bool ok = false;
        for (int bt = 0; bt < 30; ++bt) {
            const double kn = k + sc * dk;
            const double sn = sigma + sc * ds;
            const double mn = mu + sc * dm;
            if (sn > 0.0 && feasible(kn, sn, mn)) {
                const double f_new = nll(kn, sn, mn);
                if (std::isfinite(f_new) && f_new < f_cur - 1e-15) {
                    k = kn; sigma = sn; mu = mn; f_cur = f_new;
                    ok = true; break;
                }
            }
            sc *= 0.5;
        }
        if (!ok) break;
        if (std::fabs(sc * dk) + std::fabs(sc * ds) + std::fabs(sc * dm)
            < 1e-12 * (std::fabs(k) + std::fabs(sigma) + std::fabs(mu) + 1.0)) break;
    }

    auto out = Value::matrix(1, 3, ValueType::DOUBLE, mr);
    out.doubleDataMut()[0] = k;
    out.doubleDataMut()[1] = sigma;
    out.doubleDataMut()[2] = mu;
    return out;
}

Value gevfit_ci(const Value &x, double alpha, std::pmr::memory_resource *mr)
{
    auto xv = toFlat(x);
    Value parm = gevfit(x, mr);
    const double k_hat     = parm.elemAsDouble(0);
    const double sigma_hat = parm.elemAsDouble(1);
    const double mu_hat    = parm.elemAsDouble(2);

    // Compute 3-D FD Hessian of NLL at parmhat → V = H^{-1} → SE_j.
    auto nll = [&](double K, double S, double M) { return nll_gev(K, S, M, xv); };
    const double hk = std::max(std::fabs(k_hat), 0.5) * 1e-5;
    const double hs = std::max(std::fabs(sigma_hat), 1.0) * 1e-5;
    const double hm = std::max(std::fabs(mu_hat), 1.0) * 1e-5;
    const double f00 = nll(k_hat, sigma_hat, mu_hat);
    const double Hkk = (nll(k_hat + hk, sigma_hat, mu_hat) - 2 * f00
                     + nll(k_hat - hk, sigma_hat, mu_hat)) / (hk * hk);
    const double Hss = (nll(k_hat, sigma_hat + hs, mu_hat) - 2 * f00
                     + nll(k_hat, sigma_hat - hs, mu_hat)) / (hs * hs);
    const double Hmm = (nll(k_hat, sigma_hat, mu_hat + hm) - 2 * f00
                     + nll(k_hat, sigma_hat, mu_hat - hm)) / (hm * hm);
    const double Hks = (nll(k_hat + hk, sigma_hat + hs, mu_hat)
                     -  nll(k_hat + hk, sigma_hat - hs, mu_hat)
                     -  nll(k_hat - hk, sigma_hat + hs, mu_hat)
                     +  nll(k_hat - hk, sigma_hat - hs, mu_hat)) / (4 * hk * hs);
    const double Hkm = (nll(k_hat + hk, sigma_hat, mu_hat + hm)
                     -  nll(k_hat + hk, sigma_hat, mu_hat - hm)
                     -  nll(k_hat - hk, sigma_hat, mu_hat + hm)
                     +  nll(k_hat - hk, sigma_hat, mu_hat - hm)) / (4 * hk * hm);
    const double Hsm = (nll(k_hat, sigma_hat + hs, mu_hat + hm)
                     -  nll(k_hat, sigma_hat + hs, mu_hat - hm)
                     -  nll(k_hat, sigma_hat - hs, mu_hat + hm)
                     +  nll(k_hat, sigma_hat - hs, mu_hat - hm)) / (4 * hs * hm);
    // Invert 3×3 to get cov diagonals.
    const double H[9] = {Hkk, Hks, Hkm, Hks, Hss, Hsm, Hkm, Hsm, Hmm};
    const double det = H[0]*(H[4]*H[8]-H[5]*H[7]) - H[1]*(H[3]*H[8]-H[5]*H[6])
                     + H[2]*(H[3]*H[7]-H[4]*H[6]);
    Value out = Value::matrix(2, 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    if (std::fabs(det) < 1e-300) {
        for (int i = 0; i < 6; ++i) od[i] = std::numeric_limits<double>::quiet_NaN();
        return out;
    }
    // V_jj = cofactor_jj / det.
    const double V_kk = (H[4]*H[8] - H[5]*H[7]) / det;
    const double V_ss = (H[0]*H[8] - H[2]*H[6]) / det;
    const double V_mm = (H[0]*H[4] - H[1]*H[3]) / det;
    const double se_k = std::sqrt(std::max(V_kk, 0.0));
    const double se_s = std::sqrt(std::max(V_ss, 0.0));
    const double se_m = std::sqrt(std::max(V_mm, 0.0));

    // z_{α/2} via full Wichura AS-241 (handles tail beyond |q| > 0.425).
    auto z_alpha = [](double a) -> double {
        const double p = 1.0 - 0.5 * a;
        const double q = p - 0.5;
        if (std::fabs(q) <= 0.425) {
            const double r = 0.180625 - q * q;
            return q * (((((((2.509080928730122727e3 * r
                + 3.343057558358812877e4) * r + 6.726577092700870006e4) * r
                + 4.592195393154987305e4) * r + 1.373169376550946215e4) * r
                + 1.971590950306227941e3) * r + 1.331416678917643998e2) * r
                + 3.387132872796366608) /
                (((((((5.226495278852854561e3 * r + 2.872908573572194738e4) * r
                + 3.930789580009271061e4) * r + 2.121379430158659746e4) * r
                + 5.394196021424751670e3) * r + 6.871159355626076150e2) * r
                + 4.231333070160091088e1) * r + 1.0);
        }
        double r = (q < 0.0) ? p : 1.0 - p;
        r = std::sqrt(-std::log(r));
        double v;
        if (r <= 5.0) {
            r -= 1.6;
            v = (((((((7.74545014278341407640e-4 * r
                + 2.27238449892691845833e-2) * r + 2.41780725177450611770e-1) * r
                + 1.27045825245236838258e0) * r + 3.64784832476320460504e0) * r
                + 5.76949722146069140550e0) * r + 4.63033784615654529590e0) * r
                + 1.42343711074968357734e0) /
                (((((((1.05075007164441684324e-9 * r
                + 5.47593808499534494600e-4) * r + 1.51986665636164571966e-2) * r
                + 1.48103976427480074590e-1) * r + 6.89767334985100004550e-1) * r
                + 1.67638483018380384940e0) * r + 2.05319162663775882187e0) * r
                + 1.0);
        } else {
            r -= 5.0;
            v = (((((((2.01033439929228813265e-7 * r
                + 2.71155556874348757815e-5) * r + 1.24266094738807843860e-3) * r
                + 2.65321895265761230930e-2) * r + 2.96560571828504891230e-1) * r
                + 1.78482653991729133580e0) * r + 5.46378491116411436990e0) * r
                + 6.65790464350110377720e0) /
                (((((((2.04426310338993978564e-15 * r
                + 1.42151175831644588870e-7) * r + 1.84631831751005468180e-5) * r
                + 7.86869131145613259100e-4) * r + 1.48753612908506148525e-2) * r
                + 1.36929880988350204800e-1) * r + 5.99832206555887937690e-1) * r
                + 1.0);
        }
        return (q < 0.0) ? -v : v;
    };
    const double z = z_alpha(alpha);
    // k linear CI, σ log CI, μ linear CI.
    const double se_log_sigma = (sigma_hat > 0.0) ? se_s / sigma_hat : 0.0;
    od[0] = k_hat - z * se_k;
    od[1] = k_hat + z * se_k;
    od[2] = sigma_hat * std::exp(-z * se_log_sigma);
    od[3] = sigma_hat * std::exp( z * se_log_sigma);
    od[4] = mu_hat - z * se_m;
    od[5] = mu_hat + z * se_m;
    return out;
}

// ── Adapters ─────────────────────────────────────────────────────────
namespace detail {

namespace {
// Helper: read optional `alpha` (2nd positional arg), default 0.05.
double parse_alpha(Span<const Value> args, std::size_t idx = 1)
{
    if (args.size() > idx) {
        const Value &v = args[idx];
        if (!v.isChar() && !v.isString() && !v.isEmpty())
            return v.toScalar();
    }
    return 0.05;
}
} // anonymous

void gamfit_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gamfit: requires (x[, alpha[, cens[, freq[, options]]]])",
                    0, 0, "gamfit", "", "numkit:gamfit:nargin");
    auto *mr = ctx.engine->resource();
    const Value cens = (args.size() > 2) ? args[2] : Value::Empty;
    const Value freq = (args.size() > 3) ? args[3] : Value::Empty;
    outs[0] = gamfit(args[0], cens, freq, mr);
    if (nargout >= 2)
        outs[1] = gamfit_ci(args[0], parse_alpha(args), cens, freq, mr);
}

void wblfit_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("wblfit: requires (x[, alpha[, cens[, freq[, options]]]])",
                    0, 0, "wblfit", "", "numkit:wblfit:nargin");
    auto *mr = ctx.engine->resource();
    const Value cens = (args.size() > 2) ? args[2] : Value::Empty;
    const Value freq = (args.size() > 3) ? args[3] : Value::Empty;
    outs[0] = wblfit(args[0], cens, freq, mr);
    if (nargout >= 2)
        outs[1] = wblfit_ci(args[0], parse_alpha(args), cens, freq, mr);
}

void betafit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("betafit: requires (x[, alpha])",
                    0, 0, "betafit", "", "numkit:betafit:nargin");
    auto *mr = ctx.engine->resource();
    outs[0] = betafit(args[0], mr);
    if (nargout >= 2) outs[1] = betafit_ci(args[0], parse_alpha(args), mr);
}

void nbinfit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nbinfit: requires (x[, alpha])",
                    0, 0, "nbinfit", "", "numkit:nbinfit:nargin");
    auto *mr = ctx.engine->resource();
    outs[0] = nbinfit(args[0], mr);
    if (nargout >= 2) outs[1] = nbinfit_ci(args[0], parse_alpha(args), mr);
}

void evfit_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("evfit: requires (x[, alpha[, cens[, freq[, options]]]])",
                    0, 0, "evfit", "", "numkit:evfit:nargin");
    auto *mr = ctx.engine->resource();
    const Value cens = (args.size() > 2) ? args[2] : Value::Empty;
    const Value freq = (args.size() > 3) ? args[3] : Value::Empty;
    // args[4] = options struct — currently no-op (parsed for compat).
    outs[0] = evfit(args[0], cens, freq, mr);
    if (nargout >= 2)
        outs[1] = evfit_ci(args[0], parse_alpha(args), cens, freq, mr);
}

void gpfit_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gpfit: requires (x[, alpha])",
                    0, 0, "gpfit", "", "numkit:gpfit:nargin");
    auto *mr = ctx.engine->resource();
    outs[0] = gpfit(args[0], mr);
    if (nargout >= 2) outs[1] = gpfit_ci(args[0], parse_alpha(args), mr);
}

void gevfit_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gevfit: requires (x[, alpha])",
                    0, 0, "gevfit", "", "numkit:gevfit:nargin");
    auto *mr = ctx.engine->resource();
    outs[0] = gevfit(args[0], mr);
    if (nargout >= 2) outs[1] = gevfit_ci(args[0], parse_alpha(args), mr);
}

} // namespace detail
} // namespace numkit::stats
