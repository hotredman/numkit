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

/// Result of MATLAB `wfilters(wname[, kind])`. Which fields are
/// populated depends on `kind`:
///   "" / "all": all four set.
///   "d":  Lo_D, Hi_D set; Lo_R, Hi_R empty.
///   "r":  Lo_R, Hi_R set; Lo_D, Hi_D empty.
///   "l":  Lo_D, Lo_R set; Hi_D, Hi_R empty.
///   "h":  Hi_D, Hi_R set; Lo_D, Lo_R empty.
struct WFiltersResult {
    Value Lo_D;
    Value Hi_D;
    Value Lo_R;
    Value Hi_R;
};

/// MATLAB `wfilters(wname[, kind])`.
///
/// @param name  Wavelet family name (haar, db1..db4, sym2, sym4, coif1, …).
/// @param kind  Selector: "", "d", "r", "l", "h". Empty/missing returns
///              all four filters (4-output MATLAB form).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Populated subset of `{Lo_D, Hi_D, Lo_R, Hi_R}` per `kind`.
WFiltersResult wfilters(const std::string &name, const std::string &kind = "",
                        std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::wavelet
