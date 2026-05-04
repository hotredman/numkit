// libs/wavelet/include/numkit/wavelet/swt/swt.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>

namespace numkit::wavelet {

/// `swc = swt(x, n, wname)` — stationary (à trous) DWT, n levels.
/// Output is an (n+1) × N matrix; rows 1..n hold the detail
/// coefficients at levels 1..n (1 = finest), row n+1 holds the
/// approximation at level n. All rows are length N.
///
/// Boundary: periodic (MATLAB default for swt). N must be divisible
/// by 2^n.
Value swt(std::pmr::memory_resource *mr,
          const Value &x, int n, const std::string &wname);

/// Inverse stationary DWT. `swc` is an (n+1) × N matrix laid out as
/// returned by swt; we recover the n-level approximation from row
/// n+1 then walk back up averaging shift variants.
Value iswt(std::pmr::memory_resource *mr,
           const Value &swc, const std::string &wname);

} // namespace numkit::wavelet
