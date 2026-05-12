// libs/signal/include/numkit/signal/smoothing/sgolay.hpp
//
// Savitzky-Golay smoothing filter family.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Savitzky-Golay filter projection matrix.
///
/// Returns the `framelen × framelen` projection matrix B such that row
/// `r` contains the FIR coefficients producing the polynomial-fit
/// estimate at the r-th sample of a length-`framelen` window. The
/// central row (`r = floor(framelen/2)`) is the symmetric filter; the
/// other rows are the asymmetric filters used at the signal edges.
///
/// @param order     Polynomial order. Must satisfy `0 ≤ order < framelen`.
/// @param framelen  Window length. Must be odd and ≥ `order + 1`.
/// @param mr        Memory resource (nullptr → process default).
/// @return          `framelen × framelen` DOUBLE projection matrix.
/// @throws          numkit::Error  on invalid `order` / `framelen`.
///
/// @see sgolayfilt
Value sgolay(int                          order,
             int                          framelen,
             std::pmr::memory_resource *  mr = nullptr);

/// Savitzky-Golay smoothing of a 1-D signal.
///
/// Fits a local polynomial of `order` to a sliding window of
/// `framelen` samples (via least-squares), then evaluates the
/// polynomial at the centre. Smooths the signal while preserving
/// features up to `order`-th derivative.
///
/// Interior samples use the central row of `sgolay`'s projection
/// matrix; edge samples use the asymmetric rows so no zero-padding
/// artefacts appear.
///
/// @param x         Real 1-D signal.
/// @param order     Polynomial order (`0 ≤ order < framelen`).
/// @param framelen  Window length (odd, ≥ `order + 1`).
/// @param mr        Memory resource (nullptr → process default).
/// @return          Filtered signal, same shape as `x`.
///
/// @code
/// // Smooth a noisy signal preserving local quadratic features:
/// Value y = sgolayfilt(noisy, 2, 11);
/// @endcode
///
/// @see sgolay, medfilt1
Value sgolayfilt(const Value &                x,
                 int                          order,
                 int                          framelen,
                 std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
