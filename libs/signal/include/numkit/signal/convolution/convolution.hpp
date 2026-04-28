// libs/dsp/include/numkit/signal/convolution/convolution.hpp
#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit::signal {

/// 1D convolution of two real vectors.
///
/// @param shape  "full"  (default, length na + nb - 1),
///               "same"  (length max(na, nb), centered on full result),
///               "valid" (length |na - nb| + 1, no zero-padding effects).
/// @throws Error on bad shape keyword.
Value conv(Allocator &alloc,
            const Value &a,
            const Value &b,
            const std::string &shape = "full");

/// Polynomial long-division: b = conv(a, q) + r.
///
/// Returns (q, r) as a tuple. Throws Error if `a` is longer than `b` or
/// if a[0] == 0 (leading coefficient). MATLAB's `[q, r] = deconv(b, a)`.
std::tuple<Value, Value>
deconv(Allocator &alloc, const Value &b, const Value &a);

/// Cross-correlation of x and y. Returns (c, lags).
///
/// c has length nx + ny - 1. lags is an integer vector spanning
/// -(max(nx,ny)-1) ... +(max(nx,ny)-1). MATLAB's `[c, lags] = xcorr(x, y)`.
std::tuple<Value, Value>
xcorr(Allocator &alloc, const Value &x, const Value &y);

/// Auto-correlation — equivalent to xcorr(alloc, x, x).
inline std::tuple<Value, Value>
xcorr(Allocator &alloc, const Value &x)
{
    return xcorr(alloc, x, x);
}

} // namespace numkit::signal
