// libs/wavelet/include/numkit/wavelet/dwt/dwt2.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>

namespace numkit::wavelet {

/// Result of a single-level 2-D DWT (MATLAB `[cA, cH, cV, cD] = dwt2(...)`).
///   cA — approximation (Lo-rows then Lo-cols)
///   cH — horizontal detail (Lo-rows then Hi-cols)
///   cV — vertical detail   (Hi-rows then Lo-cols)
///   cD — diagonal detail   (Hi-rows then Hi-cols)
struct Dwt2Result {
    Value cA;
    Value cH;
    Value cV;
    Value cD;
};

/// `[cA, cH, cV, cD] = dwt2(X, wname)` — separable 2-D single-level
/// DWT. Composes the 1-D dwt of cycle 26: a row pass first (each row
/// of X analysed into a low + high half), then a column pass on each
/// half.
Dwt2Result dwt2(const Value &X, const std::string &wname,
                std::pmr::memory_resource *mr = nullptr);

/// Inverse 2-D DWT. Optional `outRows`/`outCols` (≥0) crop the
/// reconstruction to the requested size; pass -1 to use the natural
/// length 2*la - Lf + 2 in each dim.
Value idwt2(const Value &cA, const Value &cH,
            const Value &cV, const Value &cD,
            const std::string &wname,
            long long outRows /* = -1 */, long long outCols /* = -1 */,
            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::wavelet
