// libs/stats/src/test/hypothesis.cpp

#include <numkit/stats/test/hypothesis.hpp>

#include <numkit/stats/distributions/students_t.hpp>
#include <numkit/stats/distributions/normal.hpp>
#include <numkit/stats/distributions/chi2.hpp>
#include <numkit/stats/distributions/fisher_f.hpp>

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
    if (s == "both")  return TestTail::Both;
    if (s == "right") return TestTail::Right;
    if (s == "left")  return TestTail::Left;
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
    Value cdf = (args.size() >= 2) ? args[1] : Value();  // empty default
    double alpha = parse_alpha(args, 2, 0.05);
    TestTail tail = TestTail::Both;
    for (size_t i = 2; i < args.size(); ++i)
        if (args[i].isChar() || args[i].isString())
            tail = parse_tail(args[i].toString(), TestTail::Both);
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
        throw Error("kstest2: requires (X, Y[, alpha, tail])",
                    0, 0, "kstest2", "", "m:kstest2:nargin");
    double alpha = parse_alpha(args, 2, 0.05);
    TestTail tail = TestTail::Both;
    for (size_t i = 2; i < args.size(); ++i)
        if (args[i].isChar() || args[i].isString())
            tail = parse_tail(args[i].toString(), TestTail::Both);
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

} // namespace detail
} // namespace numkit::stats
