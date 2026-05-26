// libs/stats/src/distributions/multivariate.cpp
//
// Multivariate distribution random / cdf primitives:
//   mvnrnd  — multivariate normal random sampler
//
// Both `mvncdf` and `mvtcdf` are KNOWN GAP (need Genz quadrature for
// integration over the multivariate normal density; v1 only covers the
// random samplers). Single-dim fallback to univariate normcdf works
// trivially if needed.

#include <numkit/stats/distributions/multivariate.hpp>

#include <numkit/builtin/math/random/rng.hpp>   // sharedEngine / rngMutex
#include <numkit/builtin/math/special/special.hpp>  // betainc (used by mvtcdf d=1)
#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

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

namespace numkit::stats {

namespace {

// In-place Cholesky factorisation of an n×n symmetric positive-definite
// matrix. On return, the lower-triangular factor lives in the lower
// half of S; upper half is zeroed. Throws if S is not PD (a diagonal
// becomes ≤ 0). Algorithm: classical Cholesky-Banachiewicz.
void choleskyLowerInPlace(double *S, std::size_t n)
{
    for (std::size_t j = 0; j < n; ++j) {
        // Diagonal entry: L[j][j] = sqrt(S[j][j] - sum_{k<j} L[j][k]^2)
        double diag = S[j * n + j];
        for (std::size_t k = 0; k < j; ++k)
            diag -= S[j * n + k] * S[j * n + k];
        if (diag <= 0.0)
            throw Error("mvnrnd: Sigma must be positive definite "
                        "(Cholesky failed at row " + std::to_string(j + 1) + ")",
                         0, 0, "mvnrnd", "", "m:mvnrnd:notPD");
        const double Ljj = std::sqrt(diag);
        S[j * n + j] = Ljj;
        // Below-diagonal entries: L[i][j] = (S[i][j] - sum_{k<j} L[i][k]*L[j][k]) / L[j][j]
        for (std::size_t i = j + 1; i < n; ++i) {
            double s = S[i * n + j];
            for (std::size_t k = 0; k < j; ++k)
                s -= S[i * n + k] * S[j * n + k];
            S[i * n + j] = s / Ljj;
        }
        // Zero the strictly-upper triangle for clarity.
        for (std::size_t i = 0; i < j; ++i)
            S[i * n + j] = 0.0;
    }
}

} // namespace

// `R = mvnrnd(mu, Sigma)`           — one sample (length-d row)
// `R = mvnrnd(mu, Sigma, n)`        — n samples (n × d)
// `R = mvnrnd(MU, Sigma)`           — MU is n × d, one sample per row
//
// `mu` may be a row or column vector (length d) or an n×d matrix.
// Sigma is d × d, symmetric positive-definite (Cholesky check).
Value mvnrnd(const Value &mu, const Value &Sigma, std::size_t n,
             std::pmr::memory_resource *mr)
{
    if (Sigma.dims().rows() != Sigma.dims().cols())
        throw Error("mvnrnd: Sigma must be square",
                     0, 0, "mvnrnd", "", "m:mvnrnd:notSquare");
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
                         0, 0, "mvnrnd", "", "m:mvnrnd:badMu");
    }

    if (n == 0) n = std::max(muN, std::size_t(1));
    if (muN > 1 && n != muN)
        throw Error("mvnrnd: n must equal rows(mu) when mu is matrix",
                     0, 0, "mvnrnd", "", "m:mvnrnd:nMismatch");

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

    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::normal_distribution<double> Nz(0.0, 1.0);

    std::lock_guard<std::mutex> lk(mtx);
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

namespace {

// Standard normal CDF.
double phiCdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

// 16-point Gauss-Legendre quadrature on [0, 1] for the bivariate
// normal CDF via the parametric integral
//   Φ_2(h, k; ρ) = Φ(h) · Φ(k) + ∫_0^ρ φ_2(h, k; t) dt
// where φ_2 is the bivariate normal pdf.
double bivariateCdf(double h, double k, double rho)
{
    // Clamp ρ to avoid issues at the boundary.
    if (rho >  1.0 - 1e-12) rho =  1.0 - 1e-12;
    if (rho < -1.0 + 1e-12) rho = -1.0 + 1e-12;

    // Gauss-Legendre 16-point on [0, 1].
    static constexpr double GL_X[16] = {
        0.0052995325041750, 0.0277124884633837, 0.0671843988060841,
        0.1222977958224985, 0.1910618777986781, 0.2709916111713863,
        0.3591982246103705, 0.4524937450811813, 0.5475062549188187,
        0.6408017753896295, 0.7290083888286137, 0.8089381222013219,
        0.8777022041775015, 0.9328156011939160, 0.9722875115366163,
        0.9947004674958250
    };
    static constexpr double GL_W[16] = {
        0.0135762297058770, 0.0311267619693239, 0.0475792558412464,
        0.0623144856277781, 0.0747979944082885, 0.0845782596975013,
        0.0913017075224618, 0.0947253052275342, 0.0947253052275342,
        0.0913017075224618, 0.0845782596975013, 0.0747979944082885,
        0.0623144856277781, 0.0475792558412464, 0.0311267619693239,
        0.0135762297058770
    };

    double integral = 0.0;
    for (int i = 0; i < 16; ++i) {
        const double t = rho * GL_X[i];   // map [0, 1] → [0, ρ]
        const double r2 = 1.0 - t * t;
        // φ_2(h, k; t) at point (h, k) with corr t.
        const double quad = (h * h - 2.0 * t * h * k + k * k) / (2.0 * r2);
        const double pdf = std::exp(-quad) / (2.0 * M_PI * std::sqrt(r2));
        integral += GL_W[i] * pdf;
    }
    integral *= rho;
    return phiCdf(h) * phiCdf(k) + integral;
}

// Monte Carlo CDF (d ≥ 3): sample from N(mu, Sigma), count how many
// fall componentwise <= q. Antithetic sampling improves variance.
double monteCarloCdf(const double *L, std::size_t d,
                      const double *mu, const double *q,
                      std::size_t N, std::mt19937 &gen)
{
    std::normal_distribution<double> Nz(0.0, 1.0);
    std::size_t below = 0;
    std::vector<double> z(d), sample(d);
    const std::size_t halfN = N / 2;
    for (std::size_t s = 0; s < halfN; ++s) {
        for (std::size_t k = 0; k < d; ++k) z[k] = Nz(gen);
        // sample1 = mu + L * z, sample2 = mu - L * z  (antithetic).
        for (int sign = 0; sign < 2; ++sign) {
            const double sz = (sign == 0) ? 1.0 : -1.0;
            bool ok = true;
            for (std::size_t j = 0; j < d; ++j) {
                double v = mu[j];
                for (std::size_t k = 0; k <= j; ++k)
                    v += sz * L[j * d + k] * z[k];
                if (v > q[j]) { ok = false; break; }
            }
            if (ok) ++below;
        }
    }
    const std::size_t total = halfN * 2;
    return static_cast<double>(below) / static_cast<double>(total);
}

} // namespace

Value mvncdf(const Value &X, const Value &mu, const Value &Sigma,
              std::pmr::memory_resource *mr)
{
    const std::size_t n = X.dims().rows();
    const std::size_t d = X.dims().cols();
    if (d == 0)
        throw Error("mvncdf: X must have at least 1 column",
                    0, 0, "mvncdf", "", "m:mvncdf:badX");

    // Resolve mu (default: zeros).
    std::vector<double> muv(d, 0.0);
    if (!mu.isEmpty()) {
        if (mu.numel() != d)
            throw Error("mvncdf: mu must have length d",
                        0, 0, "mvncdf", "", "m:mvncdf:shapeMu");
        for (std::size_t i = 0; i < d; ++i) muv[i] = mu.elemAsDouble(i);
    }

    // Resolve Sigma (default: identity). Store row-major Cholesky factor.
    std::vector<double> L(d * d, 0.0);
    if (Sigma.isEmpty()) {
        for (std::size_t i = 0; i < d; ++i) L[i * d + i] = 1.0;
    } else {
        if (Sigma.dims().rows() != d || Sigma.dims().cols() != d)
            throw Error("mvncdf: Sigma must be d × d",
                        0, 0, "mvncdf", "", "m:mvncdf:shapeSigma");
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

Value mvtrnd(const Value &C, double df, std::size_t n,
              std::pmr::memory_resource *mr)
{
    if (!(df > 0.0))
        throw Error("mvtrnd: df must be positive",
                    0, 0, "mvtrnd", "", "m:mvtrnd:badDf");
    if (C.dims().rows() != C.dims().cols())
        throw Error("mvtrnd: C must be square",
                    0, 0, "mvtrnd", "", "m:mvtrnd:notSquare");
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
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::normal_distribution<double> Nz(0.0, 1.0);
    std::chi_squared_distribution<double> Chi(df);

    std::lock_guard<std::mutex> lk(mtx);
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

Value mnrnd(std::size_t N, const Value &P, std::size_t m,
              std::pmr::memory_resource *mr)
{
    const std::size_t k = P.numel();
    if (k == 0)
        throw Error("mnrnd: P must be a non-empty probability vector",
                    0, 0, "mnrnd", "", "m:mnrnd:emptyP");
    // Build cumulative probability table (renormalised).
    std::vector<double> cdf(k);
    double sumP = 0.0;
    for (std::size_t i = 0; i < k; ++i) {
        const double pi = P.elemAsDouble(i);
        if (pi < 0.0)
            throw Error("mnrnd: probabilities must be non-negative",
                        0, 0, "mnrnd", "", "m:mnrnd:negProb");
        sumP += pi;
        cdf[i] = sumP;
    }
    if (!(sumP > 0.0))
        throw Error("mnrnd: sum(P) must be positive",
                    0, 0, "mnrnd", "", "m:mnrnd:zeroP");
    for (auto &c : cdf) c /= sumP;

    auto out = Value::matrix(m, k, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::uniform_real_distribution<double> U(0.0, 1.0);

    std::lock_guard<std::mutex> lk(mtx);
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
wishrnd_factor(const Value &Sigma, double df, const Value &D_in,
               std::pmr::memory_resource *mr)
{
    if (Sigma.dims().rows() != Sigma.dims().cols())
        throw Error("wishrnd: Sigma must be square",
                    0, 0, "wishrnd", "", "m:wishrnd:notSquare");
    const std::size_t d = Sigma.dims().rows();
    if (d == 0) {
        return {Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 0, ValueType::DOUBLE, mr)};
    }
    if (!(df > static_cast<double>(d) - 1.0))
        throw Error("wishrnd: df must exceed p - 1",
                    0, 0, "wishrnd", "", "m:wishrnd:badDf");

    // L = chol(Sigma, 'lower'), row-major. If user supplied
    // D = chol(Sigma, 'upper'), transpose into L.
    std::vector<double> L(d * d);
    if (!D_in.isEmpty()) {
        if (D_in.dims().rows() != d || D_in.dims().cols() != d)
            throw Error("wishrnd: D must be p × p",
                        0, 0, "wishrnd", "", "m:wishrnd:shapeD");
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
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::normal_distribution<double> Nz(0.0, 1.0);
    std::lock_guard<std::mutex> lk(mtx);

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

Value wishrnd(const Value &Sigma, double df,
              std::pmr::memory_resource *mr)
{
    return std::get<0>(wishrnd_factor(Sigma, df, Value::Empty, mr));
}

std::tuple<Value, Value>
iwishrnd_factor(const Value &Tau, double df, const Value &DI_in,
                std::pmr::memory_resource *mr)
{
    if (Tau.dims().rows() != Tau.dims().cols())
        throw Error("iwishrnd: Tau must be square",
                    0, 0, "iwishrnd", "", "m:iwishrnd:notSquare");
    const std::size_t d = Tau.dims().rows();
    if (d == 0) {
        return {Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 0, ValueType::DOUBLE, mr)};
    }
    if (!(df > static_cast<double>(d) - 1.0))
        throw Error("iwishrnd: df must exceed p - 1",
                    0, 0, "iwishrnd", "", "m:iwishrnd:badDf");

    // Need L = chol(inv(Tau), 'lower') for Bartlett construction.
    // MATLAB convention: DI is the lower-triangular factor satisfying
    // DI' · DI = inv(Tau) — i.e. DI = chol(inv(Tau), 'lower'). So L = DI.
    std::vector<double> L(d * d);   // row-major lower-tri
    if (!DI_in.isEmpty()) {
        if (DI_in.dims().rows() != d || DI_in.dims().cols() != d)
            throw Error("iwishrnd: DI must be p × p",
                        0, 0, "iwishrnd", "", "m:iwishrnd:shapeDI");
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
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::normal_distribution<double> Nz(0.0, 1.0);
    std::lock_guard<std::mutex> lk(mtx);

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

Value iwishrnd(const Value &Tau, double df,
               std::pmr::memory_resource *mr)
{
    return std::get<0>(iwishrnd_factor(Tau, df, Value::Empty, mr));
}

// ── mvtcdf ──────────────────────────────────────────────────────────

Value mvtcdf(const Value &X, const Value &C, double df,
             std::pmr::memory_resource *mr)
{
    if (!(df > 0.0))
        throw Error("mvtcdf: df must be positive",
                    0, 0, "mvtcdf", "", "m:mvtcdf:badDf");
    // Determine dimension d from C (must be square).
    if (C.dims().rows() != C.dims().cols())
        throw Error("mvtcdf: C must be square",
                    0, 0, "mvtcdf", "", "m:mvtcdf:notSquareC");
    const std::size_t d = C.dims().rows();
    if (d == 0)
        return Value::matrix(0, 1, ValueType::DOUBLE, mr);

    // X can be length-d row or n × d.
    std::size_t n;
    if (X.isScalar()) {
        if (d != 1)
            throw Error("mvtcdf: X is scalar but d > 1",
                        0, 0, "mvtcdf", "", "m:mvtcdf:shapeX");
        n = 1;
    } else if (X.dims().rows() == 1 && X.dims().cols() == d) {
        n = 1;
    } else if (X.dims().cols() == d) {
        n = X.dims().rows();
    } else {
        throw Error("mvtcdf: X must be 1×d or n×d",
                    0, 0, "mvtcdf", "", "m:mvtcdf:shapeX");
    }

    auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();

    // d = 1: direct tcdf.
    if (d == 1) {
        for (std::size_t i = 0; i < n; ++i) {
            const double xi = X.elemAsDouble(i);
            // tcdf via I_z(ν/2, 1/2): use the existing scalar form via Value.
            // Cheaper: call a local scalar from students_t conventions.
            // Use I_z = betainc(ν/(ν+x²), ν/2, 1/2); cdf = 1 - I/2 (x≥0) else I/2.
            const double z = df / (df + xi * xi);
            Value zv = Value::scalar(z, mr);
            Value av = Value::scalar(0.5 * df, mr);
            Value bv = Value::scalar(0.5, mr);
            const double I = ::numkit::builtin::betainc(zv, av, bv, mr).toScalar();
            od[i] = (xi >= 0.0) ? 1.0 - 0.5 * I : 0.5 * I;
        }
        return out;
    }

    // d ≥ 2: deterministic Monte Carlo via Y = Z·L' / sqrt(W/df).
    // L = chol(C, 'lower') row-major.
    std::vector<double> L(d * d);
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            L[i * d + j] = C.elemAsDouble(j * d + i);
    choleskyLowerInPlace(L.data(), d);

    constexpr int N = 10000;
    std::mt19937_64 mc_gen(12345ULL);
    std::normal_distribution<double> Nz(0.0, 1.0);
    std::chi_squared_distribution<double> Chi(df);

    std::vector<double> z(d), y(d);
    // Pre-generate antithetic-pair scaffolding: for each draw we evaluate
    // both Z and -Z (sharing the same W draw) to reduce variance.
    for (std::size_t row = 0; row < n; ++row) {
        // Read X[row, :].
        std::vector<double> xrow(d);
        for (std::size_t j = 0; j < d; ++j) {
            // X stored col-major.
            xrow[j] = (X.isScalar()) ? X.toScalar()
                                     : X.elemAsDouble(j * n + row);
        }
        int hits = 0;
        for (int k = 0; k < N / 2; ++k) {
            for (std::size_t j = 0; j < d; ++j) z[j] = Nz(mc_gen);
            const double scale = std::sqrt(df / Chi(mc_gen));
            // y = (L·z)·scale.
            // Pass 1 — z direct.
            for (std::size_t j = 0; j < d; ++j) {
                double s = 0.0;
                for (std::size_t k2 = 0; k2 <= j; ++k2)
                    s += L[j * d + k2] * z[k2];
                y[j] = s * scale;
            }
            bool all_below = true;
            for (std::size_t j = 0; j < d; ++j) {
                if (y[j] > xrow[j]) { all_below = false; break; }
            }
            if (all_below) ++hits;
            // Pass 2 — antithetic (-z).
            for (std::size_t j = 0; j < d; ++j) {
                double s = 0.0;
                for (std::size_t k2 = 0; k2 <= j; ++k2)
                    s -= L[j * d + k2] * z[k2];
                y[j] = s * scale;
            }
            all_below = true;
            for (std::size_t j = 0; j < d; ++j) {
                if (y[j] > xrow[j]) { all_below = false; break; }
            }
            if (all_below) ++hits;
        }
        od[row] = static_cast<double>(hits) / static_cast<double>(N);
    }
    return out;
}

namespace detail {

void mvncdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mvncdf: requires (X [, mu, Sigma])",
                    0, 0, "mvncdf", "", "m:mvncdf:nargin");
    const Value muV    = (args.size() >= 2) ? args[1] : Value::empty();
    const Value sigmaV = (args.size() >= 3) ? args[2] : Value::empty();
    outs[0] = mvncdf(args[0], muV, sigmaV, ctx.engine->resource());
}

void mvnrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mvnrnd: requires (mu, Sigma[, n])",
                     0, 0, "mvnrnd", "", "m:mvnrnd:nargin");
    std::size_t n = 0;
    if (args.size() >= 3) n = static_cast<std::size_t>(args[2].toScalar());
    outs[0] = mvnrnd(args[0], args[1], n, ctx.engine->resource());
}

void mvtrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("mvtrnd: requires (C, df, n)",
                    0, 0, "mvtrnd", "", "m:mvtrnd:nargin");
    const double df = args[1].toScalar();
    const std::size_t n = static_cast<std::size_t>(args[2].toScalar());
    outs[0] = mvtrnd(args[0], df, n, ctx.engine->resource());
}

void mnrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mnrnd: requires (N, P [, m])",
                    0, 0, "mnrnd", "", "m:mnrnd:nargin");
    const std::size_t N = static_cast<std::size_t>(args[0].toScalar());
    std::size_t m = 1;
    if (args.size() >= 3 && !args[2].isEmpty())
        m = static_cast<std::size_t>(args[2].toScalar());
    outs[0] = mnrnd(N, args[1], m, ctx.engine->resource());
}

void wishrnd_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("wishrnd: requires (Sigma, df[, D])",
                    0, 0, "wishrnd", "", "m:wishrnd:nargin");
    const double df = args[1].toScalar();
    const Value D_in = (args.size() > 2) ? args[2] : Value::Empty;
    auto [W, D] = wishrnd_factor(args[0], df, D_in, ctx.engine->resource());
    outs[0] = std::move(W);
    if (nargout >= 2) outs[1] = std::move(D);
}

void iwishrnd_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("iwishrnd: requires (Tau, df[, DI])",
                    0, 0, "iwishrnd", "", "m:iwishrnd:nargin");
    const double df = args[1].toScalar();
    const Value DI_in = (args.size() > 2) ? args[2] : Value::Empty;
    auto [W, DI] = iwishrnd_factor(args[0], df, DI_in, ctx.engine->resource());
    outs[0] = std::move(W);
    if (nargout >= 2) outs[1] = std::move(DI);
}

void mvtcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("mvtcdf: requires (X, C, df)",
                    0, 0, "mvtcdf", "", "m:mvtcdf:nargin");
    const double df = args[2].toScalar();
    outs[0] = mvtcdf(args[0], args[1], df, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
