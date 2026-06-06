// libs/wavelet/include/numkit/wavelet/dwt/dyad.hpp
//
// Dyadic upsample / downsample helpers (dyaddown / dyadup) and wmaxlev.

#pragma once

#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>

namespace numkit::wavelet {

/// @brief Dyadic downsampling — `y = dyaddown(x, odd, type)`.
///
/// Keeps every other sample. `odd == 0` (default) keeps the even-indexed
/// samples (1-based, `x(2:2:end)`); `odd == 1` keeps the odd-indexed
/// samples (`x(1:2:end)`). For a matrix, `type` selects the axis:
/// `'c'` (default) columns, `'r'` rows, `'m'` both. Vectors ignore `type`
/// and preserve orientation.
///
/// @param x     Input vector or matrix.
/// @param odd   `0` (default, keep even) or `1` (keep odd).
/// @param type  Matrix axis `'c'` (default) / `'r'` / `'m'`.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Downsampled array.
/// @see dyadup
Value dyaddown(const Value &x, int odd = 0, char type = 'c',
               std::pmr::memory_resource *mr = nullptr);

/// @brief Dyadic upsampling with zero insertion — `y = dyadup(x, odd, type)`.
///
/// Inserts zeros between samples. `odd == 0`: `[x1 0 x2 0 … xN]` (length
/// `2N-1`); `odd == 1` (default): `[0 x1 0 x2 … xN 0]` (length `2N+1`).
/// Matrix `type` axis as in @ref dyaddown. Verified vs MATLAB R2025b:
/// `dyadup([1 2 3], 0) = [1 0 2 0 3]`, `dyadup([1 2 3], 1) = [0 1 0 2 0 3 0]`.
///
/// @param x     Input vector or matrix.
/// @param odd   `0` or `1` (default `1`).
/// @param type  Matrix axis `'c'` (default) / `'r'` / `'m'`.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Upsampled array.
/// @see dyaddown
Value dyadup(const Value &x, int odd = 1, char type = 'c',
             std::pmr::memory_resource *mr = nullptr);

/// @brief Maximum wavelet decomposition level — `L = wmaxlev(N, wname)`.
///
/// `L = floor(log2(N / (Lf - 1)))`, where `Lf` is the decomposition-filter
/// length of `wname`. `N` may be a scalar or a vector (the minimum is used,
/// e.g. for a 2-D image size `[r c]`). Verified vs MATLAB R2025b:
/// `wmaxlev(64, 'db2') = 4`, `wmaxlev([8 8], 'db1') = 3`.
///
/// @param N      Signal length (scalar) or size vector (min is used).
/// @param wname  Wavelet name (e.g. `"db2"`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Scalar maximum decomposition level (`>= 0`).
/// @throws Error if `N` is empty or the filter length is `< 2`.
Value wmaxlev(const Value &N, const std::string &wname,
              std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::wavelet
