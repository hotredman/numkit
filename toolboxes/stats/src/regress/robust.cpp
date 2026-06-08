// toolboxes/stats/src/regress/robust.cpp
//
// Robust regression + covariance:
//   robustfit — IRLS regression with bisquare/Huber weighting
//   robustcov — concentration-step (FAST-MCD-lite) robust covariance

#include <numkit/stats/regress/regress.hpp>

#include <numkit/stats/distributions/chi2.hpp>      // chi2inv

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include "robust_detail.hpp"

namespace numkit::stats {


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

} // namespace numkit::stats
