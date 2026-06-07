// libs/.../hypothesis_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by hypothesis.cpp + hypothesis_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include "reduction_helpers.hpp"  // engine-free numkit::builtin::detail dim-infra (ops re-export)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
namespace {

// ── Exact Kolmogorov-Smirnov distributions ──────────────────────────
// One-sample p-values use the EXACT finite-n distribution (MATLAB's default):
// two-sided via the Marsaglia-Tsang-Wang (2003) matrix method, one-sided via
// the Birnbaum-Tingey (1951) closed form. Two-sample uses Stephens' corrected
// asymptotic. Critical values come from inverting the matching p-function.

void mMultiply(const std::vector<double> &A, const std::vector<double> &B,
               std::vector<double> &C, int m) {
    for (int i = 0; i < m; ++i)
        for (int j = 0; j < m; ++j) {
            double s = 0.0;
            for (int k = 0; k < m; ++k) s += A[i*m+k] * B[k*m+j];
            C[i*m+j] = s;
        }
}

// V = A^n with an explicit base-10 exponent eV (rescaled to dodge overflow).
void mPower(const std::vector<double> &A, int eA, std::vector<double> &V,
            int &eV, int m, int n) {
    if (n == 1) { V = A; eV = eA; return; }
    mPower(A, eA, V, eV, m, n / 2);
    std::vector<double> B(m * m);
    mMultiply(V, V, B, m);
    int eB = 2 * eV;
    if (n % 2 == 0) { V = B; eV = eB; }
    else { mMultiply(A, B, V, m); eV = eA + eB; }
    if (V[(m/2)*m + (m/2)] > 1e140) {
        for (double &x : V) x *= 1e-140;
        eV += 140;
    }
}

// P(D_n < d) — exact two-sided Kolmogorov CDF (Marsaglia-Tsang-Wang 2003).
double marsagliaK(int n, double d) {
    if (d <= 0.0) return 0.0;
    if (d >= 1.0) return 1.0;
    const double nd = n * d;
    const int k = static_cast<int>(nd) + 1;
    const int m = 2 * k - 1;
    const double h = k - nd;
    std::vector<double> H(m * m), Q(m * m, 0.0);
    for (int i = 0; i < m; ++i)
        for (int j = 0; j < m; ++j)
            H[i*m+j] = (i - j + 1 < 0) ? 0.0 : 1.0;
    for (int i = 0; i < m; ++i) {
        H[i*m + 0]     -= std::pow(h, i + 1);
        H[(m-1)*m + i] -= std::pow(h, m - i);
    }
    H[(m-1)*m + 0] += (2.0*h - 1.0 > 0.0) ? std::pow(2.0*h - 1.0, m) : 0.0;
    for (int i = 0; i < m; ++i)
        for (int j = 0; j < m; ++j)
            if (i - j + 1 > 0)
                for (int g = 1; g <= i - j + 1; ++g) H[i*m+j] /= g;
    int eQ = 0;
    mPower(H, 0, Q, eQ, m, n);
    double s = Q[(k-1)*m + (k-1)];
    for (int i = 1; i <= n; ++i) {
        s = s * i / n;
        if (s < 1e-140) { s *= 1e140; eQ -= 140; }
    }
    return s * std::pow(10.0, static_cast<double>(eQ));
}

// P(D_n^+ >= d) — exact one-sided KS survival (Birnbaum-Tingey 1951), log space.
double birnbaumTingey(int n, double d) {
    if (d <= 0.0) return 1.0;
    if (d >= 1.0) return 0.0;
    const double dn = static_cast<double>(n);
    const int jmax = static_cast<int>(std::floor(dn * (1.0 - d)));
    double sum = 0.0;
    for (int j = 0; j <= jmax; ++j) {
        const double a = 1.0 - d - j / dn;
        const double b = d + j / dn;
        if (a <= 0.0 && n - j > 0) continue;     // a^(n-j) == 0
        const double logC = std::lgamma(dn + 1.0) - std::lgamma(j + 1.0)
                          - std::lgamma(dn - j + 1.0);
        double logterm = logC + (j - 1) * std::log(b);
        if (n - j > 0) logterm += (dn - j) * std::log(a);
        sum += std::exp(logterm);
    }
    return std::min(1.0, std::max(0.0, d * sum));
}

// Two-sided one-sample p: exact for small statistics; MATLAB's corrected
// asymptotic 2·exp(-(2.000071 + .331/√n + 1.409/n)·n·D²) for large ones.
double ksOneSampleTwoSidedP(int n, double D) {
    const double s = static_cast<double>(n) * D * D;
    if (s > 7.24 || (s > 3.76 && n > 99))
        return std::min(1.0, 2.0 * std::exp(-(2.000071 + 0.331/std::sqrt((double)n)
                                              + 1.409/n) * s));
    return 1.0 - marsagliaK(n, D);
}

// One-sided one-sample p (exact Birnbaum-Tingey on the directional statistic).
double ksOneSampleOneSidedP(int n, double D) { return birnbaumTingey(n, D); }

// Two-sample KS p (Stephens' corrected asymptotic): two-sided is the
// alternating series, one-sided the single leading term.
double ksTwoSampleP(double neff, double D, bool twoSided) {
    const double lambda = (std::sqrt(neff) + 0.12 + 0.11/std::sqrt(neff)) * D;
    if (!twoSided)
        return std::min(1.0, std::max(0.0, std::exp(-2.0 * lambda * lambda)));
    double total = 0.0;
    for (int k = 1; k <= 101; ++k) {
        const double term = std::exp(-2.0 * lambda * lambda * k * k);
        total += (k & 1) ? term : -term;
        if (term < 1e-20 && k > 4) break;
    }
    return std::min(1.0, std::max(0.0, 2.0 * total));
}

// Critical value: invert a monotone-decreasing p(D) so that p(cv) = alpha.
template <typename PFn>
double ksCriticalValue(PFn pfn, double alpha) {
    double lo = 0.0, hi = 1.0;
    for (int it = 0; it < 100; ++it) {
        const double mid = 0.5 * (lo + hi);
        if (pfn(mid) > alpha) lo = mid; else hi = mid;
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

// vartestn_full: vartestn worker -> [p, stat, df1, df2]. Def in hypothesis.cpp.
std::tuple<Value, Value, Value, Value>
vartestn_full(const Value &x, const Value &group, int test,
              std::pmr::memory_resource *mr);

} // namespace numkit::stats
