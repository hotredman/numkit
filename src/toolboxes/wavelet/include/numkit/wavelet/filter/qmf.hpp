/// @file qmf.hpp
/// @ingroup group_wavelet
// toolboxes/wavelet/include/numkit/wavelet/filter/qmf.hpp
//
// Wavelet filter helpers: qmf (quadrature mirror filter) and wrev (reverse).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::wavelet {

/// @brief Reverse a vector / flip each column — `y = wrev(x)`.
///
/// Row vector → element reverse; column vector → element reverse; matrix
/// (M×N) → per-column reverse (= `flipud`). Complex input preserves its
/// imaginary part.
///
/// @param x   Input array (real or complex).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Reversed array, same shape as `x`.
/// @see qmf
Value wrev(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Quadrature mirror filter — `y = qmf(x, p)`.
///
/// `y(k) = (-1)^(k-1) * x(N-k+1)` for `p == 0` (default); the whole result
/// is negated for `p == 1`. Verified vs MATLAB R2025b:
/// `qmf([1 2 3 4]) = [4 -3 2 -1]`, `qmf([1 2 3 4], 1) = [-4 3 -2 1]`.
///
/// @param x   Input filter (real).
/// @param p   Parity, `0` (default) or `1` (collapsed mod 2).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quadrature mirror filter, same shape as `x`.
/// @see wrev
Value qmf(const Value &x, int p = 0, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::wavelet
