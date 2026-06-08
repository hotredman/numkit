// toolboxes/stats/src/test/hypothesis.cpp

#include <numkit/stats/test/hypothesis.hpp>

#include <numkit/stats/distributions/students_t.hpp>
#include <numkit/stats/distributions/normal.hpp>
#include <numkit/stats/distributions/chi2.hpp>
#include <numkit/stats/distributions/fisher_f.hpp>
#include <numkit/stats/distributions/binomial.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <random>
#include <vector>

#include "hypothesis_detail.hpp"

namespace numkit::stats {


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
                    "numkit:ttest:nsamples");

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

    // MATLAB returns the test statistics as a struct (tstat, df, sd).
    Value stats = Value::structure(mr);
    stats.field("tstat") = Value::scalar(t, mr);
    stats.field("df")    = Value::scalar(df, mr);
    stats.field("sd")    = Value::scalar(sd, mr);

    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p, mr),
                           std::move(ci),
                           std::move(stats));
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
                    0, 0, "ttest2", "", "numkit:ttest2:nsamples");

    double t, df, se;
    Value sdVal;   // MATLAB stats.sd: pooled sd (equal) | [sx sy] (unequal)
    if (vartype == "equal") {
        const double sp2 = ((nx - 1) * vx + (ny - 1) * vy) / double(nx + ny - 2);
        se = std::sqrt(sp2 * (1.0 / double(nx) + 1.0 / double(ny)));
        df = double(nx + ny - 2);
        sdVal = Value::scalar(std::sqrt(sp2), mr);
    } else {
        // Welch (unequal variances).
        se = std::sqrt(vx / double(nx) + vy / double(ny));
        const double num = (vx / nx + vy / ny) * (vx / nx + vy / ny);
        const double den = (vx / nx) * (vx / nx) / double(nx - 1)
                         + (vy / ny) * (vy / ny) / double(ny - 1);
        df = (den > 0.0) ? num / den : double(nx + ny - 2);
        sdVal = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        sdVal.doubleDataMut()[0] = std::sqrt(vx);
        sdVal.doubleDataMut()[1] = std::sqrt(vy);
    }
    t = (mx - my) / se;
    const double p = tpvalue(t, df, tail, mr);
    const int    h = (p < alpha) ? 1 : 0;

    Value half = Value::scalar(1.0 - 0.5 * alpha, mr);
    const double tcrit = tinv(half, df, mr).toScalar();
    Value ci = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    ci.doubleDataMut()[0] = (mx - my) - tcrit * se;
    ci.doubleDataMut()[1] = (mx - my) + tcrit * se;

    Value stats = Value::structure(mr);
    stats.field("tstat") = Value::scalar(t, mr);
    stats.field("df")    = Value::scalar(df, mr);
    stats.field("sd")    = std::move(sdVal);

    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p, mr),
                           std::move(ci),
                           std::move(stats));
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
                    "numkit:ztest:badsigma");

    double mu_hat, var_hat; size_t n;
    mean_var(x, mu_hat, var_hat, n);
    if (n < 1)
        throw Error("ztest: need at least 1 sample", 0, 0, "ztest", "",
                    "numkit:ztest:nsamples");

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
                    "numkit:vartest:badv");

    double mu_hat, var_hat; size_t n;
    mean_var(x, mu_hat, var_hat, n);
    if (n < 2)
        throw Error("vartest: need at least 2 samples", 0, 0, "vartest", "",
                    "numkit:vartest:nsamples");

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

    Value stats = Value::structure(mr);
    stats.field("chisqstat") = Value::scalar(T, mr);
    stats.field("df")        = Value::scalar(df, mr);

    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p, mr),
                           std::move(ci),
                           std::move(stats));
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
                    0, 0, "vartest2", "", "numkit:vartest2:nsamples");

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

    Value stats = Value::structure(mr);
    stats.field("fstat") = Value::scalar(F, mr);
    stats.field("df1")   = Value::scalar(v1, mr);
    stats.field("df2")   = Value::scalar(v2, mr);

    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p, mr),
                           std::move(ci),
                           std::move(stats));
}

// ════════════════════════════════════════════════════════════════════
// kstest — one-sample Kolmogorov-Smirnov
// ════════════════════════════════════════════════════════════════════


std::tuple<Value, Value, Value, Value>
kstest(const Value &x, const Value &cdf, double alpha, TestTail tail, std::pmr::memory_resource *mr)
{
    if (alpha <= 0.0 || alpha >= 1.0) alpha = 0.05;
    const size_t N = x.numel();
    if (N < 1)
        throw Error("kstest: empty sample", 0, 0, "kstest", "",
                    "numkit:kstest:nsamples");

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
    // Exact finite-n KS distribution (MATLAB default): two-sided Marsaglia,
    // one-sided Birnbaum-Tingey; critical value by inverting the same p(D).
    const int ni = static_cast<int>(N);
    double p, cv;
    if (tail == TestTail::Both) {
        p  = ksOneSampleTwoSidedP(ni, D);
        cv = ksCriticalValue([ni](double dd){ return ksOneSampleTwoSidedP(ni, dd); }, alpha);
    } else {
        p  = ksOneSampleOneSidedP(ni, D);
        cv = ksCriticalValue([ni](double dd){ return ksOneSampleOneSidedP(ni, dd); }, alpha);
    }
    const int h = (p < alpha) ? 1 : 0;

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
                    "numkit:kstest2:nsamples");

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
    // MATLAB kstest2: Stephens' corrected asymptotic (no exact small-sample
    // path). Two-sided = alternating series, one-sided = single leading term.
    const bool twoSided = (tail == TestTail::Both);
    const double p  = ksTwoSampleP(n_eff, D, twoSided);
    const int    h  = (p < alpha) ? 1 : 0;
    const double cv = ksCriticalValue([n_eff, twoSided](double dd){
                          return ksTwoSampleP(n_eff, dd, twoSided); }, alpha);

    return std::make_tuple(Value::scalar(double(h), mr),
                           Value::scalar(p, mr),
                           Value::scalar(D, mr),
                           Value::scalar(cv, mr));
}

// ════════════════════════════════════════════════════════════════════
// jbtest — Jarque-Bera normality
// ════════════════════════════════════════════════════════════════════


std::tuple<Value, Value, Value, Value>
jbtest(const Value &x, double alpha, double mctol, std::pmr::memory_resource *mr)
{
    if (alpha <= 0.0 || alpha >= 1.0) alpha = 0.05;
    const size_t N = x.numel();
    if (N < 4)
        throw Error("jbtest: need at least 4 samples", 0, 0, "jbtest", "",
                    "numkit:jbtest:nsamples");

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
                    0, 0, "signtest", "", "numkit:signtest:size");
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
                    0, 0, "fishertest", "", "numkit:fishertest:size");
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
                    0, 0, "chi2gof", "", "numkit:chi2gof:size");
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


// Public API. test = 0 Bartlett, 1 LeveneQuadratic, 2 LeveneAbsolute,
// 3 BrownForsythe, 4 OBrien.
std::tuple<Value, Value, Value, Value>
vartestn_full(const Value &x, const Value &group, int test, std::pmr::memory_resource *mr)
{
    const size_t Nx = x.numel();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (Nx == 0 || group.numel() != Nx)
        throw Error("vartestn: x and group must be same length",
                    0, 0, "vartestn", "", "numkit:vartestn:size");

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
                    0, 0, "signrank", "", "numkit:signrank:size");
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
// ansaribradley — Ansari-Bradley two-sample scale (dispersion) test
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value, Value>
ansaribradley(const Value &x, const Value &y, double alpha, TestTail tail,
              std::pmr::memory_resource *mr)
{
    const size_t nx = x.numel();
    const size_t ny = y.numel();
    if (nx == 0 || ny == 0) {
        return std::make_tuple(
            Value::scalar(0.0, mr),
            Value::scalar(1.0, mr),
            Value::scalar(0.0, mr),
            Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr));
    }

    // Pool: store (value, is_x). Drop NaN.
    struct Item { double v; bool is_x; };
    std::vector<Item> items;
    items.reserve(nx + ny);
    for (size_t i = 0; i < nx; ++i) {
        const double v = x.elemAsDouble(i);
        if (!std::isnan(v)) items.push_back({v, true});
    }
    for (size_t i = 0; i < ny; ++i) {
        const double v = y.elemAsDouble(i);
        if (!std::isnan(v)) items.push_back({v, false});
    }
    const size_t N = items.size();
    size_t m = 0, n = 0;
    for (auto &it : items) (it.is_x ? m : n) += 1;
    if (m == 0 || n == 0) {
        return std::make_tuple(
            Value::scalar(0.0, mr),
            Value::scalar(1.0, mr),
            Value::scalar(0.0, mr),
            Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr));
    }

    // Sort pooled by value (ascending).
    std::vector<size_t> ord(N);
    for (size_t i = 0; i < N; ++i) ord[i] = i;
    std::sort(ord.begin(), ord.end(),
              [&](size_t a, size_t b) { return items[a].v < items[b].v; });

    // Assign V-shaped Ansari rank to each sorted POSITION, then average
    // (mid-rank) across tied groups.
    // Raw position-rank: pos i (1..N) → min(i, N+1-i).
    std::vector<double> rraw(N);
    for (size_t i = 0; i < N; ++i) {
        const size_t pos = i + 1;             // 1-based
        const size_t alt = N + 1 - pos;
        rraw[i] = static_cast<double>(std::min(pos, alt));
    }
    // Walk in sorted order, distribute mean rank within each tie group.
    std::vector<double> rsorted(N);
    {
        size_t i = 0;
        while (i < N) {
            size_t j = i + 1;
            while (j < N && items[ord[j]].v == items[ord[i]].v) ++j;
            // Group [i, j). Mean of raw V-ranks in this group.
            double s = 0.0;
            for (size_t k = i; k < j; ++k) s += rraw[k];
            const double mr_avg = s / static_cast<double>(j - i);
            for (size_t k = i; k < j; ++k) rsorted[k] = mr_avg;
            i = j;
        }
    }

    // W = sum of ranks for x (in original-pool order via ord^-1).
    std::vector<double> ranks(N);
    for (size_t k = 0; k < N; ++k) ranks[ord[k]] = rsorted[k];
    double W = 0.0;
    for (size_t k = 0; k < N; ++k)
        if (items[k].is_x) W += ranks[k];

    // ── Choose exact vs asymptotic ─────────────────────────────────
    // Exact path mirrors MATLAB's small-sample threshold (uses exact
    // permutation distribution conditional on observed ranks). Use
    // exact when min(m, n) ≤ 10. Compute scale = LCM of tie-group
    // sizes so scaled ranks are exact integers (mid-rank in a tie
    // group of size k has denominator k).
    const bool exact_path = (std::min(m, n) <= 10);
    long long scale = 1;
    if (exact_path) {
        // Build set of unique tie-group sizes by walking sorted order.
        std::vector<long long> grp_sizes;
        size_t ii = 0;
        while (ii < N) {
            size_t jj = ii + 1;
            while (jj < N && items[ord[jj]].v == items[ord[ii]].v) ++jj;
            grp_sizes.push_back(static_cast<long long>(jj - ii));
            ii = jj;
        }
        auto lcm = [](long long a, long long b) -> long long {
            if (a == 0 || b == 0) return 0;
            long long g = a, h = b;
            while (h != 0) { long long t = g % h; g = h; h = t; }
            return a / g * b;
        };
        for (long long s : grp_sizes) scale = lcm(scale, s);
        if (scale > 10000) scale = 10000;   // safety bound
    }

    // Asymptotic moments (conditional-permutation form — handles ties
    // automatically). E[W] = m·mean(r); V[W] = m·n / (N − 1) · σ²(r),
    // with σ² = (1/N) Σ(rᵢ − r̄)².
    double sum_r = 0.0;
    for (double r : rsorted) sum_r += r;
    const double Nd = static_cast<double>(N);
    const double mean_r = sum_r / Nd;
    double ss = 0.0;
    for (double r : rsorted) {
        const double d = r - mean_r;
        ss += d * d;
    }
    const double sigma2_r = ss / Nd;
    const double EW = static_cast<double>(m) * mean_r;
    const double VW = static_cast<double>(m) * static_cast<double>(n)
                    / (Nd - 1.0) * sigma2_r;
    const double sd = std::sqrt(std::max(VW, 0.0));
    const double Wstar = (sd > 0.0) ? (W - EW) / sd : 0.0;

    // ── Tail convention for ansaribradley ──────────────────────────
    // MATLAB inverts the natural rank-tail mapping: larger W means
    // x clustered toward the centre of the pool → x is LESS dispersed
    // than y. So:
    //   alt 'right' (var x > var y)  ⇒  W small  ⇒  p = P(W ≤ obs)
    //   alt 'left'  (var x < var y)  ⇒  W large  ⇒  p = P(W ≥ obs)
    double p_val;
    if (exact_path) {
        // Scale ranks by `scale` (LCM of tie-group sizes) for exact
        // integer DP.
        std::vector<long long> rk2(N);
        long long total2 = 0;
        for (size_t k = 0; k < N; ++k) {
            rk2[k] = static_cast<long long>(
                std::llround(rsorted[k] * static_cast<double>(scale)));
            total2 += rk2[k];
        }
        const size_t S = static_cast<size_t>(total2 + 1);
        std::vector<std::vector<double>> dp(m + 1, std::vector<double>(S, 0.0));
        dp[0][0] = 1.0;
        for (size_t k = 0; k < N; ++k) {
            const long long r = rk2[k];
            for (long long kk = static_cast<long long>(m) - 1; kk >= 0; --kk) {
                for (long long s = static_cast<long long>(S) - 1; s >= r; --s)
                    dp[kk + 1][s] += dp[kk][s - r];
            }
        }
        const long long W_scaled = static_cast<long long>(
            std::llround(W * static_cast<double>(scale)));
        double total = 0.0;
        for (size_t s = 0; s < S; ++s) total += dp[m][s];
        double cdfLE = 0.0, cdfGE = 0.0;
        for (long long s = 0; s <= W_scaled && s < static_cast<long long>(S); ++s)
            cdfLE += dp[m][s];
        for (long long s = W_scaled; s < static_cast<long long>(S); ++s)
            cdfGE += dp[m][s];
        cdfLE /= total;
        cdfGE /= total;
        switch (tail) {
            case TestTail::Both:
                p_val = std::min(1.0, 2.0 * std::min(cdfLE, cdfGE));
                break;
            case TestTail::Right: p_val = cdfLE; break;  // var x > var y → W ↓
            case TestTail::Left:  p_val = cdfGE; break;  // var x < var y → W ↑
        }
    } else {
        Value zV = Value::scalar(Wstar, mr);
        const double Phi = normcdf(zV, 0.0, 1.0, mr).toScalar();
        switch (tail) {
            case TestTail::Both:  p_val = 2.0 * std::min(Phi, 1.0 - Phi); break;
            case TestTail::Right: p_val = Phi;       break;  // P(W ≤ obs)
            case TestTail::Left:  p_val = 1.0 - Phi; break;  // P(W ≥ obs)
        }
    }
    p_val = std::min(1.0, std::max(0.0, p_val));

    const int h = (p_val < alpha) ? 1 : 0;
    return std::make_tuple(
        Value::scalar(static_cast<double>(h), mr),
        Value::scalar(p_val, mr),
        Value::scalar(W, mr),
        Value::scalar(Wstar, mr));
}

// ── lillietest ───────────────────────────────────────────────────────
// [h, p, kstat, critval] = lillietest(x, alpha). Lilliefors normality
// test: KS statistic vs a fitted normal + Stephens (1974) modified
// statistic / critical-value table. See test/hypothesis.hpp.
std::tuple<Value, Value, Value, Value>
lillietest(const Value &X, double alpha, std::pmr::memory_resource *mr)
{
    const std::size_t N = X.numel();
    if (N < 4)
        throw Error("lillietest: sample size must be >= 4",
                    0, 0, "lillietest", "", "numkit:lillietest:nsamples");
    if (alpha <= 0.0 || alpha >= 1.0)
        throw Error("lillietest: alpha must be in (0, 1)",
                    0, 0, "lillietest", "", "numkit:lillietest:alpha");

    // Sort sample. PMR HARD RULE: scratch via per-call ScratchArena.
    ScratchArena scratch(mr);
    ScratchVec<double> xs(N, &scratch);
    for (std::size_t i = 0; i < N; ++i) xs[i] = X.elemAsDouble(i);
    std::sort(xs.begin(), xs.end());

    double sum = 0.0;
    for (double v : xs) sum += v;
    const double mean = sum / static_cast<double>(N);
    double ss = 0.0;
    for (double v : xs) { const double d = v - mean; ss += d * d; }
    const double sd = std::sqrt(ss / static_cast<double>(N - 1));
    if (sd == 0.0)
        throw Error("lillietest: sample has zero variance",
                    0, 0, "lillietest", "", "numkit:lillietest:zeroVar");

    // KS statistic against the fitted normal CDF.
    double D = 0.0;
    const double inv_sqrt2 = 0.7071067811865476;
    for (std::size_t i = 0; i < N; ++i) {
        const double z = (xs[i] - mean) / sd;
        const double F = 0.5 * (1.0 + std::erf(z * inv_sqrt2));
        const double F_emp_lo = static_cast<double>(i)     / static_cast<double>(N);
        const double F_emp_hi = static_cast<double>(i + 1) / static_cast<double>(N);
        D = std::max(D, std::max(std::fabs(F - F_emp_lo),
                                  std::fabs(F - F_emp_hi)));
    }

    // Stephens (1974) modified statistic: D* = D*(sqrt(n) - 0.01 + 0.85/sqrt(n)).
    const double sqn = std::sqrt(static_cast<double>(N));
    const double D_star = D * (sqn - 0.01 + 0.85 / sqn);

    auto stephensP = [](double Ds) {
        if (Ds < 0.474) return 0.50;     // very high p
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
        for (std::size_t k = 1; k < sizeof(tbl)/sizeof(tbl[0]); ++k) {
            if (a >= tbl[k].a) {
                const double frac = (a - tbl[k-1].a) / (tbl[k].a - tbl[k-1].a);
                return tbl[k-1].Ds + frac * (tbl[k].Ds - tbl[k-1].Ds);
            }
        }
        return tbl[sizeof(tbl)/sizeof(tbl[0]) - 1].Ds;
    };

    const double p_val = std::min(0.5, std::max(1e-10, stephensP(D_star)));
    const double D_critstar = stephensCV(alpha);
    // Convert critical D* back to plain D (MATLAB convention).
    const double D_crit = D_critstar / (sqn - 0.01 + 0.85 / sqn);
    const int h = (D > D_crit) ? 1 : 0;

    return std::make_tuple(Value::scalar(static_cast<double>(h), mr),
                           Value::scalar(p_val, mr),
                           Value::scalar(D, mr),
                           Value::scalar(D_crit, mr));
}

} // namespace numkit::stats
