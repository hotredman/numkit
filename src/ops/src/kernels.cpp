// numkit/ops/kernels.cpp
//
// The public kernel facade (numkit/ops/kernels.hpp) — thin forwarders to the
// backend-split detail loops. The facade itself is allowed to reach into
// detail; its callers are not. Keeping the forwarders in one base TU (always
// compiled, no SIMD specialisation of its own) means the public symbol resolves
// to whichever backend (Highway / portable) was linked, with no drift.
#include <numkit/ops/kernels.hpp>

#include <numkit/ops/binary_ops.hpp>  // detail::matmulDoubleLoop

namespace numkit::ops {

void matmulDouble(const double *a, const double *b, double *c,
                  std::size_t M, std::size_t N, std::size_t K)
{
    detail::matmulDoubleLoop(a, b, c, M, N, K);
}

void matmulComplex(const std::complex<double> *a, const std::complex<double> *b,
                   std::complex<double> *c, std::size_t M, std::size_t N, std::size_t K)
{
    // Column-major C(M×N) = A(M×K)·B(K×N): c[i+j*M] = sum_k a[i+k*M]*b[k+j*K].
    // Zero, then SAXPY down columns (the (j,k,i) order matmulDoubleLoop uses) —
    // no SIMD-complex backend, but the same cache-friendly traversal.
    for (std::size_t i = 0; i < M * N; ++i) c[i] = std::complex<double>(0.0, 0.0);
    for (std::size_t j = 0; j < N; ++j)
        for (std::size_t k = 0; k < K; ++k) {
            const std::complex<double> bkj = b[k + j * K];
            for (std::size_t i = 0; i < M; ++i) c[i + j * M] += a[i + k * M] * bkj;
        }
}

void plusDouble(const double *a, const double *b, double *out, std::size_t n)
{
    detail::plusLoop(a, b, out, n);
}
void minusDouble(const double *a, const double *b, double *out, std::size_t n)
{
    detail::minusLoop(a, b, out, n);
}
void timesDouble(const double *a, const double *b, double *out, std::size_t n)
{
    detail::timesLoop(a, b, out, n);
}
void rdivideDouble(const double *a, const double *b, double *out, std::size_t n)
{
    detail::rdivideLoop(a, b, out, n);
}

} // namespace numkit::ops
