// libs/wavelet/include/numkit/wavelet/dwt/dwt.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>
#include <utility>
#include <vector>

namespace numkit::wavelet {

/// Single-level discrete wavelet transform.
///   [cA, cD] = dwt(x, wname)
/// Default extension mode is 'sym' (whole-point symmetric reflection),
/// matching MATLAB's default `dwtmode('sym')`. Output length per band
/// is `floor((N + Lf - 1) / 2)` where Lf is the filter length.
///
/// @return `(cA, cD)`; bind as `auto [cA, cD] = dwt(x, wname);`.
std::pair<Value, Value>
dwt(const Value &x, const std::string &wname,
    std::pmr::memory_resource *mr = nullptr);

/// Inverse of dwt. `len` (when ≥ 0) crops the result to that length;
/// otherwise we return the natural reconstruction length
/// `2*length(cA) - Lf + 2` (MATLAB's behaviour for 'sym' mode).
Value idwt(const Value &cA, const Value &cD,
           const std::string &wname,
           long long len /* = -1 */,
           std::pmr::memory_resource *mr = nullptr);

/// Internal entry: takes Lo_R / Hi_R directly. Used by appcoef to
/// support the `(c, l, LoR, HiR)` custom-filter form without
/// re-resolving filters by name on every cascade step.
Value idwt_with_filters_pub(const Value &cA, const Value &cD,
                            const std::vector<double> &Lo_R,
                            const std::vector<double> &Hi_R,
                            long long len,
                            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::wavelet
