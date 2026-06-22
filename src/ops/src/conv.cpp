// ops/src/conv.cpp
//
// Direct real-double linear convolution, moved verbatim from signal's
// dsp_helpers.hpp (convDirect) so the FIR kernel lives in the kernel layer.
// Portable C++; the inner accumulate is the natural SIMD target (a follow-on
// Highway backend can replace it without touching this signature or the
// toolbox call sites). See conv.hpp.

#include <numkit/ops/conv.hpp>

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

} // namespace numkit::ops
