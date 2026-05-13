// libs/signal/include/numkit/signal/smoothing/medfilt.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// 1-D median filter.
///
/// Sliding-window median over a window of `k` samples centered on each
/// output sample. Effective for removing salt-and-pepper noise and
/// short outliers while preserving step-like edges (unlike linear
/// smoothing, which blurs edges).
///
/// Boundary handling uses truncation (MATLAB's default `'truncate'`
/// mode): the window is shortened at the signal edges rather than
/// zero-padded.
///
/// @param x   Real 1-D signal.
/// @param k   Window length (samples). Default 3. Typically odd; even k
///            uses the lower-median convention.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Filtered signal, same shape as `x`.
///
/// @code
/// // Remove single-sample outliers in a noisy measurement:
/// Value clean = medfilt1(noisy, 5);
/// @endcode
///
/// @see sgolayfilt
Value medfilt1(const Value &                x,
               size_t                       k  = 3,
               std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
