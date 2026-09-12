// toolboxes/signal/src/transforms/backends/fft_kernels.hpp
//
// Transitional shim. The FFT transform kernels (radix-2/4, Stockham, SoA, with
// portable + Highway-SIMD backends) moved to the L0.5 ops layer
// (<nk/ops/fft/fft_kernels.hpp>, nk::ops::detail) — raw Complex*/double*
// buffer transforms, the proper home for a reusable SIMD primitive. This
// re-exports them into nk::signal::detail so the existing callers
// (transforms/fft.cpp, the bench, the public-API test) are unchanged.
//
// New code includes <nk/ops/fft/fft_kernels.hpp> and calls
// nk::ops::detail::fft* directly. (Moving the kernel here also lets the
// audio/comm/image FFT-using paths reach a shared ops kernel without depending
// on the signal toolbox.)

#pragma once

#include <nk/ops/fft/fft_kernels.hpp>

namespace nk::signal::detail {

using nk::ops::detail::fftRadix2Impl;
using nk::ops::detail::fftStockhamDispatch;
using nk::ops::detail::fftRadix2SoaDispatch;
using nk::ops::detail::fftRadix4Pow4SoaDispatch;
using nk::ops::detail::fftSoaStagesDispatch;

} // namespace nk::signal::detail
