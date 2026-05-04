// libs/wavelet/include/numkit/wavelet/dwt/dwt.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>

namespace numkit::wavelet {

/// Single-level discrete wavelet transform.
///   [cA, cD] = dwt(x, wname)
/// Default extension mode is 'sym' (whole-point symmetric reflection),
/// matching MATLAB's default `dwtmode('sym')`. Output length per band
/// is `floor((N + Lf - 1) / 2)` where Lf is the filter length.
void dwt(std::pmr::memory_resource *mr,
         const Value &x, const std::string &wname,
         Value *cA, Value *cD);

/// Inverse of dwt. `len` (when ≥ 0) crops the result to that length;
/// otherwise we return the natural reconstruction length
/// `2*length(cA) - Lf + 2` (MATLAB's behaviour for 'sym' mode).
Value idwt(std::pmr::memory_resource *mr,
           const Value &cA, const Value &cD,
           const std::string &wname,
           long long len /* = -1 */);

} // namespace numkit::wavelet
