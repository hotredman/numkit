// libs/wavelet/include/numkit/wavelet/filter/wfilters.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>
#include <vector>

namespace numkit::wavelet {

/// Returns the four filter banks for a named orthogonal wavelet:
///   Lo_D (analysis lowpass), Hi_D (analysis highpass),
///   Lo_R (synthesis lowpass), Hi_R (synthesis highpass).
///
/// Recognised names: haar / db1, db2, db3, db4, sym2, sym4, coif1.
/// (More families can be added without touching dwt/idwt — they only
/// consume the four returned vectors.)
struct FilterBank {
    std::vector<double> Lo_D;
    std::vector<double> Hi_D;
    std::vector<double> Lo_R;
    std::vector<double> Hi_R;
};

FilterBank wavelet_filters(const std::string &name);

/// MATLAB `wfilters(wname)` — returns 4 outputs in MATLAB order.
/// `wfilters(wname, 'd')` → [Lo_D, Hi_D]
/// `wfilters(wname, 'r')` → [Lo_R, Hi_R]
/// `wfilters(wname, 'l')` → [Lo_D, Lo_R]
/// `wfilters(wname, 'h')` → [Hi_D, Hi_R]
void wfilters(std::pmr::memory_resource *mr,
              const std::string &name, const std::string &kind,
              Value *out0, Value *out1,
              Value *out2, Value *out3);

} // namespace numkit::wavelet
