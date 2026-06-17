// toolboxes/signal/src/transforms/backends/fft_kernels.hpp
//
// Transitional shim. The FFT transform kernels (radix-2/4, Stockham, SoA, with
// portable + Highway-SIMD backends) moved to the L0.5 ops layer
// (<numkit/ops/fft/fft_kernels.hpp>, numkit::ops::detail) — raw Complex*/double*
// buffer transforms, the proper home for a reusable SIMD primitive. This
// re-exports them into numkit::signal::detail so the existing callers
// (transforms/fft.cpp, the bench, the public-API test) are unchanged.
//
// New code includes <numkit/ops/fft/fft_kernels.hpp> and calls
// numkit::ops::detail::fft* directly. (Moving the kernel here also lets the
// audio/comm/image FFT-using paths reach a shared ops kernel without depending
// on the signal toolbox.)

#pragma once

#include <numkit/ops/fft/fft_kernels.hpp>

namespace numkit::signal::detail {

using numkit::ops::detail::fftRadix2Impl;
using numkit::ops::detail::fftStockhamDispatch;
using numkit::ops::detail::fftRadix2SoaDispatch;
using numkit::ops::detail::fftRadix4Pow4SoaDispatch;
using numkit::ops::detail::fftSoaStagesDispatch;

} // namespace numkit::signal::detail
