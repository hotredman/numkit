/// @file dwt2.hpp
/// @ingroup group_wavelet
// toolboxes/wavelet/include/numkit/wavelet/dwt/dwt2.hpp

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>

namespace numkit::wavelet {

/// Result of @ref dwt2 (`[cA, cH, cV, cD] = dwt2(X, wname)`).
///
/// Naming follows the standard 2-D wavelet decomposition:
///   - cA — approximation (Lo-rows then Lo-cols, the LL band).
///   - cH — horizontal detail (Lo-rows then Hi-cols, the LH band).
///   - cV — vertical detail   (Hi-rows then Lo-cols, the HL band).
///   - cD — diagonal detail   (Hi-rows then Hi-cols, the HH band).
struct Dwt2Result {
    Value cA;
    Value cH;
    Value cV;
    Value cD;
};

/// 2-D separable single-level wavelet transform.
///
/// Composes the 1-D @ref dwt: a row pass first (each row of `X`
/// analysed into lo + hi halves), then a column pass on each half.
/// Returns the four bands packaged in @ref Dwt2Result.
///
/// @param X      Input image (2-D matrix).
/// @param wname  Wavelet name (see @ref wavelet_filters for the
///               supported family list).
/// @param mr     Memory resource (nullptr → process default).
/// @return       @ref Dwt2Result; bind via `auto r = dwt2(X, wname);`.
///
/// @code
/// auto r = dwt2(image, "db2");
/// // r.cA is the LL band, r.cH/cV/cD are the detail bands.
/// @endcode
///
/// @see idwt2, dwt
Dwt2Result dwt2(const Value &X, const std::string &wname,
                std::pmr::memory_resource *mr = nullptr);

/// 2-D inverse single-level wavelet transform.
///
/// Reconstructs the image from the four bands produced by @ref dwt2.
/// Optional `outRows` / `outCols` (≥ 0) crop the reconstruction to
/// the requested size; pass -1 to use the natural length
/// `2·la - L_f + 2` in each dim.
///
/// @param cA       Approximation band (LL).
/// @param cH       Horizontal detail band (LH).
/// @param cV       Vertical detail band (HL).
/// @param cD       Diagonal detail band (HH).
/// @param wname    Wavelet name (must match decomposition).
/// @param outRows  Output rows (-1 for natural).
/// @param outCols  Output cols (-1 for natural).
/// @param mr       Memory resource (nullptr → process default).
/// @return             Reconstructed `outRows × outCols` matrix.
///
/// @see dwt2
Value idwt2(const Value &cA, const Value &cH,
            const Value &cV, const Value &cD,
            const std::string &wname,
            long long outRows /* = -1 */, long long outCols /* = -1 */,
            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::wavelet
