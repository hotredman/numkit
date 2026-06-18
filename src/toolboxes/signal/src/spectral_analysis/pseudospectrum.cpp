// toolboxes/signal/src/spectral_analysis/pseudospectrum.cpp
//
// Subspace pseudospectrum estimators: pmusic (MUSIC) and peig (eigenvector).
// Both build the signal correlation matrix R = X'·X via corrmtx, eigendecompose
// it (classical Jacobi — toolboxes carry their own small symmetric eigensolver,
// as in stats/pca), split into signal (top p) / noise (bottom M-p) subspaces,
// then evaluate
//   P(w) = 1 / Σ_{noise} |e(w)'·v_k|²            (pmusic)
//   P(w) = 1 / Σ_{noise} |e(w)'·v_k|² / λ_k      (peig)
// over a one-sided frequency grid. Matches MATLAB on the PEAK FREQUENCIES (this
// is a frequency estimator); the absolute pseudospectrum is scale-arbitrary and
// eigendecomposition-sensitive (peaks are 1/near-zero), so it is NOT bit-matched.
// bugs/signal/pmusic-peig.

#include <numkit/signal/spectral_analysis/signal_modeling.hpp>   // corrmtx, pmusic, peig

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <tuple>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

// Classical Jacobi eigendecomposition of a D×D symmetric matrix A (row-major).
// On exit A's diagonal holds the eigenvalues; V's columns are the eigenvectors.
void jacobiEig(std::vector<double> &A, std::vector<double> &V, size_t D)
{
    V.assign(D * D, 0.0);
    for (size_t i = 0; i < D; ++i) V[i * D + i] = 1.0;
    for (int sweep = 0; sweep < 60; ++sweep) {
        double off = 0.0;
        for (size_t p = 0; p < D; ++p)
            for (size_t q = p + 1; q < D; ++q) off += A[p * D + q] * A[p * D + q];
        if (off < 1e-18) break;
        for (size_t p = 0; p + 1 < D; ++p)
            for (size_t q = p + 1; q < D; ++q) {
                const double apq = A[p * D + q];
                if (std::fabs(apq) < 1e-300) continue;
                const double app = A[p * D + p], aqq = A[q * D + q];
                const double theta = (aqq - app) / (2.0 * apq);
                const double sgn   = (theta >= 0) ? 1.0 : -1.0;
                const double t     = (theta == 0.0) ? 1.0
                                      : sgn / (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0);
                const double s = t * c;
                A[p * D + p] = app - t * apq;
                A[q * D + q] = aqq + t * apq;
                A[p * D + q] = 0.0;
                A[q * D + p] = 0.0;
                for (size_t r = 0; r < D; ++r) {
                    if (r == p || r == q) continue;
                    const double arp = A[r * D + p], arq = A[r * D + q];
                    A[r * D + p] = c * arp - s * arq;
                    A[p * D + r] = A[r * D + p];
                    A[r * D + q] = c * arq + s * arp;
                    A[q * D + r] = A[r * D + q];
                }
                for (size_t r = 0; r < D; ++r) {
                    const double vrp = V[r * D + p], vrq = V[r * D + q];
                    V[r * D + p] = c * vrp - s * vrq;
                    V[r * D + q] = c * vrq + s * vrp;
                }
            }
    }
}

std::tuple<Value, Value>
pseudospectrum(const Value &x, int p, int nfft, double fs, bool eigWeight,
               std::pmr::memory_resource *mr)
{
    if (p < 1) p = 1;
    if (nfft < 2) nfft = 256;
    const int M = 2 * p;                       // correlation-matrix order (MATLAB default)

    Value         Xc   = corrmtx(x, M - 1, mr);   // (N+M-1) × M data matrix; X'X = R
    const int     rows = static_cast<int>(Xc.dims().rows());
    const double *Xd   = Xc.doubleData();         // column-major

    std::vector<double> R(static_cast<size_t>(M) * M, 0.0);
    for (int i = 0; i < M; ++i)
        for (int j = 0; j < M; ++j) {
            double s = 0.0;
            for (int r = 0; r < rows; ++r) s += Xd[i * rows + r] * Xd[j * rows + r];
            R[i * M + j] = s;
        }

    std::vector<double> V;
    jacobiEig(R, V, static_cast<size_t>(M));
    std::vector<double> lam(M);
    for (int i = 0; i < M; ++i) lam[i] = R[i * M + i];

    // Ascending eigenvalue order; noise subspace = the smallest (M-p) vectors.
    std::vector<int> ord(M);
    std::iota(ord.begin(), ord.end(), 0);
    std::sort(ord.begin(), ord.end(), [&](int a, int b) { return lam[a] < lam[b]; });
    const int nNoise = M - p;

    const int nf = nfft / 2 + 1;
    Value     P  = Value::matrix(nf, 1, ValueType::DOUBLE, mr);
    Value     F  = Value::matrix(nf, 1, ValueType::DOUBLE, mr);
    double *  Pd = P.doubleDataMut();
    double *  Fd = F.doubleDataMut();
    for (int k = 0; k < nf; ++k) {
        const double w   = M_PI * k / (nf - 1);   // normalized angular freq [0, π]
        double       sum = 0.0;
        for (int t = 0; t < nNoise; ++t) {
            const int col = ord[t];
            double    re = 0.0, im = 0.0;
            for (int m = 0; m < M; ++m) {
                const double v = V[static_cast<size_t>(m) * M + col];
                re += v * std::cos(w * m);
                im += v * std::sin(w * m);
            }
            double proj = re * re + im * im;
            if (eigWeight) proj /= (lam[col] > 1e-300 ? lam[col] : 1e-300);
            sum += proj;
        }
        Pd[k] = (sum > 0.0) ? 1.0 / sum : std::numeric_limits<double>::infinity();
        Fd[k] = (fs / 2.0) * k / (nf - 1);
    }
    return std::make_tuple(std::move(P), std::move(F));
}

} // namespace

std::tuple<Value, Value>
pmusic(const Value &x, int p, int nfft, double fs, std::pmr::memory_resource *mr)
{
    return pseudospectrum(x, p, nfft, fs, /*eigWeight=*/false, mr);
}

std::tuple<Value, Value>
peig(const Value &x, int p, int nfft, double fs, std::pmr::memory_resource *mr)
{
    return pseudospectrum(x, p, nfft, fs, /*eigWeight=*/true, mr);
}

} // namespace numkit::signal
