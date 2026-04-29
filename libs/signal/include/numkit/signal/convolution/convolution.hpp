// libs/signal/include/numkit/signal/convolution/convolution.hpp
#pragma once

#include <memory_resource>
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
Value conv(std::pmr::memory_resource *mr,
            const Value &a,
            const Value &b,
            const std::string &shape = "full");

/// Polynomial long-division: b = conv(a, q) + r.
///
/// Returns (q, r) as a tuple. Throws Error if `a` is longer than `b` or
/// if a[0] == 0 (leading coefficient). MATLAB's `[q, r] = deconv(b, a)`.
std::tuple<Value, Value>
deconv(std::pmr::memory_resource *mr, const Value &b, const Value &a);

/// Cross-correlation of x and y. Returns (c, lags).
///
/// c has length nx + ny - 1. lags is an integer vector spanning
/// -(max(nx,ny)-1) ... +(max(nx,ny)-1). MATLAB's `[c, lags] = xcorr(x, y)`.
std::tuple<Value, Value>
xcorr(std::pmr::memory_resource *mr, const Value &x, const Value &y);

/// Auto-correlation — equivalent to xcorr(mr, x, x).
inline std::tuple<Value, Value>
xcorr(std::pmr::memory_resource *mr, const Value &x)
{
    return xcorr(mr, x, x);
}

} // namespace numkit::signal
