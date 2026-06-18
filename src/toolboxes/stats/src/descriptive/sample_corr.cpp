// toolboxes/stats/src/descriptive/sample_corr.cpp
//
// Econometrics-Toolbox sample-correlation functions: autocorr (sample ACF)
// and crosscorr (sample CCF). These normalise to a correlation (lag-0 == 1 for
// the ACF) using the BIASED autocovariance estimator c(k) = (1/N) Σ (y_t-ȳ)
// (y_{t+k}-ȳ) — matching MATLAB R2025b — and report ±NumSTD/√N confidence
// bounds (default NumSTD = 2). Distinct from signal's xcorr (raw lags).
//
// parcorr (PACF) is intentionally NOT here: MATLAB's default parcorr Method is
// OLS regression (not the Durbin-Levinson recursion on the ACF), which needs a
// rank-robust least-squares path — deferred to its own change. See
// bugs/stats/autocorr.md.

#include <numkit/stats/descriptive/descriptive.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::stats {

namespace {

// Default lag count: MATLAB uses min(20, N-1).
size_t defaultNumLags(size_t N)
{
    const size_t cap = (N >= 1) ? N - 1 : 0;
    return std::min<size_t>(20, cap);
}

// Centred copy of a (vector) Value's data: out[i] = v[i] - mean(v).
void centred(const Value &v, std::vector<double> &out, double &c0)
{
    const size_t n = v.numel();
    out.resize(n);
    double mean = 0.0;
    for (size_t i = 0; i < n; ++i) mean += v.elemAsDouble(i);
    mean = (n > 0) ? mean / static_cast<double>(n) : 0.0;
    c0 = 0.0;
    for (size_t i = 0; i < n; ++i) {
        out[i] = v.elemAsDouble(i) - mean;
        c0 += out[i] * out[i];
    }
    c0 /= static_cast<double>(n);   // biased variance
}

Value colVec(const std::vector<double> &v, std::pmr::memory_resource *mr)
{
    Value out = Value::matrix(v.size(), 1, ValueType::DOUBLE, mr);
    double *d = out.doubleDataMut();
    for (size_t i = 0; i < v.size(); ++i) d[i] = v[i];
    return out;
}

} // namespace

std::tuple<Value, Value, Value>
autocorr(const Value &y, int numLags, double numSTD, std::pmr::memory_resource *mr)
{
    const size_t N = y.numel();
    const size_t K = (numLags >= 0) ? static_cast<size_t>(numLags) : defaultNumLags(N);

    std::vector<double> yc;
    double c0 = 0.0;
    centred(y, yc, c0);

    std::vector<double> acf(K + 1), lags(K + 1);
    for (size_t k = 0; k <= K; ++k) {
        double ck = 0.0;
        for (size_t t = 0; t + k < N; ++t)
            ck += yc[t] * yc[t + k];
        ck /= static_cast<double>(N);            // biased autocovariance
        acf[k]  = (c0 != 0.0) ? ck / c0 : (k == 0 ? 1.0 : 0.0);
        lags[k] = static_cast<double>(k);
    }

    const double b = (N > 0) ? numSTD / std::sqrt(static_cast<double>(N)) : 0.0;
    std::vector<double> bounds = { b, -b };
    return std::make_tuple(colVec(acf, mr), colVec(lags, mr), colVec(bounds, mr));
}

std::tuple<Value, Value, Value>
crosscorr(const Value &y1, const Value &y2, int numLags, double numSTD,
          std::pmr::memory_resource *mr)
{
    const size_t N = y1.numel();
    if (y2.numel() != N)
        throw Error("crosscorr: y1 and y2 must have the same length",
                    0, 0, "crosscorr", "", "numkit:crosscorr:lengthMismatch");
    const size_t K = (numLags >= 0) ? static_cast<size_t>(numLags) : defaultNumLags(N);

    std::vector<double> c1, c2;
    double v1 = 0.0, v2 = 0.0;
    centred(y1, c1, v1);
    centred(y2, c2, v2);
    const double denom = std::sqrt(v1 * v2);

    // xcf at lag k = (1/N) Σ_t c1[t]·c2[t+k] / √(c1(0)·c2(0)), k = -K..K.
    const long Kl = static_cast<long>(K), Nl = static_cast<long>(N);
    std::vector<double> xcf(2 * K + 1), lags(2 * K + 1);
    for (long k = -Kl; k <= Kl; ++k) {
        double s = 0.0;
        const long t0 = std::max(0L, -k), t1 = std::min(Nl, Nl - k);
        for (long t = t0; t < t1; ++t)
            s += c1[static_cast<size_t>(t)] * c2[static_cast<size_t>(t + k)];
        s /= static_cast<double>(N);
        const size_t idx = static_cast<size_t>(k + Kl);
        xcf[idx]  = (denom != 0.0) ? s / denom : 0.0;
        lags[idx] = static_cast<double>(k);
    }

    const double b = (N > 0) ? numSTD / std::sqrt(static_cast<double>(N)) : 0.0;
    std::vector<double> bounds = { b, -b };
    return std::make_tuple(colVec(xcf, mr), colVec(lags, mr), colVec(bounds, mr));
}

} // namespace numkit::stats
