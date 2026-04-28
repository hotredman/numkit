// libs/dsp/include/numkit/m/signal/convolution/convolution.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <string>
#include <tuple>

namespace numkit::m::signal {

/// 1D convolution of two real vectors.
///
/// @param shape  "full"  (default, length na + nb - 1),
///               "same"  (length max(na, nb), centered on full result),
///               "valid" (length |na - nb| + 1, no zero-padding effects).
/// @throws MError on bad shape keyword.
MValue conv(Allocator &alloc,
            const MValue &a,
            const MValue &b,
            const std::string &shape = "full");

/// Polynomial long-division: b = conv(a, q) + r.
///
/// Returns (q, r) as a tuple. Throws MError if `a` is longer than `b` or
/// if a[0] == 0 (leading coefficient). MATLAB's `[q, r] = deconv(b, a)`.
std::tuple<MValue, MValue>
deconv(Allocator &alloc, const MValue &b, const MValue &a);

/// Cross-correlation of x and y. Returns (c, lags).
///
/// c has length nx + ny - 1. lags is an integer vector spanning
/// -(max(nx,ny)-1) ... +(max(nx,ny)-1). MATLAB's `[c, lags] = xcorr(x, y)`.
std::tuple<MValue, MValue>
xcorr(Allocator &alloc, const MValue &x, const MValue &y);

/// Auto-correlation — equivalent to xcorr(alloc, x, x).
inline std::tuple<MValue, MValue>
xcorr(Allocator &alloc, const MValue &x)
{
    return xcorr(alloc, x, x);
}

} // namespace numkit::m::signal
