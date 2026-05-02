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

/// 2-D convolution. `shape` ∈ {"full" (default), "same", "valid"}.
/// Direct nested-loop implementation; for large inputs FFT-based 2-D
/// conv would be faster but is not yet wired in.
Value conv2(std::pmr::memory_resource *mr,
            const Value &A, const Value &B,
            const std::string &shape = "full");

/// 2-D filter — equivalent to MATLAB's `filter2(h, X[, shape])`. Same
/// as `conv2(X, rot90(h, 2), shape)`: h is rotated 180° before
/// convolution (MATLAB filter2 docs).
Value filter2(std::pmr::memory_resource *mr,
              const Value &h, const Value &X,
              const std::string &shape = "same");

/// N-D convolution. Currently supports 1-D, 2-D and 3-D. shape
/// keywords match conv / conv2 ('full', 'same', 'valid').
Value convn(std::pmr::memory_resource *mr,
            const Value &A, const Value &B,
            const std::string &shape = "full");

} // namespace numkit::signal
