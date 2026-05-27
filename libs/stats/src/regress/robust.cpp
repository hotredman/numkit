// libs/stats/src/regress/robust.cpp
//
// Robust regression + covariance:
//   robustfit — IRLS regression with bisquare/Huber weighting
//   robustcov — concentration-step (FAST-MCD-lite) robust covariance

#include <numkit/stats/regress/regress.hpp>

#include <numkit/stats/distributions/chi2.hpp>      // chi2inv

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace numkit::stats {

namespace {

// Solve (M · x = b) where M is p×p row-major, in-place Gauss-Jordan
// with partial pivoting. Returns false if singular.
bool gaussSolve(double *M, double *b, std::size_t p)
{
    for (std::size_t k = 0; k < p; ++k) {
        std::size_t piv = k;
        double pmax = std::fabs(M[k * p + k]);
        for (std::size_t rr = k + 1; rr < p; ++rr) {
            const double v = std::fabs(M[rr * p + k]);
            if (v > pmax) { pmax = v; piv = rr; }
        }
        if (pmax == 0.0) return false;
        if (piv != k) {
            for (std::size_t j = 0; j < p; ++j)
                std::swap(M[k * p + j], M[piv * p + j]);
            std::swap(b[k], b[piv]);
        }
        const double pv = M[k * p + k];
        for (std::size_t rr = k + 1; rr < p; ++rr) {
            const double f = M[rr * p + k] / pv;
            for (std::size_t j = k; j < p; ++j)
                M[rr * p + j] -= f * M[k * p + j];
            b[rr] -= f * b[k];
        }
    }
    for (std::size_t k = p; k-- > 0;) {
        double s = b[k];
        for (std::size_t j = k + 1; j < p; ++j)
            s -= M[k * p + j] * b[j];
        b[k] = s / M[k * p + k];
    }
    return true;
}

// Weighted OLS: solve (X' · W · X) β = X' · W · y. X is n×p col-major,
// y and w are length n. Returns β as length-p vector.
std::vector<double> weightedLS(const double *X, std::size_t n, std::size_t p,
                                const double *y, const double *w)
{
    std::vector<double> M(p * p, 0.0);
    std::vector<double> b(p, 0.0);
    for (std::size_t i = 0; i < p; ++i) {
        for (std::size_t j = 0; j < p; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k)
                s += X[i * n + k] * w[k] * X[j * n + k];
            M[i * p + j] = s;
        }
        double s = 0.0;
        for (std::size_t k = 0; k < n; ++k)
            s += X[i * n + k] * w[k] * y[k];
        b[i] = s;
    }
    if (!gaussSolve(M.data(), b.data(), p))
        std::fill(b.begin(), b.end(), std::numeric_limits<double>::quiet_NaN());
    return b;
}

// Median absolute deviation from zero (used after standardising
// residuals); MATLAB uses median of |r - median(r)|.
double medianAbsDev(std::vector<double> r)
{
    const std::size_t n = r.size();
    if (n == 0) return 0.0;
    // First find median of r.
    std::vector<double> rc(r);
    std::nth_element(rc.begin(), rc.begin() + n / 2, rc.end());
    double med = rc[n / 2];
    if (n % 2 == 0) {
        double upper = rc[n / 2];
        auto maxIt = std::max_element(rc.begin(), rc.begin() + n / 2);
        med = 0.5 * (*maxIt + upper);
    }
    // Then MAD = median(|r - med|).
    for (auto &x : r) x = std::fabs(x - med);
    std::nth_element(r.begin(), r.begin() + n / 2, r.end());
    double mad = r[n / 2];
    if (n % 2 == 0) {
        double upper = r[n / 2];
        auto maxIt = std::max_element(r.begin(), r.begin() + n / 2);
        mad = 0.5 * (*maxIt + upper);
    }
    return mad;
}

// Weight functions.
double bisquareWeight(double u)
{
    if (std::fabs(u) >= 1.0) return 0.0;
    const double t = 1.0 - u * u;
    return t * t;
}

double huberWeight(double u)
{
    const double a = std::fabs(u);
    return (a <= 1.0) ? 1.0 : 1.0 / a;
}

} // namespace

RobustfitResult robustfit(const Value &X, const Value &y,
                           RobustWeight weight, double tune,
                           std::pmr::memory_resource *mr)
{
    const std::size_t n = X.dims().rows();
    const std::size_t p = X.dims().cols();
    if (y.numel() != n)
        throw Error("robustfit: length(y) must equal rows(X)",
                    0, 0, "robustfit", "", "numkit:robustfit:shapeMismatch");
    if (p == 0 || n <= p)
        throw Error("robustfit: need rows(X) > cols(X)",
                    0, 0, "robustfit", "", "numkit:robustfit:noDOF");

    if (std::isnan(tune)) {
        tune = (weight == RobustWeight::Bisquare) ? 4.685 : 1.345;
    }

    std::vector<double> Xv(n * p);
    for (std::size_t i = 0; i < n * p; ++i) Xv[i] = X.elemAsDouble(i);
    std::vector<double> yv(n);
    for (std::size_t i = 0; i < n; ++i) yv[i] = y.elemAsDouble(i);

    // Initial OLS (uniform weights).
    std::vector<double> w(n, 1.0);
    std::vector<double> beta = weightedLS(Xv.data(), n, p, yv.data(), w.data());

    std::vector<double> r(n), beta_prev(p, 0.0);
    const int maxIter = 50;
    const double tol = 1e-8;
    double s = 1.0;
    for (int it = 0; it < maxIter; ++it) {
        // Compute residuals.
        for (std::size_t i = 0; i < n; ++i) {
            double pred = 0.0;
            for (std::size_t j = 0; j < p; ++j)
                pred += Xv[j * n + i] * beta[j];
            r[i] = yv[i] - pred;
        }

        // Scale via MAD.
        s = medianAbsDev(r) / 0.6745;
        if (!(s > 0.0)) s = 1e-12;

        // Weights.
        for (std::size_t i = 0; i < n; ++i) {
            const double u = r[i] / (tune * s);
            w[i] = (weight == RobustWeight::Bisquare)
                       ? bisquareWeight(u)
                       : huberWeight(u);
        }

        beta_prev = beta;
        beta = weightedLS(Xv.data(), n, p, yv.data(), w.data());

        // Convergence: max |Δβ| / max(|β|, eps).
        double maxStep = 0.0, maxBeta = 0.0;
        for (std::size_t j = 0; j < p; ++j) {
            maxStep = std::max(maxStep, std::fabs(beta[j] - beta_prev[j]));
            maxBeta = std::max(maxBeta, std::fabs(beta[j]));
        }
        if (maxStep < tol * std::max(maxBeta, 1.0)) break;
    }

    // Pack outputs.
    auto bv = Value::matrix(p, 1, ValueType::DOUBLE, mr);
    std::memcpy(bv.doubleDataMut(), beta.data(), p * sizeof(double));
    return { std::move(bv), Value::scalar(s, mr) };
}

namespace {

// Compute Mahalanobis distances of rows of X (col-major n × d) w.r.t.
// mean `mu` and the inverse covariance `invC`. Returns length-n vector.
std::vector<double> mahalanobisDist(const double *X, std::size_t n,
                                     std::size_t d,
                                     const double *mu, const double *invC)
{
    std::vector<double> out(n);
    std::vector<double> diff(d);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < d; ++j)
            diff[j] = X[j * n + i] - mu[j];
        double s = 0.0;
        for (std::size_t a = 0; a < d; ++a)
            for (std::size_t b = 0; b < d; ++b)
                s += diff[a] * invC[a * d + b] * diff[b];
        out[i] = s;
    }
    return out;
}

// Compute mean / cov of a column-major n × d matrix, restricted to
// rows in `keep` (sorted ascending). Returns mu (d), C (d × d row-major).
void meanCovSubset(const double *X, std::size_t n_orig, std::size_t d,
                   const std::vector<std::size_t> &keep,
                   std::vector<double> &mu, std::vector<double> &C)
{
    const std::size_t k = keep.size();
    mu.assign(d, 0.0);
    for (std::size_t i : keep)
        for (std::size_t j = 0; j < d; ++j)
            mu[j] += X[j * n_orig + i];
    for (std::size_t j = 0; j < d; ++j) mu[j] /= static_cast<double>(k);

    C.assign(d * d, 0.0);
    std::vector<double> diff(d);
    for (std::size_t i : keep) {
        for (std::size_t j = 0; j < d; ++j) diff[j] = X[j * n_orig + i] - mu[j];
        for (std::size_t a = 0; a < d; ++a)
            for (std::size_t b = 0; b < d; ++b)
                C[a * d + b] += diff[a] * diff[b];
    }
    const double denom = static_cast<double>(k > 1 ? k - 1 : 1);
    for (auto &c : C) c /= denom;
}

// Invert d×d row-major matrix via Gauss-Jordan. Returns inverse
// row-major in `out`. Returns false on singularity.
bool invertSPD(const std::vector<double> &C, std::size_t d,
                std::vector<double> &out)
{
    std::vector<double> A(d * 2 * d, 0.0);
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            A[i * 2 * d + j] = C[i * d + j];
    for (std::size_t i = 0; i < d; ++i)
        A[i * 2 * d + (d + i)] = 1.0;
    for (std::size_t k = 0; k < d; ++k) {
        std::size_t piv = k;
        double pmax = std::fabs(A[k * 2 * d + k]);
        for (std::size_t rr = k + 1; rr < d; ++rr) {
            const double v = std::fabs(A[rr * 2 * d + k]);
            if (v > pmax) { pmax = v; piv = rr; }
        }
        if (pmax == 0.0) return false;
        if (piv != k) {
            for (std::size_t j = 0; j < 2 * d; ++j)
                std::swap(A[k * 2 * d + j], A[piv * 2 * d + j]);
        }
        const double pv = A[k * 2 * d + k];
        for (std::size_t j = 0; j < 2 * d; ++j) A[k * 2 * d + j] /= pv;
        for (std::size_t rr = 0; rr < d; ++rr) {
            if (rr == k) continue;
            const double f = A[rr * 2 * d + k];
            for (std::size_t j = 0; j < 2 * d; ++j)
                A[rr * 2 * d + j] -= f * A[k * 2 * d + j];
        }
    }
    out.assign(d * d, 0.0);
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            out[i * d + j] = A[i * 2 * d + (d + j)];
    return true;
}

} // namespace

RobustcovResult robustcov(const Value &X, std::pmr::memory_resource *mr)
{
    const std::size_t n = X.dims().rows();
    const std::size_t d = X.dims().cols();
    if (n <= d + 1)
        throw Error("robustcov: need n > d + 1 observations",
                    0, 0, "robustcov", "", "numkit:robustcov:noDOF");

    std::vector<double> Xv(n * d);
    for (std::size_t i = 0; i < n * d; ++i) Xv[i] = X.elemAsDouble(i);

    // Start from classical mean / cov of all rows.
    std::vector<std::size_t> keep(n);
    for (std::size_t i = 0; i < n; ++i) keep[i] = i;
    std::vector<double> mu, C, invC;
    meanCovSubset(Xv.data(), n, d, keep, mu, C);

    const std::size_t h = static_cast<std::size_t>(
        std::ceil(0.75 * static_cast<double>(n)));

    std::vector<std::size_t> prevKeep;
    for (int it = 0; it < 30; ++it) {
        if (!invertSPD(C, d, invC)) break;
        auto dist = mahalanobisDist(Xv.data(), n, d, mu.data(), invC.data());
        // Pick h rows with smallest distance.
        std::vector<std::pair<double, std::size_t>> pairs(n);
        for (std::size_t i = 0; i < n; ++i) pairs[i] = { dist[i], i };
        std::nth_element(pairs.begin(), pairs.begin() + h, pairs.end());
        std::vector<std::size_t> newKeep(h);
        for (std::size_t i = 0; i < h; ++i) newKeep[i] = pairs[i].second;
        std::sort(newKeep.begin(), newKeep.end());
        if (newKeep == prevKeep) break;
        prevKeep = std::move(newKeep);
        meanCovSubset(Xv.data(), n, d, prevKeep, mu, C);
        keep = prevKeep;
    }

    // Consistency correction (Pison-Van Aelst-Willems 2002):
    //   c = (h/n) / F_{d+2}(F_d^{-1}(h/n))
    // where F_d is the chi-squared CDF with d DOF. For h/n = 0.75
    // and d = 2 this gives c ≈ 1.86, restoring the variance of a
    // trimmed multivariate-normal sample back to the population value.
    const double hRatio = static_cast<double>(h) / static_cast<double>(n);
    Value pVal = Value::scalar(hRatio, mr);
    const double q = chi2inv(pVal, static_cast<double>(d), mr).toScalar();
    Value qVal = Value::scalar(q, mr);
    const double F_dp2 = chi2cdf(qVal, static_cast<double>(d + 2), mr).toScalar();
    const double scale = (F_dp2 > 0.0) ? hRatio / F_dp2 : 1.0;
    for (auto &c : C) c *= scale;

    // Pack outputs.
    auto sigma = Value::matrix(d, d, ValueType::DOUBLE, mr);
    // C is row-major; transpose to col-major for Value layout.
    double *sd = sigma.doubleDataMut();
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            sd[j * d + i] = C[i * d + j];
    auto muV = Value::matrix(1, d, ValueType::DOUBLE, mr);
    std::memcpy(muV.doubleDataMut(), mu.data(), d * sizeof(double));
    return { std::move(sigma), std::move(muV) };
}

// ── Adapters ─────────────────────────────────────────────────────────
namespace detail {

void robustfit_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("robustfit: requires (X, y [, wfun [, tune]])",
                    0, 0, "robustfit", "", "numkit:robustfit:nargin");
    RobustWeight w = RobustWeight::Bisquare;
    if (args.size() >= 3 && args[2].isChar()) {
        const std::string s = args[2].toString();
        if (s == "huber")     w = RobustWeight::Huber;
        else if (s == "bisquare") w = RobustWeight::Bisquare;
        else
            throw Error("robustfit: weight must be 'bisquare' or 'huber'",
                        0, 0, "robustfit", "", "numkit:robustfit:badWeight");
    }
    double tune = std::numeric_limits<double>::quiet_NaN();
    if (args.size() >= 4 && !args[3].isEmpty())
        tune = args[3].toScalar();
    auto r = robustfit(args[0], args[1], w, tune, ctx.engine->resource());
    outs[0] = std::move(r.b);
    if (nargout > 1) outs[1] = std::move(r.s);
}

void robustcov_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("robustcov: requires (X)",
                    0, 0, "robustcov", "", "numkit:robustcov:nargin");
    auto r = robustcov(args[0], ctx.engine->resource());
    outs[0] = std::move(r.sigma);
    if (nargout > 1) outs[1] = std::move(r.mu);
}

} // namespace detail
} // namespace numkit::stats
