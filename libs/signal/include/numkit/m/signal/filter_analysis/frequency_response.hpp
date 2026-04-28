// libs/signal/include/numkit/m/signal/filter_analysis/frequency_response.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m::signal {

/// Frequency response H(e^{j omega}) of an IIR/FIR filter: H = b(e^{-jw}) / a(e^{-jw}).
/// Returns (H, W) — complex response and the grid of frequencies w in [0, pi].
///
/// @param b     Numerator polynomial.
/// @param a     Denominator polynomial.
/// @param npts  Number of frequency points (default 512).
std::tuple<MValue, MValue>
freqz(Allocator &alloc, const MValue &b, const MValue &a, size_t npts = 512);

/// phasez(b, a[, n]) — unwrapped phase response of H(e^{jw}).
/// Returns (phi, W) — phi = unwrap(angle(H)) and the same frequency
/// grid that freqz produces. Output length is n (default 512).
std::tuple<MValue, MValue>
phasez(Allocator &alloc, const MValue &b, const MValue &a, size_t npts = 512);

/// grpdelay(b, a[, n]) — group delay = -d(phase)/d(omega).
/// Computed as the discrete derivative of unwrap(angle(freqz)) on the
/// uniform grid w in [0, π]. Returns (gd, W). Endpoints use the same
/// step (forward at 0, backward at π).
std::tuple<MValue, MValue>
grpdelay(Allocator &alloc, const MValue &b, const MValue &a, size_t npts = 512);

} // namespace numkit::m::signal
