// libs/dsp/src/backends/FftKernels.hpp
//
// Private kernel-dispatch header for FFT backends. Compiled by
// exactly one of backends/MDspFft_{portable,simd}.cpp based on
// NUMKIT_WITH_SIMD. The only caller of fftRadix2Impl is fftAlongDim
// in MDspFft.cpp — other FFT users (MDspTransform, MDspSpectral,
// convFFT) still call the inline scalar fftRadix2 in MDspHelpers.hpp
// directly; migrate them individually if their perf matters.

#pragma once

#include <numkit/m/core/MTypes.hpp>   // Complex

#include <cstddef>

namespace numkit::m::signal::detail {

// In-place radix-2 FFT over an interleaved-complex buffer.
// N must be a power of 2. W is a precomputed twiddle table of
// length N/2 (see fillFftTwiddles in MDspHelpers.hpp).
//
// The portable backend forwards to the inline scalar reference.
// The SIMD backend converts to SoA (split real/imag) internally,
// runs vectorised butterflies via Highway, and converts back.
void fftRadix2Impl(Complex *buf, std::size_t N, const Complex *W);

// Stockham auto-sort radix-2 dispatcher (SIMD backend only). Same
// contract as fftRadix2Impl but uses an internal thread-local
// scratch buffer instead of working in place — eliminates the
// bit-reversal pre-pass at the cost of 2× memory for the working
// state. Exposed for benchmarking from fft_bench; the public
// fftRadix2Impl chooses whether to route here based on N.
void fftStockhamDispatch(Complex *buf, std::size_t N, const Complex *W);

// SoA radix-2 dispatcher: same in-place semantics as fftRadix2Impl,
// but the kernel converts buf → split real/imag arrays internally to
// avoid the AVX2 LoadInterleaved2/StoreInterleaved2 permute cost,
// then converts back. Per-thread re/im scratch grows monotonically.
void fftRadix2SoaDispatch(Complex *buf, std::size_t N, const Complex *W);

// SoA radix-4 dispatcher: same algorithm as the AoS r4 path, but
// using split real/imag layout to skip the AVX2 LoadInterleaved2 ×4
// + StoreInterleaved2 ×4 permute cost per inner iteration. N must
// be a power of 4. Per-thread re/im scratch grows monotonically.
void fftRadix4Pow4SoaDispatch(Complex *buf, std::size_t N, const Complex *W);

// SoA-native FFT stages: caller provides split-real/imag scratch
// already populated with the input. Runs bit-reverse + butterfly
// stages in place on re/im, picks r2 or r4 internally based on N.
// Used by the wrapper's rfft path which packs straight into SoA and
// twists straight from SoA, avoiding the per-call AoS↔SoA conversion
// the public AoS-in/out dispatchers wrap around the same kernels.
//
// On WASM (Emscripten target) — the dispatcher falls back to AoS in
// the wrapper since SoA doesn't help SIMD128. This entry point still
// works (it just routes to the AoS path with internal conversion);
// the wrapper checks the platform and decides.
void fftSoaStagesDispatch(double *re, double *im, std::size_t N, const Complex *W);

} // namespace numkit::m::signal::detail
