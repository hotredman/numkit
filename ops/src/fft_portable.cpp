// toolboxes/signal/src/transforms/backends/fft_portable.cpp
//
// Portable FFT kernel — a thin forwarder to the inline scalar
// fftRadix2 in helpers.hpp. Compiled when NUMKIT_WITH_SIMD=OFF.

#include <numkit/ops/fft_kernels.hpp>
#include <numkit/ops/fft_radix2.hpp>

namespace numkit::ops::detail {

void fftRadix2Impl(Complex *buf, std::size_t N, const Complex *W)
{
    // dsp_helpers.hpp' fftRadix2 overload already takes (buf, N, W).
    numkit::fftRadix2(buf, N, W);
}

} // namespace numkit::ops::detail
