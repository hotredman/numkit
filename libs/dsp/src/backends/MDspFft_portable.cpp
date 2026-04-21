// libs/dsp/src/backends/MDspFft_portable.cpp
//
// Portable FFT kernel — a thin forwarder to the inline scalar
// fftRadix2 in MDspHelpers.hpp. Compiled when NUMKIT_WITH_SIMD=OFF.

#include "FftKernels.hpp"
#include "../MDspHelpers.hpp"

namespace numkit::m::dsp::detail {

void fftRadix2Impl(Complex *buf, std::size_t N, const Complex *W)
{
    // MDspHelpers' fftRadix2 overload already takes (buf, N, W).
    numkit::m::fftRadix2(buf, N, W);
}

} // namespace numkit::m::dsp::detail
