// libs/wavelet/include/numkit/wavelet/dwt/dwt2.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>

namespace numkit::wavelet {

/// `[cA, cH, cV, cD] = dwt2(X, wname)` — separable 2-D single-level
/// DWT. Composes the 1-D dwt of cycle 26: a row pass first (each row
/// of X analysed into a low + high half), then a column pass on each
/// half. MATLAB labels:
///   cA (approximation) = Lo-rows then Lo-cols
///   cH (horizontal det.) = Lo-rows then Hi-cols
///   cV (vertical det.)   = Hi-rows then Lo-cols
///   cD (diagonal det.)   = Hi-rows then Hi-cols
void dwt2(std::pmr::memory_resource *mr,
          const Value &X, const std::string &wname,
          Value *cA, Value *cH, Value *cV, Value *cD);

/// Inverse 2-D DWT. Optional `outRows`/`outCols` (≥0) crop the
/// reconstruction to the requested size; pass -1 to use the natural
/// length 2*la - Lf + 2 in each dim.
Value idwt2(std::pmr::memory_resource *mr,
            const Value &cA, const Value &cH,
            const Value &cV, const Value &cD,
            const std::string &wname,
            long long outRows /* = -1 */, long long outCols /* = -1 */);

} // namespace numkit::wavelet
