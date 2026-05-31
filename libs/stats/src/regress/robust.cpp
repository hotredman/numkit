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
#include <cctype>
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


// MATLAB robustfit weight functions (statrobustwfun.m). Argument is the
// standardised, leverage-adjusted residual u = radj / (max(s,tiny_s)*tune).
constexpr double kSqrtEps = 1.4901161193847656e-08;  // sqrt(eps)
#ifndef M_PI
constexpr double M_PI = 3.14159265358979323846;
#endif

double robustWeight(RobustWeight wt, double u)
{
    const double a = std::fabs(u);
    switch (wt) {
        case RobustWeight::Andrews: {
            const double r = std::max(kSqrtEps, a);
            return (r < M_PI) ? std::sin(r) / r : 0.0;
        }
        case RobustWeight::Bisquare: {
            if (a >= 1.0) return 0.0;
            const double t = 1.0 - u * u;
            return t * t;
        }
        case RobustWeight::Cauchy:   return 1.0 / (1.0 + u * u);
        case RobustWeight::Fair:     return 1.0 / (1.0 + a);
        case RobustWeight::Huber:    return 1.0 / std::max(1.0, a);
        case RobustWeight::Logistic: {
            const double r = std::max(kSqrtEps, a);
            return std::tanh(r) / r;
        }
        case RobustWeight::Ols:      return 1.0;
        case RobustWeight::Talwar:   return (a < 1.0) ? 1.0 : 0.0;
        case RobustWeight::Welsch:   return std::exp(-(u * u));
    }
    return 1.0;
}

double defaultTune(RobustWeight wt)
{
    switch (wt) {
        case RobustWeight::Andrews:  return 1.339;
        case RobustWeight::Bisquare: return 4.685;
        case RobustWeight::Cauchy:   return 2.385;
        case RobustWeight::Fair:     return 1.400;
        case RobustWeight::Huber:    return 1.345;
        case RobustWeight::Logistic: return 1.205;
        case RobustWeight::Ols:      return 1.0;
        case RobustWeight::Talwar:   return 2.795;
        case RobustWeight::Welsch:   return 2.985;
    }
    return 1.0;
}

// MATLAB madsigma: sort |r| ascending, drop the smallest (p-1), take the
// median of the rest, divide by 0.6745.
double madsigma(std::vector<double> r, std::size_t p)
{
    const std::size_t n = r.size();
    for (double &v : r) v = std::fabs(v);
    std::sort(r.begin(), r.end());
    const std::size_t start = (p >= 1) ? (p - 1) : 0;   // 0-based first kept
    const std::size_t m = (start < n) ? (n - start) : 0;
    if (m == 0) return 0.0;
    double med;
    if (m & 1) med = r[start + m / 2];
    else       med = 0.5 * (r[start + m / 2 - 1] + r[start + m / 2]);
    return med / 0.6745;
}

// Sample standard deviation (N-1) used for tiny_s.
double sampleStd(const std::vector<double> &v)
{
    const std::size_t n = v.size();
    if (n < 2) return 0.0;
    double s = 0.0;
    for (double e : v) s += e;
    const double m = s / double(n);
    double ss = 0.0;
    for (double e : v) ss += (e - m) * (e - m);
    return std::sqrt(ss / double(n - 1));
}

// Hat-matrix (leverage) diagonal of X (col-major n×p) via (X'X)^-1.
// h_i = x_i' (X'X)^-1 x_i. Returns NaN-free leverages clamped at 0.9999.
std::vector<double> leverage(const double *X, std::size_t n, std::size_t p)
{
    std::vector<double> XtX(p * p, 0.0);
    for (std::size_t j = 0; j < p; ++j)
        for (std::size_t k = 0; k < p; ++k) {
            double s = 0.0;
            for (std::size_t i = 0; i < n; ++i)
                s += X[j * n + i] * X[k * n + i];
            XtX[j * p + k] = s;
        }
    // Gauss-Jordan inverse of the p×p matrix.
    std::vector<double> inv(p * p, 0.0);
    for (std::size_t i = 0; i < p; ++i) inv[i * p + i] = 1.0;
    for (std::size_t col = 0; col < p; ++col) {
        std::size_t piv = col;
        double best = std::fabs(XtX[col * p + col]);
        for (std::size_t r = col + 1; r < p; ++r) {
            const double v = std::fabs(XtX[r * p + col]);
            if (v > best) { best = v; piv = r; }
        }
        if (piv != col)
            for (std::size_t c = 0; c < p; ++c) {
                std::swap(XtX[col * p + c], XtX[piv * p + c]);
                std::swap(inv[col * p + c], inv[piv * p + c]);
            }
        const double d = XtX[col * p + col];
        const double invd = (d != 0.0) ? 1.0 / d : 0.0;
        for (std::size_t c = 0; c < p; ++c) {
            XtX[col * p + c] *= invd;
            inv[col * p + c] *= invd;
        }
        for (std::size_t r = 0; r < p; ++r) {
            if (r == col) continue;
            const double f = XtX[r * p + col];
            if (f == 0.0) continue;
            for (std::size_t c = 0; c < p; ++c) {
                XtX[r * p + c] -= f * XtX[col * p + c];
                inv[r * p + c] -= f * inv[col * p + c];
            }
        }
    }
    std::vector<double> h(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        double s = 0.0;
        for (std::size_t j = 0; j < p; ++j) {
            double t = 0.0;
            for (std::size_t k = 0; k < p; ++k)
                t += inv[j * p + k] * X[k * n + i];
            s += X[j * n + i] * t;
        }
        h[i] = std::min(0.9999, s);
    }
    return h;
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

    if (std::isnan(tune)) tune = defaultTune(weight);

    std::vector<double> Xv(n * p);
    for (std::size_t i = 0; i < n * p; ++i) Xv[i] = X.elemAsDouble(i);
    std::vector<double> yv(n);
    for (std::size_t i = 0; i < n; ++i) yv[i] = y.elemAsDouble(i);

    // Leverage adjustment (DuMouchel & O'Brien): radj = r / sqrt(1 - h),
    // h = hat-matrix diagonal. MATLAB statrobustfit.m.
    const std::vector<double> h = leverage(Xv.data(), n, p);
    std::vector<double> adj(n);
    for (std::size_t i = 0; i < n; ++i)
        adj[i] = 1.0 / std::sqrt(1.0 - h[i]);

    // Floor on the scale estimate so a (near-)perfect fit can't drive s to 0.
    double tiny_s = 1e-6 * sampleStd(yv);
    if (!(tiny_s > 0.0)) tiny_s = 1.0;

    // Initial OLS (uniform weights).
    std::vector<double> w(n, 1.0);
    std::vector<double> beta = weightedLS(Xv.data(), n, p, yv.data(), w.data());

    std::vector<double> r(n), radj(n), beta_prev(p, 0.0);
    const int maxIter = 50;
    const double D = 1.4901161193847656e-08;   // sqrt(eps)
    double s = 1.0;
    for (int it = 0; it < maxIter; ++it) {
        // Residuals from the current fit, then the leverage-adjusted scale.
        for (std::size_t i = 0; i < n; ++i) {
            double pred = 0.0;
            for (std::size_t j = 0; j < p; ++j)
                pred += Xv[j * n + i] * beta[j];
            r[i] = yv[i] - pred;
            radj[i] = r[i] * adj[i];
        }
        s = madsigma(radj, p);
        const double denom = std::max(s, tiny_s) * tune;

        // New weights from the standardised, leverage-adjusted residuals.
        for (std::size_t i = 0; i < n; ++i)
            w[i] = robustWeight(weight, radj[i] / denom);

        beta_prev = beta;
        beta = weightedLS(Xv.data(), n, p, yv.data(), w.data());

        // MATLAB: stop when no |b-b0| exceeds D*max(|b|,|b0|).
        bool changed = false;
        for (std::size_t j = 0; j < p; ++j)
            if (std::fabs(beta[j] - beta_prev[j])
                > D * std::max(std::fabs(beta[j]), std::fabs(beta_prev[j])))
                changed = true;
        if (!changed) break;
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
        std::string s = args[2].toString();
        for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (s == "andrews")  w = RobustWeight::Andrews;
        else if (s == "bisquare") w = RobustWeight::Bisquare;
        else if (s == "cauchy")   w = RobustWeight::Cauchy;
        else if (s == "fair")     w = RobustWeight::Fair;
        else if (s == "huber")    w = RobustWeight::Huber;
        else if (s == "logistic") w = RobustWeight::Logistic;
        else if (s == "ols")      w = RobustWeight::Ols;
        else if (s == "talwar")   w = RobustWeight::Talwar;
        else if (s == "welsch")   w = RobustWeight::Welsch;
        else
            throw Error("robustfit: weight must be one of 'andrews', "
                        "'bisquare', 'cauchy', 'fair', 'huber', 'logistic', "
                        "'ols', 'talwar', 'welsch'",
                        0, 0, "robustfit", "", "numkit:robustfit:badWeight");
    }
    double tune = std::numeric_limits<double>::quiet_NaN();
    if (args.size() >= 4 && !args[3].isEmpty())
        tune = args[3].toScalar();

    // MATLAB robustfit adds a constant (intercept) term by default; the
    // 5th argument 'const' = 'on' (default) | 'off' toggles it. With the
    // intercept, b = [b0; slopes...] (b0 first). numkit previously fit the
    // raw X with no intercept, returning the wrong number of coefficients.
    bool addConst = true;
    if (args.size() >= 5 && args[4].isChar()) {
        const std::string c = args[4].toString();
        if (c == "off" || c == "Off" || c == "OFF")       addConst = false;
        else if (c == "on" || c == "On" || c == "ON")     addConst = true;
        else
            throw Error("robustfit: const must be 'on' or 'off'",
                        0, 0, "robustfit", "", "numkit:robustfit:badConst");
    }

    Value Xin = args[0];
    if (addConst) {
        const std::size_t n = Xin.dims().rows();
        const std::size_t p = Xin.dims().cols();
        Value Xaug = Value::matrix(n, p + 1, ValueType::DOUBLE,
                                   ctx.engine->resource());
        double *xa = Xaug.doubleDataMut();
        for (std::size_t i = 0; i < n; ++i) xa[i] = 1.0;        // intercept col
        for (std::size_t j = 0; j < p; ++j)
            for (std::size_t i = 0; i < n; ++i)
                xa[(j + 1) * n + i] = Xin.elemAsDouble(j * n + i);
        Xin = std::move(Xaug);
    }

    auto r = robustfit(Xin, args[1], w, tune, ctx.engine->resource());
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
