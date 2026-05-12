// libs/stats/src/test/hypothesis.cpp

#include <numkit/stats/test/hypothesis.hpp>

#include <numkit/stats/distributions/students_t.hpp>
#include <numkit/stats/distributions/normal.hpp>
#include <numkit/stats/distributions/chi2.hpp>
#include <numkit/stats/distributions/fisher_f.hpp>
#include <numkit/stats/distributions/binomial.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <random>
#include <vector>

namespace numkit::stats {

namespace {

void mean_var(const Value &x, double &mean_out, double &var_out, size_t &n_out) {
    const size_t N = x.numel();
    n_out = N;
    if (N == 0) { mean_out = 0.0; var_out = 0.0; return; }
    double s = 0.0;
    for (size_t i = 0; i < N; ++i) s += x.elemAsDouble(i);
    mean_out = s / double(N);
    if (N < 2) { var_out = 0.0; return; }
    double sq = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double d = x.elemAsDouble(i) - mean_out;
        sq += d * d;
    }
    var_out = sq / double(N - 1);
}

inline TestTail parse_tail(const std::string &s, TestTail def) {
    if (s == "both")     return TestTail::Both;
    if (s == "right")    return TestTail::Right;
    if (s == "left")     return TestTail::Left;
    // kstest / kstest2 aliases:
    if (s == "unequal")  return TestTail::Both;
    if (s == "larger")   return TestTail::Right;
    if (s == "smaller")  return TestTail::Left;
    return def;
}

// Compute two-sided / one-sided p-value from a t-statistic and df.
double tpvalue(double tstat, double df, TestTail tail, std::pmr::memory_resource *mr) {
    Value tv = Value::scalar(tstat, mr);
    Value cdf_v = tcdf(tv, df, mr);
    const double cdf = cdf_v.toScalar();
    switch (tail) {
        case TestTail::Both:  return 2.0 * std::min(cdf, 1.0 - cdf);
        case TestTail::Right: return 1.0 - cdf;
        case TestTail::Left:  return cdf;
    }
    return 1.0;
}

double zpvalue(double z, TestTail tail, std::pmr::memory_resource *mr) {
    Value zv = Value::scalar(z, mr);
    Value cdf_v = normcdf(zv, 0.0, 1.0, mr);
    const double cdf = cdf_v.toScalar();
    switch (tail) {
        case TestTail::Both:  return 2.0 * std::min(cdf, 1.0 - cdf);
        case TestTail::Right: return 1.0 - cdf;
        case TestTail::Left:  return cdf;
    }
    return 1.0;
}

} // anonymous

// ════════════════════════════════════════════════════════════════════
// ttest — one-sample
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value, Value>
ttest(const Value &x, double m, double alpha, TestTail tail, std::pmr::memory_resource *mr)
{
    if (alpha <= 0.0 || alpha >= 1.0) alpha = 0.05;

    double mu_hat, var_hat; size_t n;
    mean_var(x, mu_hat, var_hat, n);
    if (n < 2)
        throw Error("ttest: need at least 2 samples", 0, 0, "ttest", "",
                    "m:ttest:nsamples");

    const double sd  = std::sqrt(var_hat);
    const double se  = sd / std::sqrt(double(n));
    const double t   = (mu_hat - m) / se;
    const double df  = double(n - 1);
    const double p   = tpvalue(t, df, tail, mr);
    const int    h   = (p < alpha) ? 1 : 0;

    // Confidence interval for the mean.
    double clo, chi;
    Value half = Value::scalar(1.0 - 0.5 * alpha, mr);
    Value tcrit_v = tinv(half, df, mr);
    const double tcrit = tcrit_v.toScalar();
    switch (tail) {
        case TestTail::Both:
            clo = mu_hat - tcrit * se; chi = mu_hat + tcrit * se; break;
        case TestTail::Right: {
            Value full = Value::scalar(1.0 - alpha, mr);
            const double tc = tinv(full, df, mr).toScalar();
            clo = mu_hat - tc * se; chi = std::numeric_limits<double>::infinity();
            break;
        }
        case TestTail::Left: {
            Value full = Value::scalar(1.0 - alpha, mr);
            const double tc = tinv(full, df, mr).toScalar();
            clo = -std::numeric_limits<double>::infinity(); chi = mu_hat + tc * se;
            break;
        }
    }
    Value ci = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    ci.doubleDataMut()[0] = clo; ci.doubleDataMut()[1] = chi;

    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p, mr),
                           std::move(ci),
                           Value::scalar(t, mr));
}

// ════════════════════════════════════════════════════════════════════
// ttest2 — two-sample
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value, Value>
ttest2(const Value &x, const Value &y, double alpha, TestTail tail, const std::string &vartype, std::pmr::memory_resource *mr)
{
    if (alpha <= 0.0 || alpha >= 1.0) alpha = 0.05;

    double mx, vx, my, vy; size_t nx, ny;
    mean_var(x, mx, vx, nx);
    mean_var(y, my, vy, ny);
    if (nx < 2 || ny < 2)
        throw Error("ttest2: need at least 2 samples per group",
                    0, 0, "ttest2", "", "m:ttest2:nsamples");

    double t, df, se;
    if (vartype == "equal") {
        const double sp2 = ((nx - 1) * vx + (ny - 1) * vy) / double(nx + ny - 2);
        se = std::sqrt(sp2 * (1.0 / double(nx) + 1.0 / double(ny)));
        df = double(nx + ny - 2);
    } else {
        // Welch (default).
        se = std::sqrt(vx / double(nx) + vy / double(ny));
        const double num = (vx / nx + vy / ny) * (vx / nx + vy / ny);
        const double den = (vx / nx) * (vx / nx) / double(nx - 1)
                         + (vy / ny) * (vy / ny) / double(ny - 1);
        df = (den > 0.0) ? num / den : double(nx + ny - 2);
    }
    t = (mx - my) / se;
    const double p = tpvalue(t, df, tail, mr);
    const int    h = (p < alpha) ? 1 : 0;

    Value half = Value::scalar(1.0 - 0.5 * alpha, mr);
    const double tcrit = tinv(half, df, mr).toScalar();
    Value ci = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    ci.doubleDataMut()[0] = (mx - my) - tcrit * se;
    ci.doubleDataMut()[1] = (mx - my) + tcrit * se;

    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p, mr),
                           std::move(ci),
                           Value::scalar(t, mr));
}

// ════════════════════════════════════════════════════════════════════
// ztest — known-σ
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value, Value>
ztest(const Value &x, double m, double sigma, double alpha, TestTail tail, std::pmr::memory_resource *mr)
{
    if (alpha <= 0.0 || alpha >= 1.0) alpha = 0.05;
    if (sigma <= 0.0)
        throw Error("ztest: sigma must be positive", 0, 0, "ztest", "",
                    "m:ztest:badsigma");

    double mu_hat, var_hat; size_t n;
    mean_var(x, mu_hat, var_hat, n);
    if (n < 1)
        throw Error("ztest: need at least 1 sample", 0, 0, "ztest", "",
                    "m:ztest:nsamples");

    const double se = sigma / std::sqrt(double(n));
    const double z  = (mu_hat - m) / se;
    const double p  = zpvalue(z, tail, mr);
    const int    h  = (p < alpha) ? 1 : 0;

    Value half = Value::scalar(1.0 - 0.5 * alpha, mr);
    const double zcrit = norminv(half, 0.0, 1.0, mr).toScalar();
    Value ci = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    ci.doubleDataMut()[0] = mu_hat - zcrit * se;
    ci.doubleDataMut()[1] = mu_hat + zcrit * se;

    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p, mr),
                           std::move(ci),
                           Value::scalar(z, mr));
}

// ════════════════════════════════════════════════════════════════════
// vartest — chi-squared one-sample
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value, Value>
vartest(const Value &x, double v, double alpha, TestTail tail, std::pmr::memory_resource *mr)
{
    if (alpha <= 0.0 || alpha >= 1.0) alpha = 0.05;
    if (v <= 0.0)
        throw Error("vartest: v must be positive", 0, 0, "vartest", "",
                    "m:vartest:badv");

    double mu_hat, var_hat; size_t n;
    mean_var(x, mu_hat, var_hat, n);
    if (n < 2)
        throw Error("vartest: need at least 2 samples", 0, 0, "vartest", "",
                    "m:vartest:nsamples");

    const double df = double(n - 1);
    const double T  = df * var_hat / v;
    Value Tv = Value::scalar(T, mr);
    const double cdf = chi2cdf(Tv, df, mr).toScalar();

    double p;
    switch (tail) {
        case TestTail::Both:  p = 2.0 * std::min(cdf, 1.0 - cdf); break;
        case TestTail::Right: p = 1.0 - cdf;                      break;
        case TestTail::Left:  p = cdf;                            break;
        default:              p = 1.0;
    }
    const int h = (p < alpha) ? 1 : 0;

    // Confidence interval for σ² (two-sided).
    Value lo_v = Value::scalar(0.5 * alpha, mr);
    Value hi_v = Value::scalar(1.0 - 0.5 * alpha, mr);
    const double chi_lo = chi2inv(lo_v, df, mr).toScalar();
    const double chi_hi = chi2inv(hi_v, df, mr).toScalar();
    Value ci = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    ci.doubleDataMut()[0] = (chi_hi > 0.0) ? df * var_hat / chi_hi : 0.0;
    ci.doubleDataMut()[1] = (chi_lo > 0.0) ? df * var_hat / chi_lo
                                            : std::numeric_limits<double>::infinity();

    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p, mr),
                           std::move(ci),
                           Value::scalar(T, mr));
}

// ════════════════════════════════════════════════════════════════════
// vartest2 — F-test for equal variances
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value, Value>
vartest2(const Value &x, const Value &y, double alpha, TestTail tail, std::pmr::memory_resource *mr)
{
    if (alpha <= 0.0 || alpha >= 1.0) alpha = 0.05;

    double mx, vx, my, vy; size_t nx, ny;
    mean_var(x, mx, vx, nx);
    mean_var(y, my, vy, ny);
    if (nx < 2 || ny < 2 || vy == 0.0)
        throw Error("vartest2: need ≥ 2 samples per group and var(y) > 0",
                    0, 0, "vartest2", "", "m:vartest2:nsamples");

    const double F  = vx / vy;
    const double v1 = double(nx - 1);
    const double v2 = double(ny - 1);
    Value Fv = Value::scalar(F, mr);
    const double cdf = fcdf(Fv, v1, v2, mr).toScalar();

    double p;
    switch (tail) {
        case TestTail::Both:  p = 2.0 * std::min(cdf, 1.0 - cdf); break;
        case TestTail::Right: p = 1.0 - cdf;                      break;
        case TestTail::Left:  p = cdf;                            break;
        default:              p = 1.0;
    }
    const int h = (p < alpha) ? 1 : 0;

    // Confidence interval for the variance ratio σ_x²/σ_y².
    Value lo_v = Value::scalar(0.5 * alpha, mr);
    Value hi_v = Value::scalar(1.0 - 0.5 * alpha, mr);
    const double f_lo = finv(lo_v, v1, v2, mr).toScalar();
    const double f_hi = finv(hi_v, v1, v2, mr).toScalar();
    Value ci = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    ci.doubleDataMut()[0] = (f_hi > 0.0) ? F / f_hi : 0.0;
    ci.doubleDataMut()[1] = (f_lo > 0.0) ? F / f_lo
                                          : std::numeric_limits<double>::infinity();

    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p, mr),
                           std::move(ci),
                           Value::scalar(F, mr));
}

// ════════════════════════════════════════════════════════════════════
// kstest — one-sample Kolmogorov-Smirnov
// ════════════════════════════════════════════════════════════════════

namespace {

// Asymptotic Kolmogorov distribution survival function (P(K > x)) for
// the test statistic Dn·√n. Uses the fast-converging Smirnov series
// for the upper tail (large x) and the slow Kolmogorov series for the
// lower tail. Adequate for double precision.
double ks_pvalue(double d) {
    // d = Dn·√n. P(K ≥ d) = 2 · Σ_{k=1..∞} (-1)^(k-1) · exp(-2 k² d²).
    if (d <= 0.0) return 1.0;
    double total = 0.0;
    double prev = 0.0;
    for (int k = 1; k <= 100; ++k) {
        const double term = std::exp(-2.0 * k * k * d * d);
        if (term < 1e-20 && k > 4) break;
        total += (k & 1) ? term : -term;
        if (std::fabs(total - prev) < 1e-12 * std::fabs(total) && k > 4) break;
        prev = total;
    }
    double p = 2.0 * total;
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    return p;
}

// Asymptotic critical value: solve ks_pvalue(d) = α for d ≈ -ln(α/2)/√…
// Bisection is fine for doc-quality CV.
double ks_critical(double alpha) {
    if (alpha <= 0.0 || alpha >= 1.0) alpha = 0.05;
    double lo = 0.01, hi = 5.0;
    for (int it = 0; it < 80; ++it) {
        const double mid = 0.5 * (lo + hi);
        if (ks_pvalue(mid) > alpha) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}

// Evaluate a piecewise-linear empirical CDF at point x given a sorted
// reference grid (x_grid, F_grid). Outside the grid, clamp to 0 / 1.
double interp_cdf(const std::vector<double> &xg,
                  const std::vector<double> &Fg, double x) {
    if (xg.empty()) return 0.0;
    if (x <= xg.front()) return Fg.front();
    if (x >= xg.back())  return Fg.back();
    auto it = std::upper_bound(xg.begin(), xg.end(), x);
    const size_t k = (size_t)(it - xg.begin()) - 1;
    const double t = (x - xg[k]) / (xg[k + 1] - xg[k]);
    return Fg[k] + t * (Fg[k + 1] - Fg[k]);
}

} // anonymous

std::tuple<Value, Value, Value, Value>
kstest(const Value &x, const Value &cdf, double alpha, TestTail tail, std::pmr::memory_resource *mr)
{
    if (alpha <= 0.0 || alpha >= 1.0) alpha = 0.05;
    const size_t N = x.numel();
    if (N < 1)
        throw Error("kstest: empty sample", 0, 0, "kstest", "",
                    "m:kstest:nsamples");

    std::vector<double> xs(N);
    for (size_t i = 0; i < N; ++i) xs[i] = x.elemAsDouble(i);
    std::sort(xs.begin(), xs.end());

    // Reference CDF.
    auto refF = [&](double v) {
        if (cdf.numel() >= 2) {
            const size_t rows = cdf.dims().rows();
            std::vector<double> xg(rows), Fg(rows);
            for (size_t i = 0; i < rows; ++i) {
                xg[i] = cdf.elemAsDouble(0 * rows + i);
                Fg[i] = cdf.elemAsDouble(1 * rows + i);
            }
            // xg should already be sorted; if not, do a quick sort.
            if (!std::is_sorted(xg.begin(), xg.end())) {
                std::vector<size_t> ord(rows);
                std::iota(ord.begin(), ord.end(), (size_t)0);
                std::sort(ord.begin(), ord.end(),
                          [&](size_t a, size_t b){ return xg[a] < xg[b]; });
                std::vector<double> xs2(rows), Fs2(rows);
                for (size_t i = 0; i < rows; ++i) { xs2[i] = xg[ord[i]]; Fs2[i] = Fg[ord[i]]; }
                xg = std::move(xs2); Fg = std::move(Fs2);
            }
            return interp_cdf(xg, Fg, v);
        }
        // Default: standard normal.
        Value s = Value::scalar(v, mr);
        return normcdf(s, 0.0, 1.0, mr).toScalar();
    };

    // Compute D⁺ and D⁻ relative to reference; combine per tail.
    double Dplus = 0.0, Dminus = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double F = refF(xs[i]);
        const double up = double(i + 1) / double(N) - F;
        const double dn = F - double(i) / double(N);
        if (up > Dplus)  Dplus = up;
        if (dn > Dminus) Dminus = dn;
    }
    double D;
    switch (tail) {
        case TestTail::Right: D = Dplus;             break;
        case TestTail::Left:  D = Dminus;            break;
        default:              D = std::max(Dplus, Dminus);
    }
    const double dn = D * std::sqrt(double(N));
    const double p  = ks_pvalue(dn);
    const int    h  = (p < alpha) ? 1 : 0;
    const double cv = ks_critical(alpha) / std::sqrt(double(N));

    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p, mr),
                           Value::scalar(D, mr),
                           Value::scalar(cv, mr));
}

std::tuple<Value, Value, Value, Value>
kstest2(const Value &x, const Value &y, double alpha, TestTail tail, std::pmr::memory_resource *mr)
{
    if (alpha <= 0.0 || alpha >= 1.0) alpha = 0.05;
    const size_t Nx = x.numel(), Ny = y.numel();
    if (Nx < 1 || Ny < 1)
        throw Error("kstest2: empty sample", 0, 0, "kstest2", "",
                    "m:kstest2:nsamples");

    std::vector<double> xs(Nx), ys(Ny);
    for (size_t i = 0; i < Nx; ++i) xs[i] = x.elemAsDouble(i);
    for (size_t i = 0; i < Ny; ++i) ys[i] = y.elemAsDouble(i);
    std::sort(xs.begin(), xs.end());
    std::sort(ys.begin(), ys.end());

    // Walk the merged sorted axis to compute D⁺ and D⁻.
    double Dplus = 0.0, Dminus = 0.0;
    size_t i = 0, j = 0;
    while (i < Nx && j < Ny) {
        const double a = xs[i], b = ys[j];
        if (a <= b) ++i;
        if (b <= a) ++j;
        const double Fx = double(i) / double(Nx);
        const double Fy = double(j) / double(Ny);
        if (Fx - Fy > Dplus) Dplus = Fx - Fy;
        if (Fy - Fx > Dminus) Dminus = Fy - Fx;
    }
    double D;
    switch (tail) {
        case TestTail::Right: D = Dplus;            break;
        case TestTail::Left:  D = Dminus;           break;
        default:              D = std::max(Dplus, Dminus);
    }
    const double n_eff = double(Nx) * double(Ny) / double(Nx + Ny);
    const double dn = D * std::sqrt(n_eff);
    const double p  = ks_pvalue(dn);
    const int    h  = (p < alpha) ? 1 : 0;
    const double cv = ks_critical(alpha) / std::sqrt(n_eff);

    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p, mr),
                           Value::scalar(D, mr),
                           Value::scalar(cv, mr));
}

// ════════════════════════════════════════════════════════════════════
// jbtest — Jarque-Bera normality
// ════════════════════════════════════════════════════════════════════

namespace {

// Compute the JB statistic for a sample (used by both the main entry
// point and the Monte-Carlo H₀ simulator).
inline double jb_stat(const double *x, size_t N) {
    double mean = 0.0;
    for (size_t i = 0; i < N; ++i) mean += x[i];
    mean /= double(N);
    double m2 = 0.0, m3 = 0.0, m4 = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double d = x[i] - mean;
        const double d2 = d * d;
        m2 += d2;
        m3 += d2 * d;
        m4 += d2 * d2;
    }
    m2 /= double(N); m3 /= double(N); m4 /= double(N);
    const double S = (m2 > 0.0) ? m3 / std::pow(m2, 1.5) : 0.0;
    const double K = (m2 > 0.0) ? m4 / (m2 * m2) : 0.0;
    return double(N) / 6.0 * (S * S + 0.25 * (K - 3.0) * (K - 3.0));
}

// Monte-Carlo p / critval for JB at sample size N. Iterates batches
// until SE(p̂) < mctol or until iter cap. Uses a deterministic seed
// (so spec output is reproducible). Returns (p, critval) where p is
// MATLAB-capped at 0.5.
struct JBMCResult { double p; double cv; };
JBMCResult jb_montecarlo(size_t N, double JB_obs, double alpha, double mctol)
{
    // Hard caps: at most 1e6 reps, at least 1000 batch.
    const size_t batch     = 1000;
    const size_t max_reps  = 1'000'000;
    const double cap_p     = 0.5;  // MATLAB caps p at 0.5

    std::mt19937_64 gen(12345ULL);  // deterministic
    std::normal_distribution<double> nd(0.0, 1.0);
    std::vector<double> buf(N);
    std::vector<double> JB_h0;
    JB_h0.reserve(batch);

    size_t total = 0, exceeds = 0;
    while (total < max_reps) {
        const size_t this_batch = std::min(batch, max_reps - total);
        for (size_t i = 0; i < this_batch; ++i) {
            for (size_t k = 0; k < N; ++k) buf[k] = nd(gen);
            const double j = jb_stat(buf.data(), N);
            JB_h0.push_back(j);
            if (j >= JB_obs) ++exceeds;
        }
        total += this_batch;
        const double p_hat = double(exceeds) / double(total);
        const double se = std::sqrt(std::max(p_hat * (1.0 - p_hat), 1.0 / double(total))
                                    / double(total));
        if (se < mctol && total >= 3 * batch) break;
    }
    // Critical value: (1-alpha) quantile of the empirical H₀ distribution.
    std::sort(JB_h0.begin(), JB_h0.end());
    const double rank = (1.0 - alpha) * (double(JB_h0.size()) - 1.0);
    const size_t lo = (size_t)std::floor(rank);
    const size_t hi = std::min(lo + 1, JB_h0.size() - 1);
    const double frac = rank - double(lo);
    const double cv = JB_h0[lo] * (1.0 - frac) + JB_h0[hi] * frac;

    double p = double(exceeds) / double(total);
    if (p > cap_p) p = cap_p;
    return {p, cv};
}

} // anonymous

std::tuple<Value, Value, Value, Value>
jbtest(const Value &x, double alpha, double mctol, std::pmr::memory_resource *mr)
{
    if (alpha <= 0.0 || alpha >= 1.0) alpha = 0.05;
    const size_t N = x.numel();
    if (N < 4)
        throw Error("jbtest: need at least 4 samples", 0, 0, "jbtest", "",
                    "m:jbtest:nsamples");

    // Read sample into a flat scratch buffer.
    ScratchArena scratch(mr);
    ScratchVec<double> xv(N, &scratch);
    for (size_t i = 0; i < N; ++i) xv[i] = x.elemAsDouble(i);

    const double JB = jb_stat(xv.data(), N);

    // Path selection: use Monte Carlo for small samples by default
    // (matching MATLAB's tabulated-p behavior); use χ²(2) asymptotic
    // for large n. mctol overrides — when supplied, always use MC.
    double p, cv;
    const bool use_mc = std::isfinite(mctol) || N < 2000;
    const double mctol_eff = std::isfinite(mctol) ? mctol : 1e-3;
    if (use_mc) {
        auto R = jb_montecarlo(N, JB, alpha, mctol_eff);
        p  = R.p;
        cv = R.cv;
    } else {
        Value JBv = Value::scalar(JB, mr);
        const double cdf = chi2cdf(JBv, 2.0, mr).toScalar();
        p = 1.0 - cdf;
        Value oneMinusAlpha = Value::scalar(1.0 - alpha, mr);
        cv = chi2inv(oneMinusAlpha, 2.0, mr).toScalar();
    }
    const int h = (p < alpha) ? 1 : 0;

    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p, mr),
                           Value::scalar(JB, mr),
                           Value::scalar(cv, mr));
}

// Backward-compat 2-arg form — uses MC for small N (n < 2000).
std::tuple<Value, Value, Value, Value>
jbtest(const Value &x, double alpha, std::pmr::memory_resource *mr)
{
    return jbtest(x, alpha, std::numeric_limits<double>::quiet_NaN(), mr);
}

// ════════════════════════════════════════════════════════════════════
// signtest — non-parametric sign test
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value>
signtest(const Value &x, const Value &y_or_m, double alpha, TestTail tail, std::pmr::memory_resource *mr)
{
    const size_t Nx = x.numel();
    const bool paired = (!y_or_m.isEmpty() && !y_or_m.isScalar());
    if (paired && y_or_m.numel() != Nx)
        throw Error("signtest: X and Y must have the same length",
                    0, 0, "signtest", "", "m:signtest:size");
    const double m0 = (paired || y_or_m.isEmpty()) ? 0.0 : y_or_m.toScalar();

    // Count positive and non-zero diffs.
    long long n_pos = 0;
    long long n_eff = 0;
    for (size_t i = 0; i < Nx; ++i) {
        const double xi = x.elemAsDouble(i);
        const double yi = paired ? y_or_m.elemAsDouble(i) : m0;
        if (std::isnan(xi) || std::isnan(yi)) continue;
        const double d = xi - yi;
        if (d == 0.0) continue;
        ++n_eff;
        if (d > 0.0) ++n_pos;
    }

    double p = 1.0;
    if (n_eff > 0) {
        // Binomial(n_eff, 0.5) tail probabilities via existing binocdf.
        Value kPos = Value::scalar(double(n_pos), mr);
        Value kPosM1 = Value::scalar(double(n_pos - 1), mr);
        const double cdfLE = binocdf(kPos, double(n_eff), 0.5, mr).toScalar();
        const double cdfLT = (n_pos > 0)
                           ? binocdf(kPosM1, double(n_eff), 0.5, mr).toScalar()
                           : 0.0;
        const double pLeft  = cdfLE;            // P(X ≤ n_pos)
        const double pRight = 1.0 - cdfLT;      // P(X ≥ n_pos)
        switch (tail) {
            case TestTail::Both:  p = std::min(1.0, 2.0 * std::min(pLeft, pRight)); break;
            case TestTail::Right: p = pRight; break;
            case TestTail::Left:  p = pLeft;  break;
        }
    }

    const int h = (p < alpha) ? 1 : 0;
    return std::make_tuple(Value::scalar(p, mr),
                           Value::scalar(double(h), mr),
                           Value::scalar(double(n_pos), mr));
}

// ════════════════════════════════════════════════════════════════════
// fishertest — Fisher's exact 2×2 contingency
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value, Value, Value>
fishertest(const Value &T, double alpha, TestTail tail, std::pmr::memory_resource *mr)
{
    if (T.dims().rows() != 2 || T.dims().cols() != 2)
        throw Error("fishertest: T must be a 2×2 matrix",
                    0, 0, "fishertest", "", "m:fishertest:size");
    const double a = T.elemAsDouble(0);  // (1,1)
    const double c = T.elemAsDouble(1);  // (2,1)
    const double b = T.elemAsDouble(2);  // (1,2)
    const double d = T.elemAsDouble(3);  // (2,2)
    const double row1 = a + b;
    const double col1 = a + c;
    const double Ntot = a + b + c + d;

    auto lb = [](double n, double k) {
        if (k < 0.0 || k > n) return -std::numeric_limits<double>::infinity();
        return std::lgamma(n + 1.0) - std::lgamma(k + 1.0) - std::lgamma(n - k + 1.0);
    };

    const long long kmin = static_cast<long long>(std::max(0.0, row1 + col1 - Ntot));
    const long long kmax = static_cast<long long>(std::min(row1, col1));
    const long long kobs = static_cast<long long>(a);
    const double logC_den = lb(Ntot, col1);
    const long long K = kmax - kmin + 1;
    std::vector<double> pmf(static_cast<size_t>(K));
    for (long long k = kmin; k <= kmax; ++k) {
        const double lp = lb(row1, double(k))
                        + lb(Ntot - row1, col1 - double(k))
                        - logC_den;
        pmf[static_cast<size_t>(k - kmin)] = std::exp(lp);
    }
    const double pObs = pmf[static_cast<size_t>(kobs - kmin)];

    double p = 0.0;
    switch (tail) {
        case TestTail::Both: {
            const double tol = pObs * (1.0 + 1e-9);
            for (size_t i = 0; i < pmf.size(); ++i)
                if (pmf[i] <= tol) p += pmf[i];
            break;
        }
        case TestTail::Right:
            for (long long k = kobs; k <= kmax; ++k)
                p += pmf[static_cast<size_t>(k - kmin)];
            break;
        case TestTail::Left:
            for (long long k = kmin; k <= kobs; ++k)
                p += pmf[static_cast<size_t>(k - kmin)];
            break;
    }
    if (p > 1.0) p = 1.0;
    const int h = (p < alpha) ? 1 : 0;

    const double OR = (b * c == 0.0)
                       ? std::numeric_limits<double>::infinity()
                       : (a * d) / (b * c);
    double ci_lo = 0.0;
    double ci_hi = std::numeric_limits<double>::infinity();
    if (a > 0 && b > 0 && c > 0 && d > 0) {
        const double logOR = std::log(OR);
        const double se = std::sqrt(1.0 / a + 1.0 / b + 1.0 / c + 1.0 / d);
        Value pcrit = Value::scalar(1.0 - alpha / 2.0, mr);
        const double zcrit = norminv(pcrit, 0.0, 1.0, mr).toScalar();
        ci_lo = std::exp(logOR - zcrit * se);
        ci_hi = std::exp(logOR + zcrit * se);
    }
    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p,        mr),
                           Value::scalar(OR,       mr),
                           Value::scalar(ci_lo,    mr),
                           Value::scalar(ci_hi,    mr));
}

// ════════════════════════════════════════════════════════════════════
// chi2gof — chi-squared goodness-of-fit (frequency form)
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value, Value>
chi2gof(const Value &observed, const Value &expected, int nparams, double alpha, std::pmr::memory_resource *mr)
{
    const size_t K = observed.numel();
    if (expected.numel() != K)
        throw Error("chi2gof: observed and expected must have same length",
                    0, 0, "chi2gof", "", "m:chi2gof:size");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (K < 2)
        return std::make_tuple(Value::scalar(nan, mr),
                               Value::scalar(0.0, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(0.0, mr));

    double chi2 = 0.0;
    for (size_t i = 0; i < K; ++i) {
        const double O = observed.elemAsDouble(i);
        const double E = expected.elemAsDouble(i);
        if (E <= 0.0) continue;
        const double d = O - E;
        chi2 += d * d / E;
    }
    const double df = double(K) - 1.0 - double(nparams);
    if (df <= 0.0)
        return std::make_tuple(Value::scalar(nan, mr),
                               Value::scalar(0.0, mr),
                               Value::scalar(chi2, mr),
                               Value::scalar(df, mr));

    Value xv = Value::scalar(chi2, mr);
    const double cdf = chi2cdf(xv, df, mr).toScalar();
    const double p = std::max(0.0, 1.0 - cdf);
    const int h = (p < alpha) ? 1 : 0;
    return std::make_tuple(Value::scalar(p, mr),
                           Value::scalar(double(h), mr),
                           Value::scalar(chi2, mr),
                           Value::scalar(df, mr));
}

// ════════════════════════════════════════════════════════════════════
// vartestn — k-sample variance equality test (5 variants)
// ════════════════════════════════════════════════════════════════════

namespace {

// Group buckets: parallel `ns` (per-group sample size) and `data`
// (concatenated values, with `offsets[i]..offsets[i+1]`).
struct Groups {
    ScratchVec<size_t>  ns;        // size k
    ScratchVec<size_t>  offsets;   // size k+1
    ScratchVec<double>  data;      // total N
    explicit Groups(std::pmr::memory_resource *mr)
        : ns(mr), offsets(mr), data(mr) {}
};

Groups bucket_by_group(const Value &x, const Value &group, std::pmr::memory_resource *scratch_mr)
{
    const size_t Nx = x.numel();
    Groups G(scratch_mr);
    // First pass: collect distinct labels (preserving first-encounter
    // order) + per-label counts; ignore NaN values/labels.
    ScratchVec<double> labels(scratch_mr);  // distinct group labels
    labels.reserve(8);
    ScratchVec<size_t> counts(scratch_mr);
    counts.reserve(8);
    ScratchVec<int> bucket_for(Nx, -1, scratch_mr);
    for (size_t i = 0; i < Nx; ++i) {
        const double xi = x.elemAsDouble(i);
        const double gi = group.elemAsDouble(i);
        if (std::isnan(xi) || std::isnan(gi)) continue;
        int b = -1;
        for (size_t j = 0; j < labels.size(); ++j)
            if (labels[j] == gi) { b = (int)j; break; }
        if (b < 0) {
            b = (int)labels.size();
            labels.push_back(gi);
            counts.push_back(0);
        }
        bucket_for[i] = b;
        ++counts[(size_t)b];
    }
    const size_t k = labels.size();
    G.ns.assign(counts.begin(), counts.end());
    G.offsets.resize(k + 1);
    G.offsets[0] = 0;
    for (size_t i = 0; i < k; ++i) G.offsets[i + 1] = G.offsets[i] + counts[i];
    G.data.resize(G.offsets[k]);
    ScratchVec<size_t> cursor(k, 0, scratch_mr);
    for (size_t i = 0; i < Nx; ++i) {
        const int b = bucket_for[i];
        if (b < 0) continue;
        G.data[G.offsets[b] + cursor[b]++] = x.elemAsDouble(i);
    }
    return G;
}

// One-way ANOVA on Z values stored in groups buckets. Returns
// (F, df1, df2, p).
struct AnovaOut { double F, df1, df2, p; };
AnovaOut anova1_on_groups(const ScratchVec<double> &Z, const ScratchVec<size_t> &offsets, const ScratchVec<size_t> &ns, std::pmr::memory_resource *mr)
{
    const size_t k = ns.size();
    size_t N = 0;
    for (size_t i = 0; i < k; ++i) N += ns[i];

    // Group means.
    ScratchVec<double> Zbar(k, 0.0, Z.get_allocator().resource());
    double grand = 0.0;
    for (size_t g = 0; g < k; ++g) {
        double s = 0.0;
        for (size_t j = offsets[g]; j < offsets[g + 1]; ++j) s += Z[j];
        Zbar[g] = (ns[g] > 0) ? s / double(ns[g]) : 0.0;
        grand += s;
    }
    grand /= double(N);

    double SSB = 0.0, SSW = 0.0;
    for (size_t g = 0; g < k; ++g) {
        const double db = Zbar[g] - grand;
        SSB += double(ns[g]) * db * db;
        for (size_t j = offsets[g]; j < offsets[g + 1]; ++j) {
            const double dw = Z[j] - Zbar[g];
            SSW += dw * dw;
        }
    }
    const double df1 = double(k - 1);
    const double df2 = double(N - k);
    const double MSB = SSB / df1;
    const double MSW = SSW / df2;
    const double F = (MSW > 0.0) ? MSB / MSW : 0.0;
    Value Fv = Value::scalar(F, mr);
    const double cdf = fcdf(Fv, df1, df2, mr).toScalar();
    return {F, df1, df2, std::max(0.0, 1.0 - cdf)};
}

inline double median_sorted(double *a, size_t n) {
    std::sort(a, a + n);
    if (n & 1u) return a[n / 2];
    return 0.5 * (a[n / 2 - 1] + a[n / 2]);
}

} // anonymous

// Public API. test = 0 Bartlett, 1 LeveneQuadratic, 2 LeveneAbsolute,
// 3 BrownForsythe, 4 OBrien.
std::tuple<Value, Value, Value, Value>
vartestn_full(const Value &x, const Value &group, int test, std::pmr::memory_resource *mr)
{
    const size_t Nx = x.numel();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (Nx == 0 || group.numel() != Nx)
        throw Error("vartestn: x and group must be same length",
                    0, 0, "vartestn", "", "m:vartestn:size");

    ScratchArena scratch(mr);
    Groups G = bucket_by_group(x, group, &scratch);
    // Drop groups with <2 observations (no sample variance / can't ANOVA).
    ScratchVec<size_t> ns(&scratch);   ns.reserve(G.ns.size());
    ScratchVec<size_t> offsets(&scratch); offsets.push_back(0);
    ScratchVec<double> data(&scratch); data.reserve(G.data.size());
    for (size_t g = 0; g < G.ns.size(); ++g) {
        if (G.ns[g] < 2) continue;
        ns.push_back(G.ns[g]);
        for (size_t j = G.offsets[g]; j < G.offsets[g + 1]; ++j)
            data.push_back(G.data[j]);
        offsets.push_back(data.size());
    }
    const size_t k = ns.size();
    if (k < 2) {
        return std::make_tuple(Value::scalar(nan, mr),
                               Value::scalar(0.0, mr),
                               Value::scalar(double(k > 0 ? k - 1 : 0), mr),
                               Value::scalar(nan, mr));
    }
    size_t N = 0;
    for (auto n_i : ns) N += n_i;

    // Per-group mean / median / variance.
    ScratchVec<double> means(k, 0.0, &scratch);
    ScratchVec<double> meds(k,  0.0, &scratch);
    ScratchVec<double> vars(k,  0.0, &scratch);
    for (size_t g = 0; g < k; ++g) {
        const size_t lo = offsets[g], hi = offsets[g + 1];
        double s = 0.0;
        for (size_t j = lo; j < hi; ++j) s += data[j];
        means[g] = s / double(ns[g]);
        double s2 = 0.0;
        for (size_t j = lo; j < hi; ++j) {
            const double d = data[j] - means[g]; s2 += d * d;
        }
        vars[g] = s2 / double(ns[g] - 1);
        // Median: copy + nth_element. Use scratch sort.
        ScratchVec<double> tmp(data.begin() + lo, data.begin() + hi, &scratch);
        meds[g] = median_sorted(tmp.data(), tmp.size());
    }

    if (test == 0) {
        // Bartlett.
        double Sp2_num = 0.0;
        for (size_t i = 0; i < k; ++i) Sp2_num += double(ns[i] - 1) * vars[i];
        const double Sp2 = Sp2_num / double(N - k);
        double Q = double(N - k) * std::log(Sp2);
        for (size_t i = 0; i < k; ++i) Q -= double(ns[i] - 1) * std::log(vars[i]);
        double inv_sum = 0.0;
        for (size_t i = 0; i < k; ++i) inv_sum += 1.0 / double(ns[i] - 1);
        inv_sum -= 1.0 / double(N - k);
        const double C = 1.0 + inv_sum / (3.0 * double(k - 1));
        const double chisq = Q / C;
        const double df = double(k - 1);
        Value xv = Value::scalar(chisq, mr);
        const double cdf = chi2cdf(xv, df, mr).toScalar();
        const double p = std::max(0.0, 1.0 - cdf);
        return std::make_tuple(Value::scalar(p, mr),
                               Value::scalar(chisq, mr),
                               Value::scalar(df, mr),
                               Value::scalar(nan, mr));
    }

    // F-based tests: build Z values per observation.
    ScratchVec<double> Z(data.size(), &scratch);
    for (size_t g = 0; g < k; ++g) {
        const size_t lo = offsets[g], hi = offsets[g + 1];
        for (size_t j = lo; j < hi; ++j) {
            const double v = data[j];
            switch (test) {
                case 1: { // LeveneQuadratic
                    const double d = v - means[g]; Z[j] = d * d; break;
                }
                case 2: { // LeveneAbsolute
                    Z[j] = std::fabs(v - means[g]); break;
                }
                case 3: { // BrownForsythe
                    Z[j] = std::fabs(v - meds[g]); break;
                }
                case 4: { // OBrien
                    const double n = double(ns[g]);
                    const double dev = (v - means[g]) * (v - means[g]);
                    Z[j] = ((n - 1.5) * n * dev - 0.5 * vars[g] * (n - 1.0))
                           / ((n - 1.0) * (n - 2.0));
                    break;
                }
                default:
                    Z[j] = 0.0;
            }
        }
    }
    auto A = anova1_on_groups(Z, offsets, ns, mr);
    return std::make_tuple(Value::scalar(A.p, mr),
                           Value::scalar(A.F, mr),
                           Value::scalar(A.df1, mr),
                           Value::scalar(A.df2, mr));
}

std::tuple<Value, Value, Value>
vartestn(const Value &x, const Value &group, double /*alpha*/, std::pmr::memory_resource *mr)
{
    auto [p, stat, df1, df2] = vartestn_full(x, group, /*Bartlett*/ 0, mr);
    (void)df2;
    return std::make_tuple(std::move(p), std::move(stat), std::move(df1));
}

// ════════════════════════════════════════════════════════════════════
// runstest — Wald-Wolfowitz runs test for randomness
// ════════════════════════════════════════════════════════════════════

namespace {

inline double log_binom(double n, double k) {
    if (k < 0.0 || k > n) return -std::numeric_limits<double>::infinity();
    return std::lgamma(n + 1.0) - std::lgamma(k + 1.0) - std::lgamma(n - k + 1.0);
}

// Exact P(R = r) under H0 for a sequence with n1 ones and n0 zeros.
// r in [1, n1+n0]; returns 0 outside the support.
double exact_runs_pmf(int r, int n1, int n0)
{
    if (r < 2) return 0.0;
    const double logC = log_binom(double(n1 + n0), double(n1));
    if (r % 2 == 0) {
        const int k = r / 2;
        if (k < 1 || k > n1 || k > n0) return 0.0;
        const double lp = std::log(2.0) + log_binom(double(n1 - 1), double(k - 1))
                        + log_binom(double(n0 - 1), double(k - 1)) - logC;
        return std::exp(lp);
    }
    const int k = (r - 1) / 2;            // r = 2k+1
    double t1 = -std::numeric_limits<double>::infinity();
    double t2 = -std::numeric_limits<double>::infinity();
    // Start with 1: (k+1) ones-blocks, k zero-blocks → C(n1-1, k)·C(n0-1, k-1)
    if (k <= n1 - 1 && k - 1 >= 0 && k - 1 <= n0 - 1)
        t1 = log_binom(double(n1 - 1), double(k))
           + log_binom(double(n0 - 1), double(k - 1));
    // Start with 0: k ones-blocks, (k+1) zero-blocks → C(n1-1, k-1)·C(n0-1, k)
    if (k - 1 >= 0 && k - 1 <= n1 - 1 && k <= n0 - 1)
        t2 = log_binom(double(n1 - 1), double(k - 1))
           + log_binom(double(n0 - 1), double(k));
    if (std::isinf(t1) && std::isinf(t2)) return 0.0;
    const double lmax = std::max(t1, t2);
    const double lsum = lmax + std::log(
          (std::isinf(t1) ? 0.0 : std::exp(t1 - lmax))
        + (std::isinf(t2) ? 0.0 : std::exp(t2 - lmax)));
    return std::exp(lsum - logC);
}

} // anonymous

std::tuple<Value, Value, Value, Value, Value, Value>
runstest(const Value &x, double v_in, double alpha, TestTail tail, const std::string &method_in, std::pmr::memory_resource *mr)
{
    const size_t Nx = x.numel();

    // Default v = median(x).
    double v = v_in;
    if (std::isnan(v_in)) {
        std::vector<double> sorted;
        sorted.reserve(Nx);
        for (size_t i = 0; i < Nx; ++i) {
            const double xi = x.elemAsDouble(i);
            if (!std::isnan(xi)) sorted.push_back(xi);
        }
        std::sort(sorted.begin(), sorted.end());
        const size_t M = sorted.size();
        if (M == 0) v = 0.0;
        else if (M % 2 == 1) v = sorted[M / 2];
        else                 v = 0.5 * (sorted[M / 2 - 1] + sorted[M / 2]);
    }

    // Build binary sequence; drop values equal to v.
    std::vector<int> bin;
    bin.reserve(Nx);
    for (size_t i = 0; i < Nx; ++i) {
        const double xi = x.elemAsDouble(i);
        if (std::isnan(xi)) continue;
        if (xi > v)      bin.push_back(1);
        else if (xi < v) bin.push_back(0);
        // == v → drop
    }
    int n1 = 0, n0 = 0;
    for (int b : bin) (b ? n1 : n0) += 1;
    int R = 0;
    int prev = -1;
    for (int b : bin) { if (b != prev) { ++R; prev = b; } }

    const auto retNan = [&]() {
        return std::make_tuple(Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr),
                               Value::scalar(0.0, mr),
                               Value::scalar(double(R),  mr),
                               Value::scalar(double(n1), mr),
                               Value::scalar(double(n0), mr),
                               Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr));
    };
    if (n1 == 0 || n0 == 0) return retNan();

    std::string method = method_in;
    for (auto &c : method) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    bool exact;
    if      (method == "approximate") exact = false;
    else                              exact = true;  // MATLAB default

    double p = 1.0;
    double zval = std::numeric_limits<double>::quiet_NaN();

    if (exact) {
        // P(R ≤ R_obs) and P(R ≥ R_obs) by enumeration.
        const int N = n1 + n0;
        double cdfLE = 0.0, cdfGE = 0.0;
        for (int r = 2; r <= N; ++r) {
            const double pr = exact_runs_pmf(r, n1, n0);
            if (r <= R) cdfLE += pr;
            if (r >= R) cdfGE += pr;
        }
        switch (tail) {
            case TestTail::Both:  p = std::min(1.0, 2.0 * std::min(cdfLE, cdfGE)); break;
            case TestTail::Right: p = cdfGE; break;
            case TestTail::Left:  p = cdfLE; break;
        }
    } else {
        const double Nd = double(n1 + n0);
        const double mean = 2.0 * n1 * n0 / Nd + 1.0;
        const double var  = 2.0 * n1 * n0 * (2.0 * n1 * n0 - Nd) / (Nd * Nd * (Nd - 1.0));
        const double sd = std::sqrt(std::max(var, 0.0));
        double cc = 0.0;
        if      (R > mean) cc = +0.5;
        else if (R < mean) cc = -0.5;
        zval = (sd > 0.0) ? (R - mean - cc) / sd : 0.0;
        Value zV = Value::scalar(zval, mr);
        const double cdf = normcdf(zV, 0.0, 1.0, mr).toScalar();
        switch (tail) {
            case TestTail::Both:  p = 2.0 * std::min(cdf, 1.0 - cdf); break;
            case TestTail::Right: p = 1.0 - cdf; break;
            case TestTail::Left:  p = cdf; break;
        }
    }

    const int h = (p < alpha) ? 1 : 0;
    return std::make_tuple(Value::scalar(p, mr),
                           Value::scalar(double(h), mr),
                           Value::scalar(double(R),  mr),
                           Value::scalar(double(n1), mr),
                           Value::scalar(double(n0), mr),
                           Value::scalar(zval, mr));
}

// ════════════════════════════════════════════════════════════════════
// ranksum — Wilcoxon rank-sum (Mann-Whitney U)
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value, Value>
ranksum(const Value &x, const Value &y, double alpha, TestTail tail, const std::string &method_in, std::pmr::memory_resource *mr)
{
    const size_t nx = x.numel();
    const size_t ny = y.numel();
    const size_t N = nx + ny;
    if (N == 0) {
        return std::make_tuple(Value::scalar(1.0, mr),
                               Value::scalar(0.0, mr),
                               Value::scalar(0.0, mr),
                               Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr));
    }

    // Combine: store (value, is_x). NaN values are dropped.
    struct Item { double v; bool is_x; };
    std::vector<Item> items;
    items.reserve(N);
    for (size_t i = 0; i < nx; ++i) {
        const double v = x.elemAsDouble(i);
        if (!std::isnan(v)) items.push_back({v, true});
    }
    for (size_t i = 0; i < ny; ++i) {
        const double v = y.elemAsDouble(i);
        if (!std::isnan(v)) items.push_back({v, false});
    }
    const size_t Neff = items.size();
    size_t nx_eff = 0, ny_eff = 0;
    for (auto &it : items) (it.is_x ? nx_eff : ny_eff) += 1;
    if (Neff == 0 || nx_eff == 0 || ny_eff == 0) {
        // Degenerate: no comparison possible.
        return std::make_tuple(Value::scalar(1.0, mr),
                               Value::scalar(0.0, mr),
                               Value::scalar(0.0, mr),
                               Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr));
    }

    // Sort + assign mid-ranks.
    std::vector<size_t> ord(Neff);
    for (size_t i = 0; i < Neff; ++i) ord[i] = i;
    std::sort(ord.begin(), ord.end(),
              [&](size_t a, size_t b) { return items[a].v < items[b].v; });

    std::vector<double> ranks(Neff);
    std::vector<size_t> tieGroupSizes;
    size_t i = 0;
    while (i < Neff) {
        size_t j = i + 1;
        while (j < Neff && items[ord[j]].v == items[ord[i]].v) ++j;
        const double avg = static_cast<double>(i + j + 1) / 2.0;  // mean of (i+1)..j
        for (size_t k = i; k < j; ++k) ranks[ord[k]] = avg;
        if (j - i > 1) tieGroupSizes.push_back(j - i);
        i = j;
    }

    // W_x = sum of x's ranks.
    double Wx = 0.0;
    for (size_t k = 0; k < Neff; ++k)
        if (items[k].is_x) Wx += ranks[k];

    // Choose method.
    std::string method = method_in;
    for (auto &c : method) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    bool exact;
    if      (method == "exact")       exact = true;
    else if (method == "approximate") exact = false;
    else                              exact = (nx_eff < 10 && ny_eff < 10);  // MATLAB default

    double p = 1.0;
    double zval = std::numeric_limits<double>::quiet_NaN();

    if (exact) {
        // 2D DP: dp[k][s] = # size-k subsets of (2·ranks) summing to s.
        std::vector<long long> rk2(Neff);
        long long total2 = 0;
        for (size_t k = 0; k < Neff; ++k) {
            rk2[k] = static_cast<long long>(std::llround(ranks[k] * 2.0));
            total2 += rk2[k];
        }
        const size_t S = static_cast<size_t>(total2 + 1);
        std::vector<std::vector<double>> dp(nx_eff + 1, std::vector<double>(S, 0.0));
        dp[0][0] = 1.0;
        for (size_t k = 0; k < Neff; ++k) {
            const long long r = rk2[k];
            for (long long kk = static_cast<long long>(nx_eff) - 1; kk >= 0; --kk) {
                for (long long s = static_cast<long long>(S) - 1; s >= r; --s)
                    dp[kk + 1][s] += dp[kk][s - r];
            }
        }
        const long long Wx2 = static_cast<long long>(std::llround(Wx * 2.0));
        double total = 0.0;
        for (size_t s = 0; s < S; ++s) total += dp[nx_eff][s];
        double cdfLE = 0.0, cdfGE = 0.0;
        for (long long s = 0; s <= Wx2 && s < static_cast<long long>(S); ++s)
            cdfLE += dp[nx_eff][s];
        for (long long s = Wx2; s < static_cast<long long>(S); ++s)
            cdfGE += dp[nx_eff][s];
        cdfLE /= total;
        cdfGE /= total;
        switch (tail) {
            case TestTail::Both:  p = std::min(1.0, 2.0 * std::min(cdfLE, cdfGE)); break;
            case TestTail::Right: p = cdfGE; break;
            case TestTail::Left:  p = cdfLE; break;
        }
    } else {
        const double Nd = static_cast<double>(Neff);
        const double mean = static_cast<double>(nx_eff) * (Nd + 1) / 2.0;
        double var = static_cast<double>(nx_eff) * static_cast<double>(ny_eff)
                     * (Nd + 1) / 12.0;
        if (Nd > 1.0) {
            double tieSum = 0.0;
            for (size_t t : tieGroupSizes) {
                const double td = static_cast<double>(t);
                tieSum += td * td * td - td;
            }
            var -= static_cast<double>(nx_eff) * static_cast<double>(ny_eff)
                   / (12.0 * Nd * (Nd - 1.0)) * tieSum;
        }
        const double sd = std::sqrt(std::max(var, 0.0));
        // Continuity correction: shift W toward mean by 0.5.
        double cc = 0.0;
        if      (Wx > mean) cc = +0.5;
        else if (Wx < mean) cc = -0.5;
        zval = (sd > 0.0) ? (Wx - mean - cc) / sd : 0.0;
        Value zV = Value::scalar(zval, mr);
        const double cdf = normcdf(zV, 0.0, 1.0, mr).toScalar();
        switch (tail) {
            case TestTail::Both:  p = 2.0 * std::min(cdf, 1.0 - cdf); break;
            case TestTail::Right: p = 1.0 - cdf; break;
            case TestTail::Left:  p = cdf; break;
        }
    }

    const int h = (p < alpha) ? 1 : 0;
    return std::make_tuple(Value::scalar(p, mr),
                           Value::scalar(double(h), mr),
                           Value::scalar(Wx, mr),
                           Value::scalar(zval, mr));
}

// ════════════════════════════════════════════════════════════════════
// signrank — Wilcoxon signed-rank
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value, Value>
signrank(const Value &x, const Value &y_or_m, double alpha, TestTail tail, const std::string &method_in, std::pmr::memory_resource *mr)
{
    const size_t Nx = x.numel();
    const bool paired = (!y_or_m.isEmpty() && !y_or_m.isScalar());
    if (paired && y_or_m.numel() != Nx)
        throw Error("signrank: X and Y must have the same length",
                    0, 0, "signrank", "", "m:signrank:size");
    const double m0 = (paired || y_or_m.isEmpty()) ? 0.0 : y_or_m.toScalar();

    // Collect (|d|, sign) for non-zero diffs.
    struct DSign { double absd; int sign; };
    std::vector<DSign> ds;
    ds.reserve(Nx);
    for (size_t i = 0; i < Nx; ++i) {
        const double xi = x.elemAsDouble(i);
        const double yi = paired ? y_or_m.elemAsDouble(i) : m0;
        if (std::isnan(xi) || std::isnan(yi)) continue;
        const double d = xi - yi;
        if (d == 0.0) continue;
        ds.push_back({std::fabs(d), d > 0 ? +1 : -1});
    }
    const size_t n = ds.size();

    // Edge case: nothing to rank.
    if (n == 0) {
        return std::make_tuple(Value::scalar(1.0, mr),
                               Value::scalar(0.0, mr),
                               Value::scalar(0.0, mr),
                               Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr));
    }

    // Sort by |d| ascending; assign mid-ranks for ties.
    std::vector<size_t> ord(n);
    for (size_t i = 0; i < n; ++i) ord[i] = i;
    std::sort(ord.begin(), ord.end(),
              [&](size_t a, size_t b) { return ds[a].absd < ds[b].absd; });

    std::vector<double> ranks(n);
    std::vector<size_t> tieGroupSizes;
    size_t i = 0;
    while (i < n) {
        size_t j = i + 1;
        while (j < n && ds[ord[j]].absd == ds[ord[i]].absd) ++j;
        const double avg = (static_cast<double>(i + j + 1)) / 2.0;  // mean of (i+1)..j
        for (size_t k = i; k < j; ++k) ranks[ord[k]] = avg;
        if (j - i > 1) tieGroupSizes.push_back(j - i);
        i = j;
    }

    // W+ = sum of ranks where d > 0.
    double Wplus = 0.0;
    for (size_t k = 0; k < n; ++k)
        if (ds[k].sign > 0) Wplus += ranks[k];

    // Choose method.
    std::string method = method_in;
    for (auto &c : method) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    bool exact;
    if      (method == "exact")       exact = true;
    else if (method == "approximate") exact = false;
    else                              exact = (n <= 15);  // MATLAB default

    double p = 1.0;
    double zval = std::numeric_limits<double>::quiet_NaN();

    if (exact) {
        // Convolve subset-sum count polynomial. Scale ranks by 2 so all
        // (mid-rank averaged) values are integers.
        std::vector<long long> rk2(n);
        long long total2 = 0;
        for (size_t k = 0; k < n; ++k) {
            rk2[k] = static_cast<long long>(std::llround(ranks[k] * 2.0));
            total2 += rk2[k];
        }
        std::vector<double> P(static_cast<size_t>(total2 + 1), 0.0);
        P[0] = 1.0;
        long long cur = 0;
        for (size_t k = 0; k < n; ++k) {
            const long long r = rk2[k];
            for (long long s = cur + r; s >= r; --s) P[s] += P[s - r];
            cur += r;
        }
        const long long W2 = static_cast<long long>(std::llround(Wplus * 2.0));
        const double total = std::pow(2.0, static_cast<double>(n));
        // P(W ≤ W+) and P(W ≥ W+).
        double cdfLE = 0.0;
        for (long long s = 0; s <= W2; ++s) cdfLE += P[s];
        cdfLE /= total;
        double cdfGE = 0.0;
        for (long long s = W2; s <= total2; ++s) cdfGE += P[s];
        cdfGE /= total;
        switch (tail) {
            case TestTail::Both:  p = std::min(1.0, 2.0 * std::min(cdfLE, cdfGE)); break;
            case TestTail::Right: p = cdfGE; break;
            case TestTail::Left:  p = cdfLE; break;
        }
    } else {
        // Normal approximation with tie correction.
        const double mean = static_cast<double>(n) * (n + 1) / 4.0;
        double var = static_cast<double>(n) * (n + 1) * (2 * n + 1) / 24.0;
        for (size_t t : tieGroupSizes)
            var -= static_cast<double>(t * t * t - t) / 48.0;
        const double sd = std::sqrt(var);
        zval = (sd > 0.0) ? (Wplus - mean) / sd : 0.0;
        Value zV = Value::scalar(zval, mr);
        const double cdf = normcdf(zV, 0.0, 1.0, mr).toScalar();
        switch (tail) {
            case TestTail::Both:  p = 2.0 * std::min(cdf, 1.0 - cdf); break;
            case TestTail::Right: p = 1.0 - cdf; break;
            case TestTail::Left:  p = cdf; break;
        }
    }

    const int h = (p < alpha) ? 1 : 0;
    return std::make_tuple(Value::scalar(p, mr),
                           Value::scalar(double(h), mr),
                           Value::scalar(Wplus, mr),
                           Value::scalar(zval, mr));
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

namespace {
TestTail parse_tail_arg(Span<const Value> args, size_t i, TestTail def) {
    if (i >= args.size()) return def;
    const Value &v = args[i];
    if (v.isChar() || v.isString()) return parse_tail(v.toString(), def);
    return def;
}

double parse_alpha(Span<const Value> args, size_t i, double def) {
    if (i >= args.size() || args[i].isEmpty()) return def;
    if (args[i].isChar() || args[i].isString()) return def;
    return args[i].toScalar();
}
}

void ttest_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ttest: requires (X[, m | y][, alpha, tail | name-value])",
                    0, 0, "ttest", "", "m:ttest:nargin");
    auto *mr = ctx.engine->resource();

    // Detect paired form: ttest(x, y) where y is a non-scalar numeric
    // vector matching x's length. Pre-difference x - y and run vs m=0.
    Value xData = args[0];
    bool pairedConsumed = false;
    if (args.size() >= 2 && !args[1].isChar() && !args[1].isString()
        && !args[1].isEmpty() && args[1].numel() > 1) {
        // Build paired difference x - y in a fresh DOUBLE row.
        const Value &y = args[1];
        if (y.numel() != args[0].numel())
            throw Error("ttest: paired vectors must have equal length",
                        0, 0, "ttest", "", "m:ttest:pairedLen");
        Value diff = Value::matrix(1, y.numel(), ValueType::DOUBLE, mr);
        double *dst = diff.doubleDataMut();
        for (size_t i = 0; i < y.numel(); ++i)
            dst[i] = args[0].elemAsDouble(i) - y.elemAsDouble(i);
        xData = std::move(diff);
        pairedConsumed = true;
    }

    double m = 0.0;
    if (!pairedConsumed && args.size() >= 2 && !args[1].isChar()
        && !args[1].isString() && !args[1].isEmpty()) {
        m = args[1].toScalar();
    }
    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    size_t i = (args.size() >= 2 && (args[1].isChar() || args[1].isString())) ? 1 : 2;
    while (i < args.size()) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            std::string sl = a.toString();
            for (auto &c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (sl == "alpha" && i + 1 < args.size()) {
                alpha = args[i + 1].toScalar(); i += 2;
            } else if (sl == "tail" && i + 1 < args.size()) {
                tail = parse_tail(args[i + 1].toString(), tail); i += 2;
            } else if (sl == "dim") {
                throw Error("ttest: 'Dim' not yet supported (parity gap)",
                            0, 0, "ttest", "", "m:ttest:dim");
            } else {
                tail = parse_tail(sl, tail); ++i;
            }
        } else {
            alpha = a.toScalar(); ++i;
        }
    }
    auto [h, p, ci, t] = ttest(xData, m, alpha, tail, mr);
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(ci);
    if (nargout > 3) outs[3] = std::move(t);   // scalar tstat (struct form deferred)
}

void ttest2_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ttest2: requires (X, Y[, alpha, tail, vartype])",
                    0, 0, "ttest2", "", "m:ttest2:nargin");
    double alpha = parse_alpha(args, 2, 0.05);
    TestTail tail = TestTail::Both;
    // MATLAB R2025b default is 'equal' (pooled variance), NOT Welch.
    std::string vartype = "equal";
    for (size_t i = 2; i + 1 < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            std::string k = args[i].toString();
            for (auto &c : k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if      (k == "tail")    tail = parse_tail(args[i + 1].toString(), TestTail::Both);
            else if (k == "vartype") vartype = args[i + 1].toString();
            else if (k == "alpha")   alpha = args[i + 1].toScalar();
            else if (k == "dim")
                throw Error("ttest2: 'Dim' not yet supported (parity gap)",
                            0, 0, "ttest2", "", "m:ttest2:dim");
        }
    }
    auto [h, p, ci, t] = ttest2(args[0], args[1], alpha, tail, vartype, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(ci);
    if (nargout > 3) outs[3] = std::move(t);
}

void ztest_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ztest: requires (X, m, sigma[, alpha, tail | name-value])",
                    0, 0, "ztest", "", "m:ztest:nargin");
    const double m     = args[1].toScalar();
    const double sigma = args[2].toScalar();
    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    size_t i = 3;
    while (i < args.size()) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            std::string sl = a.toString();
            for (auto &c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (sl == "alpha" && i + 1 < args.size()) { alpha = args[i + 1].toScalar(); i += 2; }
            else if (sl == "tail" && i + 1 < args.size()) { tail = parse_tail(args[i + 1].toString(), tail); i += 2; }
            else if (sl == "dim")
                throw Error("ztest: 'Dim' not yet supported (parity gap)",
                            0, 0, "ztest", "", "m:ztest:dim");
            else { tail = parse_tail(sl, tail); ++i; }
        } else { alpha = a.toScalar(); ++i; }
    }
    auto [h, p, ci, z] = ztest(args[0], m, sigma, alpha, tail, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(ci);
    if (nargout > 3) outs[3] = std::move(z);
}

void vartest_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("vartest: requires (X, v[, alpha, tail | name-value])",
                    0, 0, "vartest", "", "m:vartest:nargin");
    const double v = args[1].toScalar();
    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    size_t i = 2;
    while (i < args.size()) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            std::string sl = a.toString();
            for (auto &c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (sl == "alpha" && i + 1 < args.size()) { alpha = args[i + 1].toScalar(); i += 2; }
            else if (sl == "tail" && i + 1 < args.size()) { tail = parse_tail(args[i + 1].toString(), tail); i += 2; }
            else if (sl == "dim")
                throw Error("vartest: 'Dim' not yet supported (parity gap)",
                            0, 0, "vartest", "", "m:vartest:dim");
            else { tail = parse_tail(sl, tail); ++i; }
        } else { alpha = a.toScalar(); ++i; }
    }
    auto [h, p, ci, T] = vartest(args[0], v, alpha, tail, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(ci);
    if (nargout > 3) outs[3] = std::move(T);
}

void vartest2_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("vartest2: requires (X, Y[, alpha, tail | name-value])",
                    0, 0, "vartest2", "", "m:vartest2:nargin");
    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    size_t i = 2;
    while (i < args.size()) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            std::string sl = a.toString();
            for (auto &c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (sl == "alpha" && i + 1 < args.size()) { alpha = args[i + 1].toScalar(); i += 2; }
            else if (sl == "tail" && i + 1 < args.size()) { tail = parse_tail(args[i + 1].toString(), tail); i += 2; }
            else if (sl == "dim")
                throw Error("vartest2: 'Dim' not yet supported (parity gap)",
                            0, 0, "vartest2", "", "m:vartest2:dim");
            else { tail = parse_tail(sl, tail); ++i; }
        } else { alpha = a.toScalar(); ++i; }
    }
    auto [h, p, ci, F] = vartest2(args[0], args[1], alpha, tail, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(ci);
    if (nargout > 3) outs[3] = std::move(F);
}

void kstest_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("kstest: requires X", 0, 0, "kstest", "",
                    "m:kstest:nargin");
    Value cdf = (args.size() >= 2 && !(args[1].isChar() || args[1].isString()))
                  ? args[1] : Value();  // empty default
    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    // Walk trailing args: positional alpha (numeric scalar), positional
    // tail string, and Name-Value pairs ('Alpha', value | 'Tail', value).
    size_t i = (cdf.numel() > 0 || (args.size() >= 2 && (args[1].isChar() || args[1].isString()))) ? 2 : 1;
    while (i < args.size()) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            std::string s = a.toString();
            std::string sl = s;
            for (auto &c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (sl == "alpha" && i + 1 < args.size()) {
                alpha = args[i + 1].toScalar();
                i += 2;
            } else if (sl == "tail" && i + 1 < args.size()) {
                tail = parse_tail(args[i + 1].toString(), tail);
                i += 2;
            } else {
                // positional tail string
                tail = parse_tail(sl, tail);
                ++i;
            }
        } else {
            // positional alpha
            alpha = a.toScalar();
            ++i;
        }
    }
    auto [h, p, D, cv] = kstest(args[0], cdf, alpha, tail, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(D);
    if (nargout > 3) outs[3] = std::move(cv);
}

void kstest2_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("kstest2: requires (X, Y[, alpha, tail | name-value])",
                    0, 0, "kstest2", "", "m:kstest2:nargin");
    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    size_t i = 2;
    while (i < args.size()) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            std::string sl = a.toString();
            for (auto &c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (sl == "alpha" && i + 1 < args.size()) {
                alpha = args[i + 1].toScalar(); i += 2;
            } else if (sl == "tail" && i + 1 < args.size()) {
                tail = parse_tail(args[i + 1].toString(), tail); i += 2;
            } else {
                tail = parse_tail(sl, tail); ++i;
            }
        } else {
            alpha = a.toScalar(); ++i;
        }
    }
    auto [h, p, D, cv] = kstest2(args[0], args[1], alpha, tail, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(D);
    if (nargout > 3) outs[3] = std::move(cv);
}

void jbtest_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("jbtest: requires X[, alpha[, mctol]]", 0, 0, "jbtest", "",
                    "m:jbtest:nargin");
    double alpha = parse_alpha(args, 1, 0.05);
    // 3rd arg = mctol (Monte-Carlo standard-error tolerance).
    const double mctol = (args.size() > 2 && !args[2].isEmpty())
                         ? args[2].toScalar()
                         : std::numeric_limits<double>::quiet_NaN();
    auto [h, p, JB, cv] = jbtest(args[0], alpha, mctol, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(JB);
    if (nargout > 3) outs[3] = std::move(cv);
}

void fishertest_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fishertest: requires (T[, alpha, tail | name-value])",
                    0, 0, "fishertest", "", "m:fishertest:nargin");
    auto *mr = ctx.engine->resource();
    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const Value &v = args[i + 1];
        if      (name == "alpha") alpha = v.toScalar();
        else if (name == "tail")  tail  = parse_tail(v.toString(), TestTail::Both);
    }
    auto [h, p, OR, lo, hi] = fishertest(args[0], alpha, tail, mr);
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        s.field("OddsRatio") = OR;
        Value ci = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        double *cd = ci.doubleDataMut();
        cd[0] = lo.toScalar();
        cd[1] = hi.toScalar();
        s.field("ConfidenceInterval") = std::move(ci);
        outs[2] = std::move(s);
    }
}

void chi2gof_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("chi2gof: requires X[, 'Frequency'/'Expected'/'Edges'/"
                    "'NBins'/'Ctrs'/'NParams'/'EMin'/'Alpha', val, ...]",
                    0, 0, "chi2gof", "", "m:chi2gof:nargin");
    auto *mr = ctx.engine->resource();
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };

    Value freq, expected, edges_arg, ctrs_arg;
    int nbins = 10;
    int nparams = -1;       // -1 = use default (2 if auto-fit, 0 if explicit O/E)
    double alpha = 0.05;
    double emin = 5.0;

    bool freq_set = false, expected_set = false, edges_set = false;
    bool nbins_set = false, ctrs_set = false, nparams_set = false;
    bool cdf_supplied = false;

    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString()) break;
        const std::string name = lower(args[i].toString());
        const Value &v = args[i + 1];
        if      (name == "frequency") { freq = v;     freq_set = true; }
        else if (name == "expected")  { expected = v; expected_set = true; }
        else if (name == "edges")     { edges_arg = v; edges_set = true; }
        else if (name == "nbins")     { nbins = (int)v.toScalar(); nbins_set = true; }
        else if (name == "ctrs")      { ctrs_arg = v; ctrs_set = true; }
        else if (name == "nparams")   { nparams = (int)v.toScalar(); nparams_set = true; }
        else if (name == "emin")      { emin = v.toScalar(); }
        else if (name == "alpha")     { alpha = v.toScalar(); }
        else if (name == "cdf")       { cdf_supplied = true; }
    }

    if (cdf_supplied)
        throw Error("chi2gof: 'CDF' function-handle argument is not yet "
                    "supported in numkit; supply 'Expected' or rely on "
                    "the default normal auto-fit instead",
                    0, 0, "chi2gof", "", "m:chi2gof:cdf_nyi");

    // Path A: explicit Frequency + Expected (existing behavior).
    if (freq_set && expected_set) {
        const int np = nparams_set ? nparams : 0;
        auto [p, h, chi2, df] = chi2gof(freq, expected, np, alpha, mr);
        outs[0] = std::move(h);
        if (nargout > 1) outs[1] = std::move(p);
        if (nargout > 2) {
            const size_t K = freq.numel();
            Value s = Value::structure(mr);
            s.field("chi2stat") = chi2;
            s.field("df")       = df;
            // Synthesize edges from the first arg if it's monotone numeric;
            // else default to 1:K with width 1.
            Value edges_out = Value::matrix(1, K + 1, ValueType::DOUBLE, mr);
            double *ep = edges_out.doubleDataMut();
            const Value &xv = args[0];
            const double x0 = xv.elemAsDouble(0);
            const double xK = xv.elemAsDouble(K - 1);
            const double dx = (K > 1) ? (xK - x0) / double(K - 1) : 1.0;
            for (size_t i = 0; i <= K; ++i) ep[i] = x0 + (double(i) - 0.5) * dx;
            s.field("edges") = edges_out;
            s.field("O")     = freq;
            s.field("E")     = expected;
            outs[2] = std::move(s);
        }
        return;
    }

    // Path B: auto-binning. Need data vector.
    const Value &x = args[0];
    const size_t N = x.numel();
    if (N < 2)
        throw Error("chi2gof: data vector must have at least 2 elements",
                    0, 0, "chi2gof", "", "m:chi2gof:size");

    ScratchArena scratch(mr);

    // Compute mean / std of x.
    double mean = 0.0;
    for (size_t i = 0; i < N; ++i) mean += x.elemAsDouble(i);
    mean /= double(N);
    double sq = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double d = x.elemAsDouble(i) - mean;
        sq += d * d;
    }
    const double sd = std::sqrt(sq / double(N - 1));

    // Build edges.
    ScratchVec<double> edges(&scratch);
    if (edges_set) {
        const size_t M = edges_arg.numel();
        edges.resize(M);
        for (size_t i = 0; i < M; ++i) edges[i] = edges_arg.elemAsDouble(i);
    } else if (ctrs_set) {
        // Centres → derive edges as midpoints + extrapolation.
        const size_t M = ctrs_arg.numel();
        edges.resize(M + 1);
        const double c0 = ctrs_arg.elemAsDouble(0);
        const double c1 = ctrs_arg.elemAsDouble(1);
        edges[0] = c0 - 0.5 * (c1 - c0);
        for (size_t i = 0; i + 1 < M; ++i) {
            edges[i + 1] = 0.5 * (ctrs_arg.elemAsDouble(i)
                                  + ctrs_arg.elemAsDouble(i + 1));
        }
        const double cN1 = ctrs_arg.elemAsDouble(M - 1);
        const double cN2 = ctrs_arg.elemAsDouble(M - 2);
        edges[M] = cN1 + 0.5 * (cN1 - cN2);
    } else {
        // NBins equally-spaced from min(x) to max(x).
        double xmin = x.elemAsDouble(0), xmax = xmin;
        for (size_t i = 1; i < N; ++i) {
            const double v = x.elemAsDouble(i);
            if (v < xmin) xmin = v;
            if (v > xmax) xmax = v;
        }
        const int K = std::max(2, nbins);
        edges.resize(K + 1);
        for (int i = 0; i <= K; ++i)
            edges[i] = xmin + (xmax - xmin) * double(i) / double(K);
    }

    // Build O = histogram of x against edges. MATLAB chi2gof's binning
    // rule depends on whether edges came from the user (left-closed,
    // last bin right-inclusive — standard histcounts) or auto-binning
    // (right-closed first bin extended, right-inclusive everywhere).
    // Verified vs R2025b on (-3:0.05:3) data with both Edges= and NBins=.
    const size_t K0 = edges.size() - 1;
    ScratchVec<double> O(K0, 0.0, &scratch);
    for (size_t i = 0; i < N; ++i) {
        const double v = x.elemAsDouble(i);
        if (v < edges[0] || v > edges[K0]) continue;
        for (size_t b = 0; b < K0; ++b) {
            bool in;
            if (edges_set) {
                // Left-closed standard histcounts.
                in = (b == K0 - 1)
                    ? (v >= edges[b] && v <= edges[b + 1])
                    : (v >= edges[b] && v <  edges[b + 1]);
            } else {
                // Right-closed (auto-binning).
                in = (b == 0)
                    ? (v <= edges[1])
                    : (v > edges[b] && v <= edges[b + 1]);
            }
            if (in) { O[b] += 1.0; break; }
        }
    }

    // Build E under N(mean, sd) by default (via norm CDF).
    ScratchVec<double> E(K0, 0.0, &scratch);
    auto Phi = [](double z) {
        return 0.5 * (1.0 + std::erf(z / std::sqrt(2.0)));
    };
    // Total mass under tail-extended bins so that Σ E = N (matches MATLAB).
    // The first bin extends to -∞, the last to +∞ (chi2gof convention).
    for (size_t b = 0; b < K0; ++b) {
        const double z_lo = (b == 0)        ? -std::numeric_limits<double>::infinity()
                                            : (edges[b]     - mean) / sd;
        const double z_hi = (b == K0 - 1)   ?  std::numeric_limits<double>::infinity()
                                            : (edges[b + 1] - mean) / sd;
        const double F_lo = std::isfinite(z_lo) ? Phi(z_lo) : 0.0;
        const double F_hi = std::isfinite(z_hi) ? Phi(z_hi) : 1.0;
        E[b] = double(N) * (F_hi - F_lo);
    }

    // Apply EMin: merge tail bins with E < emin (working from each end
    // inward, merging the small bin into its inward neighbour). MATLAB
    // also merges contiguous low-E interior runs, but tail-only is the
    // common case; this matches the audit reference output.
    auto merge_left = [&]() {
        while (O.size() > 1 && E[0] < emin) {
            O[1] += O[0]; E[1] += E[0];
            O.erase(O.begin()); E.erase(E.begin());
            edges.erase(edges.begin() + 1);  // remove inner edge
        }
    };
    auto merge_right = [&]() {
        while (O.size() > 1 && E.back() < emin) {
            O[O.size() - 2] += O.back(); E[E.size() - 2] += E.back();
            O.pop_back(); E.pop_back();
            edges.erase(edges.end() - 2);  // remove inner edge
        }
    };
    merge_left();
    merge_right();

    // Compute chi2.
    double chi2 = 0.0;
    for (size_t b = 0; b < O.size(); ++b) {
        if (E[b] > 0.0) {
            const double d = O[b] - E[b];
            chi2 += d * d / E[b];
        }
    }
    const int K_final = (int)O.size();
    // Default NParams = 2 (mean + std estimated from data) unless user
    // overrode or supplied explicit Edges (MATLAB still defaults to 2).
    const int np = nparams_set ? nparams : 2;
    const double df = double(K_final) - 1.0 - double(np);
    Value chi2v = Value::scalar(chi2, mr);
    const double cdf = (df > 0.0) ? chi2cdf(chi2v, df, mr).toScalar() : 1.0;
    const double p = std::max(0.0, 1.0 - cdf);
    const int h = (p < alpha) ? 1 : 0;

    outs[0] = Value::scalar(double(h), mr);
    if (nargout > 1) outs[1] = Value::scalar(p, mr);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        s.field("chi2stat") = Value::scalar(chi2, mr);
        s.field("df")       = Value::scalar(df, mr);
        // Pack edges / O / E into Value rows.
        Value edges_out = Value::matrix(1, edges.size(), ValueType::DOUBLE, mr);
        std::copy(edges.begin(), edges.end(), edges_out.doubleDataMut());
        s.field("edges") = edges_out;
        Value O_out = Value::matrix(1, O.size(), ValueType::DOUBLE, mr);
        std::copy(O.begin(), O.end(), O_out.doubleDataMut());
        s.field("O") = O_out;
        Value E_out = Value::matrix(1, E.size(), ValueType::DOUBLE, mr);
        std::copy(E.begin(), E.end(), E_out.doubleDataMut());
        s.field("E") = E_out;
        outs[2] = std::move(s);
    }
    (void)nbins_set;  // documented arg, no separate code path needed
    (void)ctrs_set;
    (void)expected_set;
}

void vartestn_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("vartestn: requires (X[, GROUP][, N-V pairs])",
                    0, 0, "vartestn", "", "m:vartestn:nargin");
    auto *mr = ctx.engine->resource();
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    // Parse: vartestn(X[, GROUP], N-V...). The 2nd arg is GROUP iff it's
    // a non-string vector. If it's a string, no group given (matrix
    // input form: each column = group).
    int test = 0;  // Bartlett default
    size_t nv_start = 1;
    Value X = args[0];
    Value G;
    bool have_group = false;
    if (args.size() >= 2 && !(args[1].isChar() || args[1].isString())) {
        G = args[1];
        have_group = true;
        nv_start = 2;
    }
    for (size_t i = nv_start; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString()) break;
        const std::string name = lower(args[i].toString());
        if (name == "testtype") {
            const std::string v = lower(args[i + 1].toString());
            if      (v == "bartlett")        test = 0;
            else if (v == "levenequadratic") test = 1;
            else if (v == "leveneabsolute")  test = 2;
            else if (v == "brownforsythe")   test = 3;
            else if (v == "obrien")          test = 4;
            else throw Error("vartestn: unknown TestType '" + v + "'",
                             0, 0, "vartestn", "", "m:vartestn:badtype");
        }
        // 'display' / 'alpha' silently ignored (Display has no console
        // effect; alpha doesn't change p/stat output).
    }

    // Matrix-input form: build (values, group) where group encodes the
    // column index of each observation.
    if (!have_group) {
        const size_t R = X.dims().rows();
        const size_t C = X.dims().cols();
        if (R == 0 || C < 2)
            throw Error("vartestn: matrix input must have >=2 columns",
                        0, 0, "vartestn", "", "m:vartestn:size");
        ScratchArena scratch(mr);
        ScratchVec<double> vv(R * C, &scratch);
        ScratchVec<double> gg(R * C, &scratch);
        for (size_t c = 0; c < C; ++c)
            for (size_t r = 0; r < R; ++r) {
                vv[c * R + r] = X.elemAsDouble(c * R + r);
                gg[c * R + r] = double(c + 1);
            }
        Value Vx = Value::matrix(R * C, 1, ValueType::DOUBLE, mr);
        Value Vg = Value::matrix(R * C, 1, ValueType::DOUBLE, mr);
        std::copy(vv.begin(), vv.end(), Vx.doubleDataMut());
        std::copy(gg.begin(), gg.end(), Vg.doubleDataMut());
        X = std::move(Vx);
        G = std::move(Vg);
    }

    auto [p, stat, df1, df2] = vartestn_full(X, G, test, mr);
    outs[0] = std::move(p);
    if (nargout > 1) {
        Value s = Value::structure(mr);
        if (test == 0) {
            s.field("chisqstat") = stat;
            s.field("df")        = df1;
        } else {
            s.field("fstat") = stat;
            // df is a 1×2 row vector [df1 df2].
            Value dfv = Value::matrix(1, 2, ValueType::DOUBLE, mr);
            double *dp = dfv.doubleDataMut();
            dp[0] = df1.toScalar();
            dp[1] = df2.toScalar();
            s.field("df") = dfv;
        }
        outs[1] = std::move(s);
    }
}

void runstest_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("runstest: requires X[, v | 'ud'][, alpha, tail | name-value]",
                    0, 0, "runstest", "", "m:runstest:nargin");
    auto *mr = ctx.engine->resource();

    // arg[1] is positional v (scalar), 'ud' string for up-down test, or a
    // name-value start.
    double v = std::numeric_limits<double>::quiet_NaN();   // sentinel: use median(x)
    bool up_down = false;
    size_t i = 1;
    if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        std::string s = args[i].toString();
        std::string sl = s;
        for (auto &c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (sl == "ud") { up_down = true; ++i; }
        // otherwise leave for the Name-Value loop below.
    } else if (i < args.size() && !args[i].isEmpty()) {
        v = args[i].toScalar();
        ++i;
    }

    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    std::string method;

    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const Value &val = args[i + 1];
        if      (name == "alpha")  alpha = val.toScalar();
        else if (name == "tail")   tail  = parse_tail(val.toString(), TestTail::Both);
        else if (name == "method") method = val.toString();
        i += 2;
    }

    // For 'ud' (up-down test): replace x with sign(diff(x)) — the
    // resulting binary sequence above/below 0 counts ascent/descent
    // runs, which is exactly what MATLAB's runstest('ud') does.
    Value xUsed = args[0];
    if (up_down) {
        const Value &x = args[0];
        const size_t Nx = x.numel();
        if (Nx < 2) {
            Value diffSign = Value::matrix(0, 0, ValueType::DOUBLE, mr);
            xUsed = std::move(diffSign);
        } else {
            std::vector<double> diffs;
            diffs.reserve(Nx - 1);
            for (size_t k = 1; k < Nx; ++k) {
                const double xa = x.elemAsDouble(k - 1);
                const double xb = x.elemAsDouble(k);
                if (std::isnan(xa) || std::isnan(xb)) continue;
                diffs.push_back((xb > xa) ? 1.0 : ((xb < xa) ? -1.0 : 0.0));
            }
            Value sgn = Value::matrix(1, diffs.size(), ValueType::DOUBLE, mr);
            if (!diffs.empty())
                std::copy(diffs.begin(), diffs.end(), sgn.doubleDataMut());
            xUsed = std::move(sgn);
        }
        v = 0.0;       // reference value for the sign sequence
    }

    auto [p, h, R, n1, n0, z] = runstest(xUsed, v, alpha, tail, method, mr);
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        s.field("nruns") = R;
        s.field("n1")    = n1;
        s.field("n0")    = n0;
        if (!std::isnan(z.toScalar())) s.field("z") = z;
        outs[2] = std::move(s);
    }
}

void ranksum_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ranksum: requires (X, Y[, alpha, tail | name-value])",
                    0, 0, "ranksum", "", "m:ranksum:nargin");
    auto *mr = ctx.engine->resource();

    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    std::string method;

    size_t i = 2;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()) {
        alpha = args[i].toScalar();
        ++i;
    }
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const Value &v = args[i + 1];
        if      (name == "alpha")  alpha = v.toScalar();
        else if (name == "tail")   tail  = parse_tail(v.toString(), TestTail::Both);
        else if (name == "method") method = v.toString();
        i += 2;
    }

    auto [p, h, rs, z] = ranksum(args[0], args[1], alpha, tail, method, mr);
    outs[0] = std::move(p);
    if (nargout > 1) outs[1] = std::move(h);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        if (!std::isnan(z.toScalar())) s.field("zval") = z;
        s.field("ranksum") = rs;
        outs[2] = std::move(s);
    }
}

void signrank_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("signrank: requires X[, m | y][, alpha, tail or "
                    "name-value]", 0, 0, "signrank", "", "m:signrank:nargin");
    auto *mr = ctx.engine->resource();

    Value y_or_m = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    size_t i = 1;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()) {
        y_or_m = args[i];
        ++i;
    }

    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    std::string method;

    if (i < args.size() && !args[i].isChar() && !args[i].isString()) {
        alpha = args[i].toScalar();
        ++i;
    }
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const Value &v = args[i + 1];
        if      (name == "alpha")  alpha = v.toScalar();
        else if (name == "tail")   tail  = parse_tail(v.toString(), TestTail::Both);
        else if (name == "method") method = v.toString();
        i += 2;
    }

    auto [p, h, sr, z] = signrank(args[0], y_or_m, alpha, tail, method, mr);
    outs[0] = std::move(p);
    if (nargout > 1) outs[1] = std::move(h);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        s.field("signedrank") = sr;
        // zval only present for approximate method (NaN otherwise).
        if (!std::isnan(z.toScalar())) s.field("zval") = z;
        outs[2] = std::move(s);
    }
}

void signtest_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("signtest: requires X[, m | y][, alpha, tail or "
                    "name-value]", 0, 0, "signtest", "", "m:signtest:nargin");
    auto *mr = ctx.engine->resource();

    // arg[1] may be: missing, scalar median, or paired y vector. Skip it
    // if it's a string (start of name-value list).
    Value y_or_m = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    size_t i = 1;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()) {
        y_or_m = args[i];
        ++i;
    }

    double alpha = 0.05;
    TestTail tail = TestTail::Both;

    // Optional positional alpha next (legacy 3-arg form), then name-value.
    if (i < args.size() && !args[i].isChar() && !args[i].isString()) {
        alpha = args[i].toScalar();
        ++i;
    }
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const Value &v = args[i + 1];
        if      (name == "alpha")  alpha = v.toScalar();
        else if (name == "tail")   tail  = parse_tail(v.toString(), TestTail::Both);
        else if (name == "method") { /* exact / approximate — both rely on binocdf */ }
        i += 2;
    }

    auto [p, h, sig] = signtest(args[0], y_or_m, alpha, tail, mr);
    outs[0] = std::move(p);
    if (nargout > 1) outs[1] = std::move(h);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        // MATLAB R2025b stats struct shape: {zval, sign}. zval is NaN
        // for the exact (binomial) path — currently always taken;
        // 'approximate' would populate zval with the normal-approx z.
        s.field("zval") = Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr);
        s.field("sign") = sig;
        outs[2] = std::move(s);
    }
}

// ── lillietest (Lilliefors normality test) ─────────────────────────
//
// Tests H0: x ~ N(mu, sigma^2) for unspecified mu, sigma. Uses KS
// statistic against a fitted normal CDF (mean and std estimated
// from sample). p-value via Stephens (1974) approximation.

void lillietest_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("lillietest: requires at least 1 argument",
                    0, 0, "lillietest", "", "m:lillietest:nargin");
    auto *mr = ctx.engine->resource();
    const Value &X = args[0];
    const std::size_t N = X.numel();
    if (N < 4)
        throw Error("lillietest: sample size must be >= 4",
                    0, 0, "lillietest", "", "m:lillietest:nsamples");

    double alpha = 0.05;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        alpha = args[1].toScalar();
        if (alpha <= 0.0 || alpha >= 1.0)
            throw Error("lillietest: alpha must be in (0, 1)",
                        0, 0, "lillietest", "", "m:lillietest:alpha");
    }

    // Sort sample. PMR HARD RULE: scratch via per-call ScratchArena.
    ScratchArena scratch(mr);
    ScratchVec<double> xs(N, &scratch);
    for (std::size_t i = 0; i < N; ++i) xs[i] = X.elemAsDouble(i);
    std::sort(xs.begin(), xs.end());

    // Sample mean and std (sample std with N-1 normalization).
    double sum = 0.0;
    for (double v : xs) sum += v;
    const double mean = sum / static_cast<double>(N);
    double ss = 0.0;
    for (double v : xs) { const double d = v - mean; ss += d * d; }
    const double sd = std::sqrt(ss / static_cast<double>(N - 1));
    if (sd == 0.0)
        throw Error("lillietest: sample has zero variance",
                    0, 0, "lillietest", "", "m:lillietest:zeroVar");

    // KS statistic against fitted normal CDF.
    // For each sorted x_i: F_emp_lo = (i-1)/N, F_emp_hi = i/N (1-based i).
    // F_norm(x_i) computed via std::erf.
    double D = 0.0;
    const double inv_sqrt2 = 0.7071067811865476;
    for (std::size_t i = 0; i < N; ++i) {
        const double z = (xs[i] - mean) / sd;
        const double F = 0.5 * (1.0 + std::erf(z * inv_sqrt2));
        const double F_emp_lo = static_cast<double>(i)         / static_cast<double>(N);
        const double F_emp_hi = static_cast<double>(i + 1)     / static_cast<double>(N);
        D = std::max(D, std::max(std::fabs(F - F_emp_lo),
                                  std::fabs(F - F_emp_hi)));
    }

    // Stephens (1974) modified statistic for Lilliefors:
    // D* = D * (sqrt(n) - 0.01 + 0.85/sqrt(n))
    const double sqn = std::sqrt(static_cast<double>(N));
    const double D_star = D * (sqn - 0.01 + 0.85 / sqn);

    // Stephens p-value approximation (valid for D* > 0.43 roughly):
    //   p ≈ exp(-7.01256 * D*^2)  for D* large
    // Better: use the approximate inverse table.
    // Lilliefors critical values (Stephens 1974, table 8.5.4):
    //   alpha   0.20    0.15    0.10    0.05    0.01
    //   D*      0.741   0.775   0.819   0.895   1.035
    // Below 0.20 the test always fails to reject (h=0 with p>0.20).
    auto stephensP = [](double Ds) {
        if (Ds < 0.474) return 0.50;     // very high p
        // Smooth interp between table points.
        struct Pt { double Ds, p; };
        static const Pt tbl[] = {
            {0.474, 0.50},
            {0.741, 0.20},
            {0.775, 0.15},
            {0.819, 0.10},
            {0.895, 0.05},
            {1.035, 0.01},
            {1.50,  1e-4},
            {2.00,  1e-7}
        };
        for (std::size_t k = 1; k < sizeof(tbl)/sizeof(tbl[0]); ++k) {
            if (Ds <= tbl[k].Ds) {
                const double frac = (Ds - tbl[k-1].Ds) / (tbl[k].Ds - tbl[k-1].Ds);
                // Log-linear interpolation in p.
                const double lp = std::log(tbl[k-1].p)
                                + frac * (std::log(tbl[k].p) - std::log(tbl[k-1].p));
                return std::exp(lp);
            }
        }
        return 1e-10;
    };
    auto stephensCV = [](double a) {
        struct Pt { double a, Ds; };
        static const Pt tbl[] = {
            {0.20, 0.741},
            {0.15, 0.775},
            {0.10, 0.819},
            {0.05, 0.895},
            {0.01, 1.035}
        };
        // Find bracket.
        for (std::size_t k = 1; k < sizeof(tbl)/sizeof(tbl[0]); ++k) {
            if (a >= tbl[k].a) {
                // Linear interp in alpha.
                const double frac = (a - tbl[k-1].a) / (tbl[k].a - tbl[k-1].a);
                return tbl[k-1].Ds + frac * (tbl[k].Ds - tbl[k-1].Ds);
            }
        }
        return tbl[sizeof(tbl)/sizeof(tbl[0]) - 1].Ds;
    };

    const double p_val = std::min(0.5, std::max(1e-10, stephensP(D_star)));
    const double D_critstar = stephensCV(alpha);
    // Convert critical D* back to plain D for output (per MATLAB convention).
    const double D_crit = D_critstar / (sqn - 0.01 + 0.85 / sqn);
    const int h = (D > D_crit) ? 1 : 0;

    outs[0] = Value::scalar(static_cast<double>(h), mr);
    if (nargout > 1) outs[1] = Value::scalar(p_val,  mr);
    if (nargout > 2) outs[2] = Value::scalar(D,      mr);
    if (nargout > 3) outs[3] = Value::scalar(D_crit, mr);
}

} // namespace detail
} // namespace numkit::stats
