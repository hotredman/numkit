// libs/stats/src/test/ad_dw.cpp
//
// Two classical hypothesis tests:
//   adtest  — Anderson-Darling test for normality (parameters estimated)
//   dwtest  — Durbin-Watson test for first-order autocorrelation in
//             regression residuals

#include <numkit/stats/test/hypothesis.hpp>

#include <numkit/stats/distributions/normal.hpp>
#include <numkit/stats/distributions/beta.hpp>
#include <numkit/linalg/eig.hpp>   // eig_symmetric — exact DW p-value (Imhof)

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace numkit::stats {

namespace {

// p-value formula from D'Agostino & Stephens (1986), Goodness-of-Fit
// Techniques, table 4.7 — parameters-estimated A² statistic.
double adPvalueEstimatedParams(double A2star)
{
    double p;
    if (A2star < 0.200) {
        p = 1.0 - std::exp(-13.436 + 101.14 * A2star - 223.73 * A2star * A2star);
    } else if (A2star < 0.340) {
        p = 1.0 - std::exp(-8.318 + 42.796 * A2star - 59.938 * A2star * A2star);
    } else if (A2star < 0.600) {
        p = std::exp(0.9177 - 4.279 * A2star - 1.38 * A2star * A2star);
    } else if (A2star < 13.0) {
        p = std::exp(1.2937 - 5.709 * A2star + 0.0186 * A2star * A2star);
    } else {
        p = 0.0;
    }
    return std::max(0.0, std::min(1.0, p));
}

} // namespace

std::tuple<Value, Value, Value, Value>
adtest(const Value &x, double alpha, std::pmr::memory_resource *mr)
{
    if (alpha <= 0.0 || alpha >= 1.0)
        throw Error("adtest: alpha must be in (0, 1)",
                    0, 0, "adtest", "", "numkit:adtest:badAlpha");

    // Pull data as DOUBLE vector, drop NaNs.
    const std::size_t N = x.numel();
    std::vector<double> v;
    v.reserve(N);
    for (std::size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        if (std::isnan(xi)) continue;
        v.push_back(xi);
    }
    const std::size_t n = v.size();
    if (n < 4)
        throw Error("adtest: need at least 4 non-NaN observations",
                    0, 0, "adtest", "", "numkit:adtest:tooFewObs");

    // Sample mean and unbiased std.
    double mean = 0.0;
    for (double xi : v) mean += xi;
    mean /= static_cast<double>(n);
    double s2 = 0.0;
    for (double xi : v) {
        const double d = xi - mean;
        s2 += d * d;
    }
    s2 /= static_cast<double>(n - 1);
    const double sd = std::sqrt(s2);
    if (!(sd > 0.0))
        throw Error("adtest: sample has zero variance",
                    0, 0, "adtest", "", "numkit:adtest:zeroVar");

    // Standardise + sort.
    std::vector<double> z = v;
    for (double &zi : z) zi = (zi - mean) / sd;
    std::sort(z.begin(), z.end());

    // A² = -n - (1/n) Σ (2i-1) · [ln Φ(z_i) + ln(1 - Φ(z_{n+1-i}))]
    // Use stable: ln(1 - Φ(z)) for large z via the complementary form
    // — for our z values that come from a finite sample the standard
    // form is OK.
    double sum = 0.0;
    constexpr double SQRT2 = 1.41421356237309504880;
    for (std::size_t i = 0; i < n; ++i) {
        const double Phi_lo = 0.5 * std::erfc(-z[i] / SQRT2);
        const double Phi_hi = 0.5 * std::erfc(-z[n - 1 - i] / SQRT2);
        const double lnLo = std::log(std::max(Phi_lo, 1e-300));
        const double lnHi = std::log(std::max(1.0 - Phi_hi, 1e-300));
        sum += (2.0 * static_cast<double>(i) + 1.0) * (lnLo + lnHi);
    }
    const double A2 = -static_cast<double>(n) - sum / static_cast<double>(n);

    // Stephens 1986 small-sample adjustment for parameters estimated.
    const double nD = static_cast<double>(n);
    const double A2star = A2 * (1.0 + 0.75 / nD + 2.25 / (nD * nD));

    // p-value via piecewise rational fit.
    const double p = adPvalueEstimatedParams(A2star);

    // Critical value for parameters-estimated AD at alpha = 0.05 is
    // 0.752; we expose this as the 4th output regardless of `alpha`
    // (MATLAB does the same — it's the standard reference critical
    // for normality with estimated parameters).
    const double cv = 0.752;

    const int h = (p < alpha) ? 1 : 0;
    return {
        Value::scalar(static_cast<double>(h), mr),
        Value::scalar(p,       mr),
        Value::scalar(A2star,  mr),
        Value::scalar(cv,      mr),
    };
}

namespace {

constexpr double kPi = 3.141592653589793;

// Invert a small D×D row-major matrix in place via Gauss-Jordan with partial
// pivoting. Throws on (near-)singularity.
void invertSmall(std::vector<double> &A, std::size_t D)
{
    std::vector<double> aug(D * 2 * D, 0.0);
    for (std::size_t r = 0; r < D; ++r) {
        for (std::size_t c = 0; c < D; ++c) aug[r * 2 * D + c] = A[r * D + c];
        aug[r * 2 * D + D + r] = 1.0;
    }
    for (std::size_t kk = 0; kk < D; ++kk) {
        std::size_t piv = kk; double pmax = std::fabs(aug[kk * 2 * D + kk]);
        for (std::size_t r = kk + 1; r < D; ++r) {
            const double v = std::fabs(aug[r * 2 * D + kk]);
            if (v > pmax) { pmax = v; piv = r; }
        }
        if (pmax < 1e-14)
            throw Error("dwtest: design matrix X is rank-deficient (X''X singular)",
                        0, 0, "dwtest", "", "numkit:dwtest:singular");
        if (piv != kk)
            for (std::size_t c = 0; c < 2 * D; ++c)
                std::swap(aug[kk * 2 * D + c], aug[piv * 2 * D + c]);
        const double inv = 1.0 / aug[kk * 2 * D + kk];
        for (std::size_t c = 0; c < 2 * D; ++c) aug[kk * 2 * D + c] *= inv;
        for (std::size_t r = 0; r < D; ++r) {
            if (r == kk) continue;
            const double f = aug[r * 2 * D + kk];
            if (f == 0.0) continue;
            for (std::size_t c = 0; c < 2 * D; ++c)
                aug[r * 2 * D + c] -= f * aug[kk * 2 * D + c];
        }
    }
    for (std::size_t r = 0; r < D; ++r)
        for (std::size_t c = 0; c < D; ++c)
            A[r * D + c] = aug[r * 2 * D + D + c];
}

// Imhof (1961) integrand g(u) = sin(θ(u)) / (u·ρ(u)) for Q = Σ h_j Z_j²
// (Z_j iid N(0,1), each central χ²₁):
//   θ(u) = ½ Σ atan(h_j u),  ρ(u) = Π (1 + h_j² u²)^{¼}.
// The u→0 limit is ½ Σ h_j.
double imhofIntegrand(double u, const std::vector<double> &h)
{
    if (u <= 0.0) {
        double s = 0.0; for (double hj : h) s += hj; return 0.5 * s;
    }
    double theta = 0.0, logrho = 0.0;
    for (double hj : h) {
        const double hu = hj * u;
        theta  += std::atan(hu);
        logrho += 0.25 * std::log1p(hu * hu);
    }
    theta *= 0.5;
    return std::sin(theta) / (u * std::exp(logrho));
}

// Exact P(Q < 0) for Q = Σ h_j Z_j² via Imhof's CF inversion:
//   P(Q < 0) = ½ − (1/π) ∫_0^∞ g(u) du.
// The infinite tail is mapped to [0,1) with u = t/(1−t) (du = dt/(1−t)²) and
// integrated with composite Simpson. Used as pLeft = P(DW < dw).
double imhofPLeft(const std::vector<double> &h)
{
    const std::size_t m = h.size();
    if (m == 0) return 0.5;
    if (m == 1) return (h[0] < 0.0) ? 1.0 : (h[0] > 0.0 ? 0.0 : 0.5);

    const int N = 6000;                 // even panel count
    auto Ft = [&](double t) -> double {
        if (t >= 1.0) return 0.0;
        const double u = t / (1.0 - t);
        const double w = 1.0 / ((1.0 - t) * (1.0 - t));
        return imhofIntegrand(u, h) * w;
    };
    double s = Ft(0.0) + Ft(1.0);
    const double step = 1.0 / N;
    for (int i = 1; i < N; ++i) {
        const double t = i * step;
        s += ((i & 1) ? 4.0 : 2.0) * Ft(t);
    }
    const double integral = s * step / 3.0;
    double p = 0.5 - integral / kPi;
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    return p;
}

// Exact left-tail Durbin–Watson p-value pLeft = P(DW < dw) under H0.
// DW = e'Ae / e'e; under H0 e = Mε (M = I − X(X'X)⁻¹X', ε ~ N(0,σ²I)), so
// DW < dw ⟺ ε'M(A−dw·I)Mε < 0. The nonzero eigenvalues λ_j of MAM give
// Q = Σ (λ_j − dw) Z_j²; Imhof inverts its CF. A is the first-difference
// tridiagonal (diag 1,2,…,2,1; off-diag −1).
double dwExactPLeft(const Value &X, std::size_t n, std::size_t k, double dw,
                    std::pmr::memory_resource *mr)
{
    // X'X (k×k) and its inverse.
    std::vector<double> XtX(k * k, 0.0);
    for (std::size_t a = 0; a < k; ++a)
        for (std::size_t b = 0; b < k; ++b) {
            double sab = 0.0;
            for (std::size_t i = 0; i < n; ++i)
                sab += X.elemAsDouble(a * n + i) * X.elemAsDouble(b * n + i);
            XtX[a * k + b] = sab;
        }
    invertSmall(XtX, k);

    // P = X · (X'X)⁻¹  (n×k, row-major): P[i,a] = Σ_b X[i,b] inv[b,a].
    std::vector<double> P(n * k, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t a = 0; a < k; ++a) {
            double s = 0.0;
            for (std::size_t b = 0; b < k; ++b)
                s += X.elemAsDouble(b * n + i) * XtX[b * k + a];
            P[i * k + a] = s;
        }
    // M = I − P·X'  (n×n symmetric, row-major): M[i,j] = δij − Σ_a P[i,a] X[j,a].
    std::vector<double> M(n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (std::size_t a = 0; a < k; ++a)
                s += P[i * k + a] * X.elemAsDouble(a * n + j);
            M[i * n + j] = (i == j ? 1.0 : 0.0) - s;
        }
    // AM = A·M with A the DW tridiagonal applied row-wise (O(n²)).
    std::vector<double> AM(n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) {
            double v = (i == 0 || i + 1 == n ? 1.0 : 2.0) * M[i * n + j];
            if (i > 0)     v -= M[(i - 1) * n + j];
            if (i + 1 < n) v -= M[(i + 1) * n + j];
            AM[i * n + j] = v;
        }
    // S = M·AM (n×n symmetric) as a Value for eig_symmetric (column-major).
    Value S = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *sd = S.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (std::size_t l = 0; l < n; ++l) s += M[i * n + l] * AM[l * n + j];
            sd[j * n + i] = s;   // column-major store
        }
    auto [V, D] = ::numkit::linalg::eig_symmetric(S, mr);
    (void)V;
    std::vector<double> eig(n);
    { const double *dd = D.doubleData();
      for (std::size_t i = 0; i < n; ++i) eig[i] = dd[i + i * n]; }
    std::sort(eig.begin(), eig.end());
    // Drop the k structural zeros (col(X) directions); keep the n−k residual-
    // space eigenvalues, then h_j = λ_j − dw.
    std::vector<double> h;
    h.reserve(n - k);
    for (std::size_t i = k; i < n; ++i) h.push_back(eig[i] - dw);
    return imhofPLeft(h);
}

// Approximate left-tail p (Durbin & Watson 1971 beta-on-[0,4] moment fit) —
// the 'approximate' method and large-n fallback.
double dwApproxPLeft(double dw, std::size_t n, std::pmr::memory_resource *mr)
{
    const double a = 0.5 * (static_cast<double>(n) - 1.0);
    Value pv = betacdf(Value::scalar(dw / 4.0, mr), a, a, mr);
    return pv.toScalar();
}

} // namespace

// Shared front-end: validates inputs, computes the DW statistic, and the
// left-tail p (P(DW < dw)) by the requested method. `exact==true` uses Imhof's
// exact distribution (MATLAB default); otherwise the beta approximation.
static double dwStatAndPLeft(const Value &r, const Value &X, bool exact,
                             double &dwOut, std::pmr::memory_resource *mr)
{
    const std::size_t n = r.numel();
    if (n < 3)
        throw Error("dwtest: need at least 3 residuals",
                    0, 0, "dwtest", "", "numkit:dwtest:tooFewObs");
    const std::size_t k = X.dims().cols();
    if (X.dims().rows() != n)
        throw Error("dwtest: rows(X) must equal length(r)",
                    0, 0, "dwtest", "", "numkit:dwtest:shapeMismatch");
    if (k >= n)
        throw Error("dwtest: design matrix has no degrees of freedom",
                    0, 0, "dwtest", "", "numkit:dwtest:noDOF");

    std::vector<double> rv(n);
    for (std::size_t i = 0; i < n; ++i) rv[i] = r.elemAsDouble(i);
    double num = 0.0, den = 0.0;
    for (std::size_t i = 0; i < n; ++i) den += rv[i] * rv[i];
    for (std::size_t i = 1; i < n; ++i) { const double d = rv[i] - rv[i - 1]; num += d * d; }
    if (!(den > 0.0))
        throw Error("dwtest: residuals are all zero",
                    0, 0, "dwtest", "", "numkit:dwtest:zeroResid");
    dwOut = num / den;
    return exact ? dwExactPLeft(X, n, k, dwOut, mr)
                 : dwApproxPLeft(dwOut, n, mr);
}

std::tuple<Value, Value>
dwtest(const Value &r, const Value &X, std::pmr::memory_resource *mr)
{
    // Default: exact (MATLAB) method, two-sided ('both') tail.
    double dw = 0.0;
    const double pLeft = dwStatAndPLeft(r, X, /*exact=*/true, dw, mr);
    const double pBoth = std::min(1.0, 2.0 * std::min(pLeft, 1.0 - pLeft));
    return { Value::scalar(pBoth, mr), Value::scalar(dw, mr) };
}

// ── Engine adapters ─────────────────────────────────────────────────
namespace detail {

void adtest_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("adtest: requires (x [, alpha])",
                    0, 0, "adtest", "", "numkit:adtest:nargin");
    double alpha = 0.05;
    if (args.size() >= 2 && !args[1].isEmpty())
        alpha = args[1].toScalar();
    auto [h, p, stat, cv] = adtest(args[0], alpha, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(stat);
    if (nargout > 3) outs[3] = std::move(cv);
}

void dwtest_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dwtest: requires (residuals, design)",
                    0, 0, "dwtest", "", "numkit:dwtest:nargin");
    auto *mr = ctx.engine->resource();

    auto toLowerAscii = [](std::string s) {
        for (char &c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        return s;
    };

    // Optional name/value options: 'Tail' (both|right|left, default both) and
    // 'Method' (exact|approximate, default exact — matches MATLAB).
    int  tail  = 0;        // 0=both, 1=right (positive ac), 2=left (negative ac)
    bool exact = true;
    for (size_t i = 2; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("dwtest: expected option name (string)",
                        0, 0, "dwtest", "", "numkit:dwtest:badOption");
        const std::string key = toLowerAscii(args[i].toString());
        const std::string val = toLowerAscii(args[i + 1].toString());
        if (key == "tail") {
            if (val == "both")       tail = 0;
            else if (val == "right") tail = 1;
            else if (val == "left")  tail = 2;
            else throw Error("dwtest: Tail must be 'both', 'right' or 'left'",
                             0, 0, "dwtest", "", "numkit:dwtest:badOption");
        } else if (key == "method") {
            if (val == "exact")            exact = true;
            else if (val == "approximate") exact = false;
            else throw Error("dwtest: Method must be 'exact' or 'approximate'",
                             0, 0, "dwtest", "", "numkit:dwtest:badOption");
        } else {
            throw Error("dwtest: unsupported option '" + key + "'",
                        0, 0, "dwtest", "", "numkit:dwtest:badOption");
        }
    }
    if ((args.size() - 2) % 2 != 0)
        throw Error("dwtest: option name without value",
                    0, 0, "dwtest", "", "numkit:dwtest:badOption");

    double dw = 0.0;
    const double pLeft = dwStatAndPLeft(args[0], args[1], exact, dw, mr);
    double p;
    if (tail == 1)      p = pLeft;                  // 'right': positive autocorrelation
    else if (tail == 2) p = 1.0 - pLeft;            // 'left' : negative autocorrelation
    else                p = std::min(1.0, 2.0 * std::min(pLeft, 1.0 - pLeft)); // 'both'
    outs[0] = Value::scalar(p, mr);
    if (nargout > 1) outs[1] = Value::scalar(dw, mr);
}

} // namespace detail
} // namespace numkit::stats
