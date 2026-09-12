// ops/src/fft/fft_portable.cpp
//
// Portable FFT kernel — a thin forwarder to the inline scalar
// fftRadix2 in helpers.hpp. Compiled when NK_WITH_SIMD=OFF.

#include <nk/ops/fft/fft_kernels.hpp>
#include <nk/ops/fft/fft_radix2.hpp>

namespace nk::ops::detail {

void fftRadix2Impl(Complex *buf, std::size_t N, const Complex *W)
{
    // dsp_helpers.hpp' fftRadix2 overload already takes (buf, N, W).
    nk::fftRadix2(buf, N, W);
}

} // namespace nk::ops::detail
