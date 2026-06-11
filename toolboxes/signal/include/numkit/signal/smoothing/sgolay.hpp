// toolboxes/signal/include/numkit/signal/smoothing/sgolay.hpp
//
// Savitzky-Golay smoothing filter family.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

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

/// Savitzky-Golay differentiation-filter matrix (the second output `G` of
/// MATLAB's `[B,G] = sgolay(order,framelen)`).
///
/// Returns the `framelen × (order+1)` matrix `G = V·(VᵀV)⁻¹`, where `V` is
/// the centred Vandermonde matrix of the window offsets. Column `j`
/// (1-based) is the FIR filter that, applied to a length-`framelen` window,
/// estimates the polynomial coefficient `a_j` at the central point — so
/// `G(:,1)` is the smoothing filter (equals the central row of `sgolay`)
/// and `factorial(j-1) · G(:,j)` estimates the `(j-1)`-th derivative.
///
/// @param order     Polynomial order. Must satisfy `0 ≤ order < framelen`.
/// @param framelen  Window length. Must be odd and ≥ `order + 1`.
/// @param mr        Memory resource (nullptr → process default).
/// @return          `framelen × (order+1)` DOUBLE differentiation matrix.
/// @throws          numkit::Error  on invalid `order` / `framelen`.
///
/// @see sgolay
Value sgolayDiff(int                          order,
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

/// Savitzky-Golay smoothing with optional weighting and dimension.
///
/// Generalises @ref sgolayfilt to matrices and weighted least-squares:
///   - If `x` is a matrix, each 1-D slice along `dim` is filtered
///     independently (`dim = 1` → columns, `dim = 2` → rows). `dim = 0`
///     selects the first non-singleton dimension (the MATLAB default).
///   - `weights` is an empty Value (unweighted) or a `framelen`-length
///     vector of positive weights for the local least-squares fit.
///
/// @param x         Real 1-D or 2-D signal.
/// @param order     Polynomial order (`0 ≤ order < framelen`).
/// @param framelen  Window length (odd, ≥ `order + 1`).
/// @param weights   Empty, or `framelen` positive weights.
/// @param dim       Operating dimension (1, 2, or 0 = auto).
/// @param mr        Memory resource (nullptr → process default).
/// @return          Filtered signal, same shape as `x`.
///
/// @see sgolay, medfilt1
Value sgolayfilt(const Value &                x,
                 int                          order,
                 int                          framelen,
                 const Value &                weights,
                 int                          dim,
                 std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
