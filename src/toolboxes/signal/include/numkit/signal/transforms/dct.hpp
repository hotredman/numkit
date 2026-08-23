/// @file dct.hpp
/// @ingroup group_signal
// toolboxes/signal/include/numkit/signal/transforms/dct.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::signal {

/// Type-II discrete cosine transform (the default form).
///
/// 1-D entry point — computes
/// \f$ Y[k] = w[k] \sum_{n=0}^{N-1} x[n] \cos\!\left(\frac{\pi (2n+1) k}{2N}\right) \f$
/// where \f$ w[0] = \sqrt{1/N},\ w[k>0] = \sqrt{2/N} \f$ (orthonormal form).
///
/// @param x   Real 1-D vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape DOUBLE vector.
///
/// @code  Value Y = dct({1.0, 2.0, 3.0, 4.0});  @endcode
///
/// @see idct
Value dct(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse Type-II DCT (1-D form).
///
/// Exact left-inverse of @ref dct(const Value &, std::pmr::memory_resource *).
///
/// @param x   Real 1-D vector (DCT coefficients).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape DOUBLE vector (reconstructed signal).
/// @see dct
Value idct(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// DCT-II with explicit length / axis.
///
/// @param x    Real input (1-D or 2-D).
/// @param n    Output length along `dim`. `n <= 0` ≡ use native extent.
///             `n > axis_len` zero-pads; `n < axis_len` truncates.
/// @param dim  Axis to transform along. `0` (default) = first non-singleton.
///             `1` = rows, `2` = columns.
/// @param mr   Memory resource (nullptr → process default).
/// @return     DOUBLE array with `dim` axis replaced by length `n`.
///
/// @code
/// Value Y = dct(matrix, -1, 1);   // per-column DCT of a 2-D matrix
/// @endcode
Value dct(const Value &                x,
          int                          n,
          int                          dim,
          std::pmr::memory_resource *  mr = nullptr);

/// @brief Inverse DCT-II with explicit length / axis.
///
/// Exact left-inverse of @ref dct(const Value &, int, int, std::pmr::memory_resource *).
///
/// @param x    Real input (1-D or 2-D) of DCT coefficients.
/// @param n    Output length along `dim`. `n <= 0` ≡ use native extent.
///             `n > axis_len` zero-pads; `n < axis_len` truncates.
/// @param dim  Axis to transform along. `0` (default) = first non-singleton.
///             `1` = rows, `2` = columns.
/// @param mr   Memory resource (nullptr → process default).
/// @return     DOUBLE array with `dim` axis replaced by length `n`.
/// @see dct
Value idct(const Value &x, int n, int dim,
           std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
