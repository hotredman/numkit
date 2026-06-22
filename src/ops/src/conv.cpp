// ops/src/conv.cpp
//
// Direct real-double linear convolution, moved verbatim from signal's
// dsp_helpers.hpp (convDirect) so the FIR kernel lives in the kernel layer.
// Portable C++; the inner accumulate is the natural SIMD target (a follow-on
// Highway backend can replace it without touching this signature or the
// toolbox call sites). See conv.hpp.

#include <numkit/ops/conv.hpp>

#include <algorithm>

namespace numkit::ops {

ScratchVec<double> convDirect(const double *a, std::size_t na,
                              const double *b, std::size_t nb,
                              std::pmr::memory_resource *mr)
{
    std::size_t        nc = na + nb - 1;
    ScratchVec<double> c(nc, mr);
    for (std::size_t i = 0; i < na; ++i)
        for (std::size_t j = 0; j < nb; ++j)
            c[i + j] += a[i] * b[j];
    return c;
}

void conv2Direct(const double *A, std::size_t M, std::size_t N,
                 const double *B, std::size_t P, std::size_t Q,
                 double *out)
{
    const std::size_t outR = M + P - 1;
    const std::size_t outC = N + Q - 1;
    std::fill_n(out, outR * outC, 0.0);
    for (std::size_t j = 0; j < Q; ++j) {
        for (std::size_t i = 0; i < P; ++i) {
            const double bij = B[j * P + i];
            if (bij == 0.0) continue;
            for (std::size_t cc = 0; cc < N; ++cc) {
                const std::size_t outCol = j + cc;
                for (std::size_t rr = 0; rr < M; ++rr) {
                    out[outCol * outR + (i + rr)] += A[cc * M + rr] * bij;
                }
            }
        }
    }
}

} // namespace numkit::ops
