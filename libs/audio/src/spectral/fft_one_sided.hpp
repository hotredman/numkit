// libs/audio/src/spectral/fft_one_sided.hpp
//
// Shared per-frame FFT helpers for libs/audio spectral pipelines
// (shape_descriptors, melspec_delta, cepstral). Wraps libs/signal::fft
// to produce one-sided power / magnitude spectra of length N/2+1 — the
// shape every audio toolbox filter-bank pipeline expects.
//
// Why this exists: Audio Toolbox functions all use STFT with
// winLen = round(0.03*fs), giving N ∈ {240, 480, 662, 1323} for
// fs ∈ {8k, 16k, 22.05k, 44.1k}. None are pow2, so until libs/signal
// shipped Bluestein (chirp-z) for non-pow2 N, these pipelines used a
// hand-rolled O(N²) DFT. This header is the Cycle-J swap to O(N log N).
//
// Lifetime: a per-frame ScratchArena bumps allocations off the caller's
// mr — both the input frame Value and the output Y Value live in the
// arena and free in one shot when the helper returns.

#pragma once

#include <numkit/core/scratch.hpp>
#include <numkit/core/value.hpp>
#include <numkit/signal/transforms/fft.hpp>

#include <complex>
#include <cstddef>

namespace numkit::audio::detail {

inline void fftPowerHalf(std::pmr::memory_resource *mr,
                         const double *x, std::size_t N,
                         double *out_pow_half)
{
    ScratchArena arena(mr);
    Value frame = Value::matrix(N, 1, ValueType::DOUBLE, &arena);
    double *fd = frame.doubleDataMut();
    for (std::size_t i = 0; i < N; ++i) fd[i] = x[i];
    Value Y = signal::fft(frame, -1, 0, &arena);
    const std::complex<double> *Yd = Y.complexData();
    const std::size_t H = N / 2 + 1;
    for (std::size_t k = 0; k < H; ++k) out_pow_half[k] = std::norm(Yd[k]);
}

inline void fftMagHalf(std::pmr::memory_resource *mr,
                       const double *x, std::size_t N,
                       double *out_mag_half)
{
    ScratchArena arena(mr);
    Value frame = Value::matrix(N, 1, ValueType::DOUBLE, &arena);
    double *fd = frame.doubleDataMut();
    for (std::size_t i = 0; i < N; ++i) fd[i] = x[i];
    Value Y = signal::fft(frame, -1, 0, &arena);
    const std::complex<double> *Yd = Y.complexData();
    const std::size_t H = N / 2 + 1;
    for (std::size_t k = 0; k < H; ++k) out_mag_half[k] = std::abs(Yd[k]);
}

} // namespace numkit::audio::detail
