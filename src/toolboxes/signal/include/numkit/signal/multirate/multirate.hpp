// toolboxes/signal/include/numkit/signal/multirate/multirate.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::signal {

/// Integer-rate downsampling — keep every n-th sample.
///
/// `y[i] = x[i · n]`. Does NOT apply an anti-aliasing filter — use
/// `decimate` when you need that. Output length is `ceil(nx / n)`.
/// Orientation (row vs. column) is preserved.
///
/// @param x   Input signal.
/// @param n   Decimation factor, ≥ 1.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Downsampled signal.
///
/// @see decimate, upsample, resample
///
/// `phase` (0 ≤ phase < n) offsets the first kept sample: the output is
/// `x[phase], x[phase+n], …`.
Value downsample(const Value &                x,
                 size_t                       n,
                 std::pmr::memory_resource *  mr = nullptr,
                 size_t                       phase = 0);

/// Integer-rate upsampling — zero-stuff between samples.
///
/// `y[i · n] = x[i]`, other positions zero. Output length is `nx · n`.
/// Orientation preserved. Pair with a lowpass filter for proper
/// band-limited interpolation, or use `resample`.
///
/// @param x   Input signal.
/// @param n   Up-sampling factor, ≥ 1.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Upsampled signal.
///
/// @see downsample, resample, interp
///
/// `phase` (0 ≤ phase < n) places the samples at offset `phase`:
/// `y[phase + i·n] = x[i]`.
Value upsample(const Value &                x,
               size_t                       n,
               std::pmr::memory_resource *  mr = nullptr,
               size_t                       phase = 0);

/// Integer-factor decimation with anti-alias filtering.
///
/// Pipeline: design an FIR lowpass with cutoff `1/factor` of Nyquist and
/// order `8 · factor` (clamped to `nx - 1`), filter, then downsample by
/// `factor`. Suppresses aliasing that would otherwise wrap into the
/// preserved Nyquist band.
///
/// @param x       Input signal.
/// @param factor  Decimation factor, ≥ 1.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Decimated signal, length `ceil(nx / factor)`.
///
/// @see downsample, resample
Value decimate(const Value &                x,
               size_t                       factor,
               std::pmr::memory_resource *  mr = nullptr);

/// Rational rate conversion: change rate by factor `p/q`.
///
/// Pipeline: upsample by `p` with zero-stuffing → anti-alias FIR
/// lowpass at cutoff `min(1/p, 1/q)` of Nyquist (order `10 · max(p, q)`)
/// → downsample by `q`.
///
/// @param x   Input signal.
/// @param p   Numerator (target rate).
/// @param q   Denominator (source rate).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Resampled signal of length `ceil(nx · p / q)`.
///
/// @code
/// Value y = resample(x, 3, 2);     // 1.5× speedup
/// @endcode
///
/// @see decimate, upsample, downsample
Value resample(const Value &                x,
               size_t                       p,
               size_t                       q,
               std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
