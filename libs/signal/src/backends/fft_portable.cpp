// libs/dsp/src/backends/fft_portable.cpp
//
// Portable FFT kernel — a thin forwarder to the inline scalar
// fftRadix2 in helpers.hpp. Compiled when NUMKIT_WITH_SIMD=OFF.

#include "FftKernels.hpp"
#include "../dsp_helpers.hpp"

namespace numkit::signal::detail {

void fftRadix2Impl(Complex *buf, std::size_t N, const Complex *W)
{
    // MDspHelpers' fftRadix2 overload already takes (buf, N, W).
    numkit::fftRadix2(buf, N, W);
}

} // namespace numkit::signal::detail
