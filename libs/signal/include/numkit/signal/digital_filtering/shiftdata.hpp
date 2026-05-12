// libs/signal/include/numkit/signal/digital_filtering/shiftdata.hpp
//
// MATLAB Signal Toolbox shiftdata + unshiftdata (Phase 4.4):
// utilities for writing dim-aware functions. shiftdata moves the
// chosen dim to dim 1 (leading); unshiftdata reverses.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <tuple>

namespace numkit::signal {

/// Move a chosen dimension to the leading axis for dim-aware processing.
///
/// Two operating modes:
///   * `dim == 0` (or unspecified): "auto" mode. Calls `shiftdim(x)` to
///     drop leading singleton dimensions. `perm` is returned empty;
///     `nshifts` contains the number of dimensions rolled up.
///   * `dim > 0`: explicit dim mode. Calls `permute(x, perm)` where
///     `perm = [dim, 1..dim-1, dim+1..ndims]`. `nshifts` is empty.
///
/// Useful when implementing a function that operates "along the first
/// non-singleton dimension by default, or along an explicit dim if
/// provided" — the standard MATLAB convention.
///
/// @param x    Input array.
/// @param dim  Dimension to move to the front. `0` (default) = auto.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Tuple `(shifted, perm, nshifts)`. Use `unshiftdata` to
///             reverse.
///
/// @see unshiftdata
std::tuple<Value, Value, Value>
shiftdata(const Value &                x,
          int                          dim = 0,
          std::pmr::memory_resource *  mr  = nullptr);

/// Inverse of shiftdata.
///
/// @param x         Reshaped array from shiftdata.
/// @param perm      Permutation vector from shiftdata (empty → use nshifts).
/// @param nshifts   Number of dimensions shiftdata had to roll up.
///
/// If `perm` is empty: `y = shiftdim(x, -nshifts)`.
/// Otherwise:          `y = ipermute(x, perm)`.
Value unshiftdata(const Value &x, const Value &perm, const Value &nshifts,
                  std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
