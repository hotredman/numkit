// libs/stats/src/test/hypothesis.cpp

#include <numkit/stats/test/hypothesis.hpp>

#include <numkit/stats/distributions/students_t.hpp>
#include <numkit/stats/distributions/normal.hpp>
#include <numkit/stats/distributions/chi2.hpp>
#include <numkit/stats/distributions/fisher_f.hpp>
#include <numkit/stats/distributions/binomial.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
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
double tpvalue(std::pmr::memory_resource *mr, double tstat, double df, TestTail tail) {
    Value tv = Value::scalar(tstat, mr);
    Value cdf_v = tcdf(mr, tv, df);
    const double cdf = cdf_v.toScalar();
    switch (tail) {
        case TestTail::Both:  return 2.0 * std::min(cdf, 1.0 - cdf);
        case TestTail::Right: return 1.0 - cdf;
        case TestTail::Left:  return cdf;
    }
    return 1.0;
}

double zpvalue(std::pmr::memory_resource *mr, double z, TestTail tail) {
    Value zv = Value::scalar(z, mr);
    Value cdf_v = normcdf(mr, zv, 0.0, 1.0);
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
ttest(std::pmr::memory_resource *mr, const Value &x,
      double m, double alpha, TestTail tail)
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
    const double p   = tpvalue(mr, t, df, tail);
    const int    h   = (p < alpha) ? 1 : 0;

    // Confidence interval for the mean.
    double clo, chi;
    Value half = Value::scalar(1.0 - 0.5 * alpha, mr);
    Value tcrit_v = tinv(mr, half, df);
    const double tcrit = tcrit_v.toScalar();
    switch (tail) {
        case TestTail::Both:
            clo = mu_hat - tcrit * se; chi = mu_hat + tcrit * se; break;
        case TestTail::Right: {
            Value full = Value::scalar(1.0 - alpha, mr);
            const double tc = tinv(mr, full, df).toScalar();
            clo = mu_hat - tc * se; chi = std::numeric_limits<double>::infinity();
            break;
        }
        case TestTail::Left: {
            Value full = Value::scalar(1.0 - alpha, mr);
            const double tc = tinv(mr, full, df).toScalar();
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
ttest2(std::pmr::memory_resource *mr, const Value &x, const Value &y,
       double alpha, TestTail tail, const std::string &vartype)
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
    const double p = tpvalue(mr, t, df, tail);
    const int    h = (p < alpha) ? 1 : 0;

    Value half = Value::scalar(1.0 - 0.5 * alpha, mr);
    const double tcrit = tinv(mr, half, df).toScalar();
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
ztest(std::pmr::memory_resource *mr, const Value &x,
      double m, double sigma, double alpha, TestTail tail)
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
    const double p  = zpvalue(mr, z, tail);
    const int    h  = (p < alpha) ? 1 : 0;

    Value half = Value::scalar(1.0 - 0.5 * alpha, mr);
    const double zcrit = norminv(mr, half, 0.0, 1.0).toScalar();
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
vartest(std::pmr::memory_resource *mr, const Value &x,
        double v, double alpha, TestTail tail)
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
    const double cdf = chi2cdf(mr, Tv, df).toScalar();

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
    const double chi_lo = chi2inv(mr, lo_v, df).toScalar();
    const double chi_hi = chi2inv(mr, hi_v, df).toScalar();
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
vartest2(std::pmr::memory_resource *mr, const Value &x, const Value &y,
         double alpha, TestTail tail)
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
    const double cdf = fcdf(mr, Fv, v1, v2).toScalar();

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
    const double f_lo = finv(mr, lo_v, v1, v2).toScalar();
    const double f_hi = finv(mr, hi_v, v1, v2).toScalar();
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
kstest(std::pmr::memory_resource *mr, const Value &x,
       const Value &cdf, double alpha, TestTail tail)
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
        return normcdf(mr, s, 0.0, 1.0).toScalar();
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
kstest2(std::pmr::memory_resource *mr, const Value &x, const Value &y,
        double alpha, TestTail tail)
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

std::tuple<Value, Value, Value, Value>
jbtest(std::pmr::memory_resource *mr, const Value &x, double alpha)
{
    if (alpha <= 0.0 || alpha >= 1.0) alpha = 0.05;
    const size_t N = x.numel();
    if (N < 4)
        throw Error("jbtest: need at least 4 samples", 0, 0, "jbtest", "",
                    "m:jbtest:nsamples");

    // Sample skewness and kurtosis (population formula — MATLAB default).
    double mean = 0.0;
    for (size_t i = 0; i < N; ++i) mean += x.elemAsDouble(i);
    mean /= double(N);
    double m2 = 0.0, m3 = 0.0, m4 = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double d = x.elemAsDouble(i) - mean;
        const double d2 = d * d;
        m2 += d2;
        m3 += d2 * d;
        m4 += d2 * d2;
    }
    m2 /= double(N); m3 /= double(N); m4 /= double(N);
    const double S = (m2 > 0.0) ? m3 / std::pow(m2, 1.5) : 0.0;
    const double K = (m2 > 0.0) ? m4 / (m2 * m2) : 0.0;

    // JB = n/6 · (S² + (K-3)²/4)  ~  χ²(2)
    const double JB = double(N) / 6.0 * (S * S + 0.25 * (K - 3.0) * (K - 3.0));
    Value JBv = Value::scalar(JB, mr);
    const double cdf = chi2cdf(mr, JBv, 2.0).toScalar();
    const double p = 1.0 - cdf;
    const int h = (p < alpha) ? 1 : 0;

    Value oneMinusAlpha = Value::scalar(1.0 - alpha, mr);
    const double cv = chi2inv(mr, oneMinusAlpha, 2.0).toScalar();

    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p, mr),
                           Value::scalar(JB, mr),
                           Value::scalar(cv, mr));
}

// ════════════════════════════════════════════════════════════════════
// signtest — non-parametric sign test
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value>
signtest(std::pmr::memory_resource *mr, const Value &x,
         const Value &y_or_m, double alpha, TestTail tail)
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
        const double cdfLE = binocdf(mr, kPos, double(n_eff), 0.5).toScalar();
        const double cdfLT = (n_pos > 0)
                           ? binocdf(mr, kPosM1, double(n_eff), 0.5).toScalar()
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
fishertest(std::pmr::memory_resource *mr, const Value &T,
           double alpha, TestTail tail)
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
        const double zcrit = norminv(mr, pcrit, 0.0, 1.0).toScalar();
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
chi2gof(std::pmr::memory_resource *mr,
        const Value &observed, const Value &expected,
        int nparams, double alpha)
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
    const double cdf = chi2cdf(mr, xv, df).toScalar();
    const double p = std::max(0.0, 1.0 - cdf);
    const int h = (p < alpha) ? 1 : 0;
    return std::make_tuple(Value::scalar(p, mr),
                           Value::scalar(double(h), mr),
                           Value::scalar(chi2, mr),
                           Value::scalar(df, mr));
}

// ════════════════════════════════════════════════════════════════════
// vartestn — Bartlett's k-sample variance test
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value>
vartestn(std::pmr::memory_resource *mr, const Value &x, const Value &group,
         double /*alpha*/)
{
    const size_t Nx = x.numel();
    if (Nx == 0 || group.numel() != Nx)
        throw Error("vartestn: x and group must be same length",
                    0, 0, "vartestn", "", "m:vartestn:size");

    // Bucket observations by group label. Use string keys (covers numeric,
    // char, and string labels uniformly via toString conversion semantics
    // expected by callers — see partition logic below).
    std::vector<double> labels(Nx);
    std::vector<double> values(Nx);
    for (size_t i = 0; i < Nx; ++i) {
        labels[i] = group.elemAsDouble(i);
        values[i] = x.elemAsDouble(i);
    }

    // Group by label.
    std::vector<std::pair<double, std::vector<double>>> groups;
    for (size_t i = 0; i < Nx; ++i) {
        const double xi = values[i];
        if (std::isnan(xi) || std::isnan(labels[i])) continue;
        bool found = false;
        for (auto &g : groups) {
            if (g.first == labels[i]) {
                g.second.push_back(xi);
                found = true;
                break;
            }
        }
        if (!found) groups.push_back({labels[i], {xi}});
    }

    // Collect (n_i, var_i).
    std::vector<size_t> ns;
    std::vector<double> vars;
    size_t N = 0;
    for (auto &g : groups) {
        const auto &vec = g.second;
        const size_t n = vec.size();
        if (n < 2) continue;  // group with <2 obs has no sample variance
        double mean = 0.0;
        for (double v : vec) mean += v;
        mean /= double(n);
        double s2 = 0.0;
        for (double v : vec) { const double d = v - mean; s2 += d * d; }
        s2 /= double(n - 1);
        ns.push_back(n);
        vars.push_back(s2);
        N += n;
    }
    const size_t k = ns.size();
    if (k < 2) {
        // Fewer than 2 groups with ≥2 obs → degenerate, no test.
        return std::make_tuple(Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr),
                               Value::scalar(0.0, mr),
                               Value::scalar(double(k > 0 ? k - 1 : 0), mr));
    }

    // Pooled sample variance.
    double Sp2_num = 0.0;
    for (size_t i = 0; i < k; ++i) Sp2_num += double(ns[i] - 1) * vars[i];
    const double Sp2 = Sp2_num / double(N - k);

    // Q.
    double Q = double(N - k) * std::log(Sp2);
    for (size_t i = 0; i < k; ++i) Q -= double(ns[i] - 1) * std::log(vars[i]);

    // C.
    double inv_sum = 0.0;
    for (size_t i = 0; i < k; ++i) inv_sum += 1.0 / double(ns[i] - 1);
    inv_sum -= 1.0 / double(N - k);
    const double C = 1.0 + inv_sum / (3.0 * double(k - 1));

    const double chisq = Q / C;
    const double df = double(k - 1);

    // p = 1 - chi2cdf(chisq, df).
    Value xv = Value::scalar(chisq, mr);
    const double cdf = chi2cdf(mr, xv, df).toScalar();
    const double p = std::max(0.0, 1.0 - cdf);

    return std::make_tuple(Value::scalar(p, mr),
                           Value::scalar(chisq, mr),
                           Value::scalar(df, mr));
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
runstest(std::pmr::memory_resource *mr, const Value &x, double v_in,
         double alpha, TestTail tail, const std::string &method_in)
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
        const double cdf = normcdf(mr, zV, 0.0, 1.0).toScalar();
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
ranksum(std::pmr::memory_resource *mr, const Value &x, const Value &y,
        double alpha, TestTail tail, const std::string &method_in)
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
        const double cdf = normcdf(mr, zV, 0.0, 1.0).toScalar();
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
signrank(std::pmr::memory_resource *mr, const Value &x,
         const Value &y_or_m, double alpha, TestTail tail,
         const std::string &method_in)
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
        const double cdf = normcdf(mr, zV, 0.0, 1.0).toScalar();
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
        throw Error("ttest: requires (X[, m, alpha, tail])", 0, 0, "ttest", "",
                    "m:ttest:nargin");
    double m     = (args.size() >= 2 && !args[1].isEmpty()) ? args[1].toScalar() : 0.0;
    double alpha = parse_alpha(args, 2, 0.05);
    TestTail tail = TestTail::Both;
    for (size_t i = 2; i < args.size(); ++i)
        if (args[i].isChar() || args[i].isString())
            tail = parse_tail(args[i].toString(), TestTail::Both);
    auto [h, p, ci, t] = ttest(ctx.engine->resource(), args[0], m, alpha, tail);
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(ci);
    if (nargout > 3) outs[3] = std::move(t);
}

void ttest2_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ttest2: requires (X, Y[, alpha, tail, vartype])",
                    0, 0, "ttest2", "", "m:ttest2:nargin");
    double alpha = parse_alpha(args, 2, 0.05);
    TestTail tail = TestTail::Both;
    std::string vartype = "unequal";
    for (size_t i = 2; i + 1 < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            const auto k = args[i].toString();
            if (k == "Tail")    tail = parse_tail(args[i + 1].toString(), TestTail::Both);
            else if (k == "Vartype" || k == "VarType") vartype = args[i + 1].toString();
            else if (k == "Alpha") alpha = args[i + 1].toScalar();
        }
    }
    auto [h, p, ci, t] = ttest2(ctx.engine->resource(), args[0], args[1],
                                 alpha, tail, vartype);
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(ci);
    if (nargout > 3) outs[3] = std::move(t);
}

void ztest_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ztest: requires (X, m, sigma[, alpha, tail])",
                    0, 0, "ztest", "", "m:ztest:nargin");
    const double m     = args[1].toScalar();
    const double sigma = args[2].toScalar();
    double alpha = parse_alpha(args, 3, 0.05);
    TestTail tail = TestTail::Both;
    for (size_t i = 3; i < args.size(); ++i)
        if (args[i].isChar() || args[i].isString())
            tail = parse_tail(args[i].toString(), TestTail::Both);
    auto [h, p, ci, z] = ztest(ctx.engine->resource(), args[0], m, sigma, alpha, tail);
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(ci);
    if (nargout > 3) outs[3] = std::move(z);
}

void vartest_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("vartest: requires (X, v[, alpha, tail])",
                    0, 0, "vartest", "", "m:vartest:nargin");
    const double v = args[1].toScalar();
    double alpha = parse_alpha(args, 2, 0.05);
    TestTail tail = TestTail::Both;
    for (size_t i = 2; i < args.size(); ++i)
        if (args[i].isChar() || args[i].isString())
            tail = parse_tail(args[i].toString(), TestTail::Both);
    auto [h, p, ci, T] = vartest(ctx.engine->resource(), args[0], v, alpha, tail);
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(ci);
    if (nargout > 3) outs[3] = std::move(T);
}

void vartest2_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("vartest2: requires (X, Y[, alpha, tail])",
                    0, 0, "vartest2", "", "m:vartest2:nargin");
    double alpha = parse_alpha(args, 2, 0.05);
    TestTail tail = TestTail::Both;
    for (size_t i = 2; i < args.size(); ++i)
        if (args[i].isChar() || args[i].isString())
            tail = parse_tail(args[i].toString(), TestTail::Both);
    auto [h, p, ci, F] = vartest2(ctx.engine->resource(), args[0], args[1],
                                   alpha, tail);
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
    auto [h, p, D, cv] = kstest(ctx.engine->resource(), args[0], cdf, alpha, tail);
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
    auto [h, p, D, cv] = kstest2(ctx.engine->resource(), args[0], args[1],
                                  alpha, tail);
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(D);
    if (nargout > 3) outs[3] = std::move(cv);
}

void jbtest_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("jbtest: requires X[, alpha]", 0, 0, "jbtest", "",
                    "m:jbtest:nargin");
    double alpha = parse_alpha(args, 1, 0.05);
    auto [h, p, JB, cv] = jbtest(ctx.engine->resource(), args[0], alpha);
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
    auto [h, p, OR, lo, hi] = fishertest(mr, args[0], alpha, tail);
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
        throw Error("chi2gof: requires X[, 'Frequency', f, 'Expected', e, "
                    "'NParams', np, 'Alpha', a]",
                    0, 0, "chi2gof", "", "m:chi2gof:nargin");
    auto *mr = ctx.engine->resource();

    Value freq, expected;
    int nparams = 0;
    double alpha = 0.05;

    // First arg may be the data vector (auto-binned form, NYI) or just
    // a category-label vector that pairs with Frequency / Expected. We
    // require Frequency + Expected to be supplied via name-value.
    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const Value &v = args[i + 1];
        if      (name == "frequency") freq = v;
        else if (name == "expected")  expected = v;
        else if (name == "nparams")   nparams = static_cast<int>(v.toScalar());
        else if (name == "alpha")     alpha = v.toScalar();
        // 'edges', 'nbins', 'ctype', 'emin', etc. silently ignored
    }
    if (freq.isEmpty() || expected.isEmpty())
        throw Error("chi2gof: numkit currently requires explicit "
                    "'Frequency' and 'Expected' name-value arguments.",
                    0, 0, "chi2gof", "", "m:chi2gof:auto");

    auto [p, h, chi2, df] = chi2gof(mr, freq, expected, nparams, alpha);
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        s.field("chi2stat") = chi2;
        s.field("df")       = df;
        outs[2] = std::move(s);
    }
}

void vartestn_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("vartestn: requires (X, GROUP)",
                    0, 0, "vartestn", "", "m:vartestn:nargin");
    auto *mr = ctx.engine->resource();

    double alpha = 0.05;
    // Skip name-value pairs (Display, TestType, etc.) — only Bartlett supported.
    for (size_t i = 2; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name == "alpha") alpha = args[i + 1].toScalar();
        // 'display' / 'testtype' silently ignored
    }

    auto [p, chisq, df] = vartestn(mr, args[0], args[1], alpha);
    outs[0] = std::move(p);
    if (nargout > 1) {
        Value s = Value::structure(mr);
        s.field("chisqstat") = chisq;
        s.field("df")        = df;
        outs[1] = std::move(s);
    }
}

void runstest_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("runstest: requires X[, v][, alpha, tail | name-value]",
                    0, 0, "runstest", "", "m:runstest:nargin");
    auto *mr = ctx.engine->resource();

    // arg[1] is positional v (scalar) or a name-value start.
    double v = std::numeric_limits<double>::quiet_NaN();   // sentinel: use median(x)
    size_t i = 1;
    if (i < args.size() && !args[i].isChar() && !args[i].isString() && !args[i].isEmpty()) {
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

    auto [p, h, R, n1, n0, z] = runstest(mr, args[0], v, alpha, tail, method);
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

    auto [p, h, rs, z] = ranksum(mr, args[0], args[1], alpha, tail, method);
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

    auto [p, h, sr, z] = signrank(mr, args[0], y_or_m, alpha, tail, method);
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

    auto [p, h, sig] = signtest(mr, args[0], y_or_m, alpha, tail);
    outs[0] = std::move(p);
    if (nargout > 1) outs[1] = std::move(h);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        s.field("sign") = sig;
        outs[2] = std::move(s);
    }
}

} // namespace detail
} // namespace numkit::stats
