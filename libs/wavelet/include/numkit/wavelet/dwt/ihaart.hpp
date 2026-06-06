// libs/wavelet/include/numkit/wavelet/dwt/ihaart.hpp
//
// 1-D inverse Haar discrete wavelet transform (ihaart).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::wavelet {

/// @brief 1-D inverse Haar wavelet transform
/// (`xrec = ihaart(a, d, level, integerflag)`).
///
/// Reconstructs the signal from the approximation `a` and detail `d` (a
/// plain matrix for a single-level transform, else a length-`Nlevels` cell
/// column with `d{1}` the finest scale — matching @ref haart's output).
/// `level` (default 0 = lossless) zeroes the finest `level` detail bands
/// before reconstruction. `integer` selects the lifting integer inverse
/// (`x0 = a - floor(d/2)`, `x1 = x0 + d`), unavailable for complex `a`.
/// `d` must be real.
///
/// @param a        Approximation coefficients (column / matrix).
/// @param d        Detail — a matrix (1 level) or a cell column (multi-level).
/// @param level    Finest detail bands to suppress, in `[0, Nlevels)`
///                 (default 0 → lossless).
/// @param integer  `false` (default) → orthogonal; `true` → integer lifting.
/// @param mr       Memory resource (nullptr → process default).
/// @return         Reconstructed signal (column for vector input).
/// @throws Error on empty `a`/`d`, complex `d`, `level` outside
///         `[0, Nlevels)`, or `integer == true` with complex `a`.
/// @see haart
Value ihaart(const Value &a, const Value &d, int level = 0,
             bool integer = false, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::wavelet
