// libs/signal/include/numkit/signal/digital_filtering/buffer.hpp
//
// `buffer` — partition a signal into
// (possibly overlapping or underlapping) frames.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>
#include <tuple>

namespace numkit::signal {

/// @brief Partition `x` into frames of length `n`.
///
/// Returns the frame matrix `Y` only. Use @ref buffer2 for the
/// `(Y, Z)` form where `Z` holds the partial trailing samples.
///
/// @param x    Input signal (row or column vector).
/// @param n    Frame length.
/// @param p    Overlap behaviour:
///               * `p == 0`: non-overlapping, last frame zero-padded;
///               * `p > 0`:  overlap with `p` initial-zero samples;
///               * `p < 0`:  skip `|p|` samples between frames.
/// @param opt  String (`"nodelay"`) or numeric initial-condition
///             vector.
///             `Value::Empty` = default.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Frame matrix `Y`.
/// @see buffer2
Value buffer(const Value &x, int n, int p = 0,
             const Value &opt = Value::Empty,
             std::pmr::memory_resource *mr = nullptr);

/// @brief 2-output form: `[Y, Z] = buffer(x, n, p, opt)`.
///
/// `Y` has only complete frames (no trailing zero-pad). `Z` holds
/// the partial-frame remainder, shaped to match `x`'s orientation.
///
/// @param x    Input signal (row or column vector).
/// @param n    Frame length.
/// @param p    Overlap behaviour (see @ref buffer).
/// @param opt  String (`"nodelay"`) or numeric initial-condition
///             vector; `Value::Empty` = default.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Tuple `(Y, Z)`.
/// @see buffer
std::tuple<Value, Value>
buffer2(const Value &x, int n, int p = 0,
        const Value &opt = Value::Empty,
        std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
