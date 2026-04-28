// libs/dsp/include/numkit/m/signal/MDspGaps.hpp
//
// Phase-9 DSP gaps: medfilt1, findpeaks, goertzel, dct, idct.
// All operate on flat 1D vectors. SOS / medfilt2 / wavelet are
// explicitly out of scope per the parity-expansion plan.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m::signal {

/// medfilt1(x, k) — sliding-window median over a window of k samples
/// (default 3). Window is symmetric around the current sample with
/// boundary truncation (no zero padding) — matches MATLAB's
/// 'truncate' endpoint mode, which is the default.
MValue medfilt1(Allocator &alloc, const MValue &x, size_t k = 3);

/// findpeaks(x) — strict local maxima (x[i-1] < x[i] > x[i+1]).
/// Returns (peakValues, peakIndices_1based) as row vectors.
std::tuple<MValue, MValue>
findpeaks(Allocator &alloc, const MValue &x);

/// goertzel(x, ind) — single-frequency DFT for each 1-based index in
/// `ind`. Output is complex, same shape as `ind`.
MValue goertzel(Allocator &alloc, const MValue &x, const MValue &ind);

/// dct(x) — Type-II discrete cosine transform (MATLAB default).
MValue dct(Allocator &alloc, const MValue &x);

/// idct(x) — inverse Type-II DCT.
MValue idct(Allocator &alloc, const MValue &x);

} // namespace numkit::m::signal
