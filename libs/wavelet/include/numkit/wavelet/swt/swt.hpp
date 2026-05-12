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
Value swt(const Value &x, int n, const std::string &wname, std::pmr::memory_resource *mr = nullptr);

/// Inverse stationary DWT. `swc` is an (n+1) × N matrix laid out as
/// returned by swt; we recover the n-level approximation from row
/// n+1 then walk back up averaging shift variants.
Value iswt(const Value &swc, const std::string &wname, std::pmr::memory_resource *mr = nullptr);

/// `swc = modwt(x, n, wname)` — maximal-overlap discrete wavelet
/// transform, n levels. Output is an (n+1) × N matrix:
///   row 1..n : wavelet coefficients W_j (level 1 = finest)
///   row n+1  : scaling coefficients V_n (final approximation)
/// All rows are length N. Filters are scaled by 1/√2 each level so
/// the transform is energy-preserving (Parseval's holds), unlike
/// `swt` which uses unit-energy filters and requires an explicit
/// /2 redundancy factor on inversion. Periodic boundary; signal
/// length need NOT divide 2^n (the transform is shift-invariant).
Value modwt(const Value &x, int n, const std::string &wname, std::pmr::memory_resource *mr = nullptr);

/// Inverse MODWT — exact left-inverse of `modwt`.
Value imodwt(const Value &swc, const std::string &wname, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::wavelet
