// toolboxes/builtin/src/lang/operators/backends/binary_ops_portable.cpp
//
// Scalar reference inner loops for plus / minus / times / rdivide on
// real double arrays. Compiled when NUMKIT_WITH_SIMD=OFF. The
// Highway-dispatched variant lives in binary_ops_highway.cpp.

#include <numkit/ops/binary_ops.hpp>

#include <cstddef>

namespace numkit::ops::detail {

// plus/minus/times/rdivide forward to the shared scalar bodies in
// binary_scalar.cpp so this fallback and the Highway small-N gate are one
// definition (can't drift) — see bugs/ops/cheap-elementwise-simd-small-n.
void plusLoop(const double *a, const double *b, double *out, std::size_t n)
{
    plusScalar(a, b, out, n);
}

void minusLoop(const double *a, const double *b, double *out, std::size_t n)
{
    minusScalar(a, b, out, n);
}

void timesLoop(const double *a, const double *b, double *out, std::size_t n)
{
    timesScalar(a, b, out, n);
}

void rdivideLoop(const double *a, const double *b, double *out, std::size_t n)
{
    rdivideScalar(a, b, out, n);
}

void matmulDoubleLoop(const double *a, const double *b, double *c,
                      std::size_t M, std::size_t N, std::size_t K)
{
    // SAXPY over columns of A — the scalar reference implementation
    // shares the loop order (j, k, i) with the SIMD backend so any
    // difference in output comes purely from FMA vs mul+add, not from
    // reduction-order changes.
    for (std::size_t j = 0; j < N; ++j) {
        double *c_col = c + j * M;
        for (std::size_t i = 0; i < M; ++i) c_col[i] = 0.0;
        for (std::size_t k = 0; k < K; ++k) {
            const double bkj = b[j * K + k];
            const double *a_col = a + k * M;
            for (std::size_t i = 0; i < M; ++i)
                c_col[i] += bkj * a_col[i];
        }
    }
}

} // namespace numkit::ops::detail
