// libs/control/src/internal/numerics.cpp
//
// Shared numerical kernels — see internal/numerics.hpp.

#include <numkit/control/internal/numerics.hpp>

#include <algorithm>
#include <cmath>

namespace numkit::control::internal {

bool solveInPlace(Mat &A, Mat &B, std::size_t n, std::size_t nrhs)
{
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t pk = k;
        double bestAbs = std::abs(A[k * n + k]);
        for (std::size_t i = k + 1; i < n; ++i) {
            const double v = std::abs(A[k * n + i]);
            if (v > bestAbs) { bestAbs = v; pk = i; }
        }
        if (bestAbs < 1e-14) return false;
        if (pk != k) {
            for (std::size_t j = 0; j < n; ++j)
                std::swap(A[j * n + k], A[j * n + pk]);
            for (std::size_t j = 0; j < nrhs; ++j)
                std::swap(B[j * n + k], B[j * n + pk]);
        }
        const double diag = A[k * n + k];
        for (std::size_t i = k + 1; i < n; ++i) {
            const double f = A[k * n + i] / diag;
            A[k * n + i] = f;
            for (std::size_t j = k + 1; j < n; ++j)
                A[j * n + i] -= f * A[j * n + k];
            for (std::size_t j = 0; j < nrhs; ++j)
                B[j * n + i] -= f * B[j * n + k];
        }
    }
    for (std::size_t j = 0; j < nrhs; ++j) {
        for (std::size_t i = n; i-- > 0;) {
            double s = B[j * n + i];
            for (std::size_t k = i + 1; k < n; ++k)
                s -= A[k * n + i] * B[j * n + k];
            B[j * n + i] = s / A[i * n + i];
        }
    }
    return true;
}

Mat matmulSq(const Mat &A, const Mat &B, std::size_t n)
{
    Mat C(n * n, 0.0);
    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t i = 0; i < n; ++i) {
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k)
                s += A[k * n + i] * B[j * n + k];
            C[j * n + i] = s;
        }
    return C;
}

namespace {

double matInfNorm(const Mat &A, std::size_t n) {
    double m = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        double s = 0.0;
        for (std::size_t j = 0; j < n; ++j) s += std::abs(A[j * n + i]);
        m = std::max(m, s);
    }
    return m;
}

Mat eyeM(std::size_t n) {
    Mat I(n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i) I[i * n + i] = 1.0;
    return I;
}

Mat zerosM(std::size_t r, std::size_t c) {
    return Mat(r * c, 0.0);
}

} // anonymous

Mat expm(const Mat &Ain, std::size_t n)
{
    if (n == 0) return Mat{};
    Mat A = Ain;
    const double normA = matInfNorm(A, n);
    int s = 0;
    if (normA > 0.5) {
        const double l2 = std::log2(normA / 0.5);
        s = static_cast<int>(std::ceil(std::max(l2, 0.0)));
    }
    if (s > 0) {
        const double scale = std::pow(0.5, s);
        for (auto &v : A) v *= scale;
    }
    // Canonical [6/6] Padé coefficients for exp(x):
    //   c_k = (12 − k)! · 6! / (12! · k! · (6 − k)!)
    static const double c[7] = {
        1.0,
        1.0 /  2.0,
        5.0 / 44.0,
        1.0 / 66.0,
        1.0 / 792.0,
        1.0 / 15840.0,
        1.0 / 665280.0
    };
    Mat I  = eyeM(n);
    Mat A2 = matmulSq(A,  A,  n);
    Mat A3 = matmulSq(A2, A,  n);
    Mat A4 = matmulSq(A2, A2, n);
    Mat A5 = matmulSq(A4, A,  n);
    Mat A6 = matmulSq(A4, A2, n);

    auto axpy = [&](Mat &dst, const Mat &src, double a) {
        for (std::size_t i = 0; i < n * n; ++i) dst[i] += a * src[i];
    };
    Mat N = zerosM(n, n), D = zerosM(n, n);
    axpy(N, I,   c[0]); axpy(D, I,   c[0]);
    axpy(N, A,   c[1]); axpy(D, A,  -c[1]);
    axpy(N, A2,  c[2]); axpy(D, A2,  c[2]);
    axpy(N, A3,  c[3]); axpy(D, A3, -c[3]);
    axpy(N, A4,  c[4]); axpy(D, A4,  c[4]);
    axpy(N, A5,  c[5]); axpy(D, A5, -c[5]);
    axpy(N, A6,  c[6]); axpy(D, A6,  c[6]);

    Mat Dcopy = D;
    Mat X     = N;
    if (!solveInPlace(Dcopy, X, n, n)) {
        // Truncated power series fallback.
        Mat E    = I;
        Mat term = I;
        for (int k = 1; k < 30; ++k) {
            term = matmulSq(term, A, n);
            const double inv = 1.0 / static_cast<double>(k);
            for (auto &v : term) v *= inv;
            for (std::size_t i = 0; i < n * n; ++i) E[i] += term[i];
        }
        X = E;
    }
    for (int k = 0; k < s; ++k) X = matmulSq(X, X, n);
    return X;
}

Vec charPoly(const Mat &A, std::size_t n)
{
    Mat M(n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i) M[i * n + i] = 1.0;   // I
    Vec coeff(n + 1, 0.0);
    coeff[0] = 1.0;

    for (std::size_t k = 1; k <= n; ++k) {
        Mat AM = matmulSq(A, M, n);
        double tr = 0.0;
        for (std::size_t i = 0; i < n; ++i) tr += AM[i * n + i];
        const double ck = -tr / static_cast<double>(k);
        coeff[k] = ck;
        for (std::size_t i = 0; i < n; ++i) AM[i * n + i] += ck;
        M = std::move(AM);
    }
    return coeff;
}

} // namespace numkit::control::internal
