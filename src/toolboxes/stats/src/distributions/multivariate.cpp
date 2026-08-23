// toolboxes/stats/src/distributions/multivariate.cpp
//
// Multivariate distribution random / cdf primitives:
//   mvnrnd  — multivariate normal random sampler
//
// Both `mvncdf` and `mvtcdf` are KNOWN GAP (need Genz quadrature for
// integration over the multivariate normal density; v1 only covers the
// random samplers). Single-dim fallback to univariate normcdf works
// trivially if needed.

#include <numkit/stats/distributions/multivariate.hpp>

#include <numkit/builtin/datafun.hpp>  // RngContext + rand/randn/randi/randperm (session-scoped, no global/mutex)
#include <numkit/builtin/specfun.hpp>  // betainc (used by mvtcdf d=1)
#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <random>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "multivariate_detail.hpp"

namespace numkit::stats {


// `R = mvnrnd(mu, Sigma)`           — one sample (length-d row)
// `R = mvnrnd(mu, Sigma, n)`        — n samples (n × d)
// `R = mvnrnd(MU, Sigma)`           — MU is n × d, one sample per row
//
// `mu` may be a row or column vector (length d) or an n×d matrix.
// Sigma is d × d, symmetric positive-definite (Cholesky check).
Value mvnrnd(::numkit::ops::RngContext &rng, const Value &mu, const Value &Sigma, std::size_t n,
             std::pmr::memory_resource *mr)
{
    if (Sigma.dims().rows() != Sigma.dims().cols())
        throw Error("mvnrnd: Sigma must be square",
                     0, 0, "mvnrnd", "", "numkit:mvnrnd:notSquare");
    const std::size_t d = Sigma.dims().rows();
    if (d == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // mu can be 1×d, d×1, or n×d. Decide n + broadcast.
    std::size_t muN = 1;
    if (!mu.isScalar()) {
        if (mu.dims().rows() == d && mu.dims().cols() == 1) muN = 1;
        else if (mu.dims().rows() == 1 && mu.dims().cols() == d) muN = 1;
        else if (mu.dims().cols() == d) muN = mu.dims().rows();
        else
            throw Error("mvnrnd: mu must be 1×d, d×1, or n×d",
                         0, 0, "mvnrnd", "", "numkit:mvnrnd:badMu");
    }

    if (n == 0) n = std::max(muN, std::size_t(1));
    if (muN > 1 && n != muN)
        throw Error("mvnrnd: n must equal rows(mu) when mu is matrix",
                     0, 0, "mvnrnd", "", "numkit:mvnrnd:nMismatch");

    ScratchArena scratch(mr);

    // Copy Sigma into a scratch column-major buffer for the in-place
    // Cholesky. We actually want ROW-major for the chol algorithm above,
    // so transpose explicitly. Sigma is symmetric so just copy as-is.
    ScratchVec<double> L(d * d, &scratch);
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            L[i * d + j] = Sigma.elemAsDouble(j * d + i);  // col-major → row-major
    choleskyLowerInPlace(L.data(), d);   // L[i*d+j] is L_ij (lower-tri)

    // mu as a flat d-length vector when row/col form; otherwise row-major n×d.
    auto getMu = [&](std::size_t row, std::size_t col) -> double {
        if (mu.isScalar()) return mu.toScalar();
        if (muN == 1) {
            // 1×d or d×1 — index by `col` regardless.
            return mu.elemAsDouble(col);
        }
        // n × d, column-major storage.
        return mu.elemAsDouble(col * muN + row);
    };

    auto out = Value::matrix(n, d, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    auto &gen = rng;
    std::normal_distribution<double> Nz(0.0, 1.0);

    ScratchVec<double> z(d, &scratch);
    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t k = 0; k < d; ++k) z[k] = Nz(gen);
        // sample = mu + L · z  (one row per call; column-major output)
        for (std::size_t j = 0; j < d; ++j) {
            double acc = 0.0;
            for (std::size_t k = 0; k <= j; ++k)
                acc += L[j * d + k] * z[k];
            od[j * n + r] = getMu(r, j) + acc;
        }
    }
    return out;
}


Value mvncdf(const Value &X, const Value &mu, const Value &Sigma,
              std::pmr::memory_resource *mr)
{
    const std::size_t n = X.dims().rows();
    const std::size_t d = X.dims().cols();
    if (d == 0)
        throw Error("mvncdf: X must have at least 1 column",
                    0, 0, "mvncdf", "", "numkit:mvncdf:badX");

    // Resolve mu (default: zeros).
    std::vector<double> muv(d, 0.0);
    if (!mu.isEmpty()) {
        if (mu.numel() != d)
            throw Error("mvncdf: mu must have length d",
                        0, 0, "mvncdf", "", "numkit:mvncdf:shapeMu");
        for (std::size_t i = 0; i < d; ++i) muv[i] = mu.elemAsDouble(i);
    }

    // Resolve Sigma (default: identity). Store row-major Cholesky factor.
    std::vector<double> L(d * d, 0.0);
    if (Sigma.isEmpty()) {
        for (std::size_t i = 0; i < d; ++i) L[i * d + i] = 1.0;
    } else {
        if (Sigma.dims().rows() != d || Sigma.dims().cols() != d)
            throw Error("mvncdf: Sigma must be d × d",
                        0, 0, "mvncdf", "", "numkit:mvncdf:shapeSigma");
        // Copy col-major Sigma to row-major buffer.
        std::vector<double> S(d * d);
        for (std::size_t i = 0; i < d; ++i)
            for (std::size_t j = 0; j < d; ++j)
                S[i * d + j] = Sigma.elemAsDouble(j * d + i);
        choleskyLowerInPlace(S.data(), d);
        L = S;
    }

    auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    if (d == 1) {
        // Univariate: (q - mu) / sqrt(Sigma).
        const double sd = L[0];
        for (std::size_t i = 0; i < n; ++i) {
            const double q = X.elemAsDouble(i);
            od[i] = phiCdf((q - muv[0]) / sd);
        }
        return out;
    }
    if (d == 2) {
        // Bivariate: standardise (q - mu) and compute the correlation.
        // Sigma = L · L', so std-devs are diag entries of L (only the
        // diagonal of L is the standard deviation when L is lower-tri).
        // Actually the std devs are sqrt of diagonal of Sigma. Get them
        // from Sigma directly.
        const double s1 = Sigma.isEmpty() ? 1.0 : std::sqrt(Sigma.elemAsDouble(0));
        const double s2 = Sigma.isEmpty() ? 1.0 : std::sqrt(Sigma.elemAsDouble(3));
        const double s12 = Sigma.isEmpty() ? 0.0 : Sigma.elemAsDouble(2);
        const double rho = (s1 > 0 && s2 > 0) ? s12 / (s1 * s2) : 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double h = (X.elemAsDouble(0 * n + i) - muv[0]) / s1;
            const double k = (X.elemAsDouble(1 * n + i) - muv[1]) / s2;
            od[i] = bivariateCdf(h, k, rho);
        }
        return out;
    }

    // d ≥ 3: Monte Carlo.
    std::mt19937 gen(12345);   // deterministic seed for reproducibility
    std::vector<double> q(d);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < d; ++j)
            q[j] = X.elemAsDouble(j * n + i);
        od[i] = monteCarloCdf(L.data(), d, muv.data(), q.data(), 20000, gen);
    }
    return out;
}

Value mvtrnd(::numkit::ops::RngContext &rng, const Value &C, double df, std::size_t n,
              std::pmr::memory_resource *mr)
{
    if (!(df > 0.0))
        throw Error("mvtrnd: df must be positive",
                    0, 0, "mvtrnd", "", "numkit:mvtrnd:badDf");
    if (C.dims().rows() != C.dims().cols())
        throw Error("mvtrnd: C must be square",
                    0, 0, "mvtrnd", "", "numkit:mvtrnd:notSquare");
    const std::size_t d = C.dims().rows();
    if (d == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Cholesky of correlation matrix.
    std::vector<double> L(d * d);
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            L[i * d + j] = C.elemAsDouble(j * d + i);
    choleskyLowerInPlace(L.data(), d);

    auto out = Value::matrix(n, d, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    auto &gen = rng;
    std::normal_distribution<double> Nz(0.0, 1.0);
    std::chi_squared_distribution<double> Chi(df);

    std::vector<double> z(d);
    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t k = 0; k < d; ++k) z[k] = Nz(gen);
        const double scale = std::sqrt(df / Chi(gen));
        for (std::size_t j = 0; j < d; ++j) {
            double v = 0.0;
            for (std::size_t k = 0; k <= j; ++k) v += L[j * d + k] * z[k];
            od[j * n + r] = v * scale;
        }
    }
    return out;
}

Value mnrnd(::numkit::ops::RngContext &rng, std::size_t N, const Value &P, std::size_t m,
              std::pmr::memory_resource *mr)
{
    const std::size_t k = P.numel();
    if (k == 0)
        throw Error("mnrnd: P must be a non-empty probability vector",
                    0, 0, "mnrnd", "", "numkit:mnrnd:emptyP");
    // Build cumulative probability table (renormalised).
    std::vector<double> cdf(k);
    double sumP = 0.0;
    for (std::size_t i = 0; i < k; ++i) {
        const double pi = P.elemAsDouble(i);
        if (pi < 0.0)
            throw Error("mnrnd: probabilities must be non-negative",
                        0, 0, "mnrnd", "", "numkit:mnrnd:negProb");
        sumP += pi;
        cdf[i] = sumP;
    }
    if (!(sumP > 0.0))
        throw Error("mnrnd: sum(P) must be positive",
                    0, 0, "mnrnd", "", "numkit:mnrnd:zeroP");
    for (auto &c : cdf) c /= sumP;

    auto out = Value::matrix(m, k, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    auto &gen = rng;
    std::uniform_real_distribution<double> U(0.0, 1.0);

    std::vector<std::size_t> counts(k);
    for (std::size_t r = 0; r < m; ++r) {
        std::fill(counts.begin(), counts.end(), 0u);
        for (std::size_t t = 0; t < N; ++t) {
            const double u = U(gen);
            // Linear scan; k is typically small.
            std::size_t cat = k - 1;
            for (std::size_t i = 0; i < k; ++i) {
                if (u <= cdf[i]) { cat = i; break; }
            }
            ++counts[cat];
        }
        for (std::size_t j = 0; j < k; ++j)
            od[j * m + r] = static_cast<double>(counts[j]);
    }
    return out;
}

// In-place inverse of a lower-triangular matrix via back-substitution.
// L is d×d lower triangular row-major; on return L holds L^{-1}.
static void invertLowerTriInPlace(double *L, std::size_t d)
{
    // Standard column-by-column back-sub:
    // For each column j, set X(j,j) = 1/L(j,j), then for i > j:
    //   X(i,j) = -(1/L(i,i)) · sum_{k=j..i-1} L(i,k) · X(k,j).
    // Easier: compute Linv ← I, then solve L · Linv = I in place,
    // writing each column.
    std::vector<double> Linv(d * d, 0.0);
    for (std::size_t j = 0; j < d; ++j) {
        // Solve L · x = e_j for x.
        // x[j] = 1/L[j][j]
        Linv[j * d + j] = 1.0 / L[j * d + j];
        for (std::size_t i = j + 1; i < d; ++i) {
            double s = 0.0;
            for (std::size_t k = j; k < i; ++k)
                s += L[i * d + k] * Linv[k * d + j];
            Linv[i * d + j] = -s / L[i * d + i];
        }
    }
    std::memcpy(L, Linv.data(), d * d * sizeof(double));
}

std::tuple<Value, Value>
wishrnd_factor(::numkit::ops::RngContext &rng, const Value &Sigma, double df, const Value &D_in,
               std::pmr::memory_resource *mr)
{
    if (Sigma.dims().rows() != Sigma.dims().cols())
        throw Error("wishrnd: Sigma must be square",
                    0, 0, "wishrnd", "", "numkit:wishrnd:notSquare");
    const std::size_t d = Sigma.dims().rows();
    if (d == 0) {
        return {Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 0, ValueType::DOUBLE, mr)};
    }
    if (!(df > static_cast<double>(d) - 1.0))
        throw Error("wishrnd: df must exceed p - 1",
                    0, 0, "wishrnd", "", "numkit:wishrnd:badDf");

    // L = chol(Sigma, 'lower'), row-major. If user supplied
    // D = chol(Sigma, 'upper'), transpose into L.
    std::vector<double> L(d * d);
    if (!D_in.isEmpty()) {
        if (D_in.dims().rows() != d || D_in.dims().cols() != d)
            throw Error("wishrnd: D must be p × p",
                        0, 0, "wishrnd", "", "numkit:wishrnd:shapeD");
        // D upper-tri: L[i,j] = D[j,i] for j ≤ i.
        for (std::size_t i = 0; i < d; ++i) {
            for (std::size_t j = 0; j < d; ++j)
                L[i * d + j] = 0.0;
            for (std::size_t j = 0; j <= i; ++j)
                L[i * d + j] = D_in.elemAsDouble(i * d + j);   // col-major D[j,i]
        }
    } else {
        for (std::size_t i = 0; i < d; ++i)
            for (std::size_t j = 0; j < d; ++j)
                L[i * d + j] = Sigma.elemAsDouble(j * d + i);
        choleskyLowerInPlace(L.data(), d);
    }

    // Sample Bartlett factor B (lower-tri).
    auto &gen = rng;
    std::normal_distribution<double> Nz(0.0, 1.0);

    std::vector<double> B(d * d, 0.0);
    for (std::size_t i = 0; i < d; ++i) {
        const double dfi = df - static_cast<double>(i);
        std::chi_squared_distribution<double> Chi(dfi);
        B[i * d + i] = std::sqrt(Chi(gen));
        for (std::size_t j = 0; j < i; ++j)
            B[i * d + j] = Nz(gen);
    }

    // M = L · B (lower-tri).
    std::vector<double> M(d * d, 0.0);
    for (std::size_t i = 0; i < d; ++i) {
        for (std::size_t j = 0; j <= i; ++j) {
            double s = 0.0;
            for (std::size_t k = j; k <= i; ++k)
                s += L[i * d + k] * B[k * d + j];
            M[i * d + j] = s;
        }
    }

    // W = M · M'.
    auto W = Value::matrix(d, d, ValueType::DOUBLE, mr);
    double *od = W.doubleDataMut();
    for (std::size_t i = 0; i < d; ++i) {
        for (std::size_t j = 0; j < d; ++j) {
            double s = 0.0;
            const std::size_t mlim = std::min(i, j);
            for (std::size_t k = 0; k <= mlim; ++k)
                s += M[i * d + k] * M[j * d + k];
            od[j * d + i] = s;
        }
    }

    // Build D = L^T (upper-tri) — col-major output.
    auto D = Value::matrix(d, d, ValueType::DOUBLE, mr);
    double *dd = D.doubleDataMut();
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            dd[j * d + i] = (j >= i) ? L[j * d + i] : 0.0;
    return {std::move(W), std::move(D)};
}

Value wishrnd(::numkit::ops::RngContext &rng, const Value &Sigma, double df,
              std::pmr::memory_resource *mr)
{
    return std::get<0>(wishrnd_factor(rng, Sigma, df, Value::Empty, mr));
}

std::tuple<Value, Value>
iwishrnd_factor(::numkit::ops::RngContext &rng, const Value &Tau, double df, const Value &DI_in,
                std::pmr::memory_resource *mr)
{
    if (Tau.dims().rows() != Tau.dims().cols())
        throw Error("iwishrnd: Tau must be square",
                    0, 0, "iwishrnd", "", "numkit:iwishrnd:notSquare");
    const std::size_t d = Tau.dims().rows();
    if (d == 0) {
        return {Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 0, ValueType::DOUBLE, mr)};
    }
    if (!(df > static_cast<double>(d) - 1.0))
        throw Error("iwishrnd: df must exceed p - 1",
                    0, 0, "iwishrnd", "", "numkit:iwishrnd:badDf");

    // Need L = chol(inv(Tau), 'lower') for Bartlett construction.
    // MATLAB convention: DI is the lower-triangular factor satisfying
    // DI' · DI = inv(Tau) — i.e. DI = chol(inv(Tau), 'lower'). So L = DI.
    std::vector<double> L(d * d);   // row-major lower-tri
    if (!DI_in.isEmpty()) {
        if (DI_in.dims().rows() != d || DI_in.dims().cols() != d)
            throw Error("iwishrnd: DI must be p × p",
                        0, 0, "iwishrnd", "", "numkit:iwishrnd:shapeDI");
        // DI lower-tri (col-major); copy directly to row-major L.
        for (std::size_t i = 0; i < d; ++i) {
            for (std::size_t j = 0; j < d; ++j) L[i * d + j] = 0.0;
            for (std::size_t j = 0; j <= i; ++j)
                L[i * d + j] = DI_in.elemAsDouble(j * d + i);   // col-major (i,j)
        }
    } else {
        // Build inv(Tau) via L_T = chol(Tau, 'lower'), then chol(inv) lower.
        std::vector<double> LT(d * d);
        for (std::size_t i = 0; i < d; ++i)
            for (std::size_t j = 0; j < d; ++j)
                LT[i * d + j] = Tau.elemAsDouble(j * d + i);
        choleskyLowerInPlace(LT.data(), d);
        std::vector<double> Linv = LT;
        invertLowerTriInPlace(Linv.data(), d);
        std::vector<double> invTau(d * d, 0.0);
        for (std::size_t i = 0; i < d; ++i) {
            for (std::size_t j = 0; j <= i; ++j) {
                double s = 0.0;
                for (std::size_t k = std::max(i, j); k < d; ++k)
                    s += Linv[k * d + i] * Linv[k * d + j];
                invTau[i * d + j] = s;
                if (i != j) invTau[j * d + i] = s;
            }
        }
        L = invTau;
        choleskyLowerInPlace(L.data(), d);
    }

    // Bartlett sample Y ~ W(inv(Tau), df).
    auto &gen = rng;
    std::normal_distribution<double> Nz(0.0, 1.0);

    std::vector<double> B(d * d, 0.0);
    for (std::size_t i = 0; i < d; ++i) {
        const double dfi = df - static_cast<double>(i);
        std::chi_squared_distribution<double> Chi(dfi);
        B[i * d + i] = std::sqrt(Chi(gen));
        for (std::size_t j = 0; j < i; ++j)
            B[i * d + j] = Nz(gen);
    }
    std::vector<double> M(d * d, 0.0);
    for (std::size_t i = 0; i < d; ++i) {
        for (std::size_t j = 0; j <= i; ++j) {
            double s = 0.0;
            for (std::size_t k = j; k <= i; ++k)
                s += L[i * d + k] * B[k * d + j];
            M[i * d + j] = s;
        }
    }
    std::vector<double> Y(d * d, 0.0);
    for (std::size_t i = 0; i < d; ++i) {
        for (std::size_t j = 0; j < d; ++j) {
            double s = 0.0;
            const std::size_t mlim = std::min(i, j);
            for (std::size_t k = 0; k <= mlim; ++k)
                s += M[i * d + k] * M[j * d + k];
            Y[i * d + j] = s;
        }
    }

    // W = inv(Y) via Cholesky.
    std::vector<double> LY = Y;
    choleskyLowerInPlace(LY.data(), d);
    invertLowerTriInPlace(LY.data(), d);
    auto W = Value::matrix(d, d, ValueType::DOUBLE, mr);
    double *od = W.doubleDataMut();
    for (std::size_t i = 0; i < d; ++i) {
        for (std::size_t j = 0; j < d; ++j) {
            double s = 0.0;
            for (std::size_t k = std::max(i, j); k < d; ++k)
                s += LY[k * d + i] * LY[k * d + j];
            od[j * d + i] = s;
        }
    }

    // DI = L (lower-tri) — MATLAB returns the LOWER factor with
    // DI' · DI = inv(Tau) (its transpose is the upper Cholesky).
    auto DI = Value::matrix(d, d, ValueType::DOUBLE, mr);
    double *dd = DI.doubleDataMut();
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            dd[j * d + i] = (j <= i) ? L[i * d + j] : 0.0;
    return {std::move(W), std::move(DI)};
}

Value iwishrnd(::numkit::ops::RngContext &rng, const Value &Tau, double df,
               std::pmr::memory_resource *mr)
{
    return std::get<0>(iwishrnd_factor(rng, Tau, df, Value::Empty, mr));
}

// ── mvtcdf ──────────────────────────────────────────────────────────


Value mvtcdf(const Value &X, const Value &C, double df, double tol,
             std::pmr::memory_resource *mr)
{
    if (!(df > 0.0))
        throw Error("mvtcdf: df must be positive",
                    0, 0, "mvtcdf", "", "numkit:mvtcdf:badDf");
    auto [d, n] = mvtcdf_shape_check(X, C, "mvtcdf");
    if (d == 0) return Value::matrix(0, 1, ValueType::DOUBLE, mr);

    auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();

    if (d == 1) {
        for (std::size_t i = 0; i < n; ++i) {
            const double xi = X.elemAsDouble(i);
            const double z = df / (df + xi * xi);
            Value zv = Value::scalar(z, mr);
            Value av = Value::scalar(0.5 * df, mr);
            Value bv = Value::scalar(0.5, mr);
            const double I = ::numkit::builtin::betainc(zv, av, bv, mr).toScalar();
            od[i] = (xi >= 0.0) ? 1.0 - 0.5 * I : 0.5 * I;
        }
        return out;
    }

    std::vector<double> L(d * d);
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            L[i * d + j] = C.elemAsDouble(j * d + i);
    choleskyLowerInPlace(L.data(), d);

    const int N = mvtcdf_N_from_tol(tol);
    std::mt19937_64 mc_gen(12345ULL);
    const double NEG_INF = -std::numeric_limits<double>::infinity();

    const std::size_t nRows = n;   // plain local: capturing a structured
                                   // binding directly in a lambda is C++20.
    for (std::size_t row = 0; row < nRows; ++row) {
        auto fetch = [&](std::size_t j) -> std::pair<double, double> {
            const double xj = X.isScalar() ? X.toScalar()
                                           : X.elemAsDouble(j * nRows + row);
            return {NEG_INF, xj};
        };
        od[row] = mvtcdf_mc(L, d, df, N, mc_gen, fetch);
    }
    return out;
}

Value mvtcdf_box(const Value &Lb, const Value &Ub, const Value &C, double df,
                 double tol, std::pmr::memory_resource *mr)
{
    if (!(df > 0.0))
        throw Error("mvtcdf: df must be positive",
                    0, 0, "mvtcdf", "", "numkit:mvtcdf:badDf");
    auto [d, n] = mvtcdf_shape_check(Lb, C, "mvtcdf");
    if (d == 0) return Value::matrix(0, 1, ValueType::DOUBLE, mr);
    // Ub must have the same shape as Lb.
    if (Ub.numel() != Lb.numel())
        throw Error("mvtcdf: L and U must have the same shape",
                    0, 0, "mvtcdf", "", "numkit:mvtcdf:shapeUL");

    auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();

    if (d == 1) {
        // 1-D box: P(L ≤ Y ≤ U) = F(U) - F(L).
        Value tu = Value::scalar(df, mr);
        for (std::size_t i = 0; i < n; ++i) {
            const double Lv = Lb.elemAsDouble(i);
            const double Uv = Ub.elemAsDouble(i);
            auto cdf_one = [&](double x) -> double {
                if (std::isinf(x)) return (x > 0) ? 1.0 : 0.0;
                const double z = df / (df + x * x);
                Value zv = Value::scalar(z, mr);
                Value av = Value::scalar(0.5 * df, mr);
                Value bv = Value::scalar(0.5, mr);
                const double I = ::numkit::builtin::betainc(zv, av, bv, mr).toScalar();
                return (x >= 0.0) ? 1.0 - 0.5 * I : 0.5 * I;
            };
            od[i] = std::max(0.0, cdf_one(Uv) - cdf_one(Lv));
        }
        return out;
    }

    std::vector<double> Lc(d * d);
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            Lc[i * d + j] = C.elemAsDouble(j * d + i);
    choleskyLowerInPlace(Lc.data(), d);

    const int N = mvtcdf_N_from_tol(tol);
    std::mt19937_64 mc_gen(12345ULL);

    const std::size_t nRows = n;   // plain local (see mvtcdf above): avoid
                                   // capturing a structured binding in a lambda.
    for (std::size_t row = 0; row < nRows; ++row) {
        auto fetch = [&](std::size_t j) -> std::pair<double, double> {
            const double lj = Lb.isScalar() ? Lb.toScalar()
                                            : Lb.elemAsDouble(j * nRows + row);
            const double uj = Ub.isScalar() ? Ub.toScalar()
                                            : Ub.elemAsDouble(j * nRows + row);
            return {lj, uj};
        };
        od[row] = mvtcdf_mc(Lc, d, df, N, mc_gen, fetch);
    }
    return out;
}

} // namespace numkit::stats
