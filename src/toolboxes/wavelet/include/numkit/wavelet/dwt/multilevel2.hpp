/// @file multilevel2.hpp
/// @ingroup group_wavelet
// toolboxes/wavelet/include/numkit/wavelet/dwt/multilevel2.hpp

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>

namespace numkit::wavelet {

/// Result of @ref wavedec2 (`[C, S] = wavedec2(X, N, wname)`).
///
/// `C` is a row vector packing the coefficients coarsest-first:
/// `[cA_N | cH_N cV_N cD_N | … | cH_1 cV_1 cD_1]`, each sub-band
/// linearised column-major. `S` is the `(N+2)×2` bookkeeping matrix of
/// sub-band sizes: `S(1,:)=size(cA_N)`, `S(i,:)=size of the level-(N-i+2)
/// detail` for `i=2..N+1`, and `S(N+2,:)=size(X)`.
struct Wavedec2Result {
    Value c;
    Value s;
};

/// 2-D multilevel wavelet decomposition. Iterates @ref dwt2 `n` times on
/// the running approximation (LL) band and packs the `[C, S]` layout.
///
/// @param X      Input image (2-D matrix).
/// @param n      Number of decomposition levels (≥ 1).
/// @param wname  Wavelet name (see @ref wavelet_filters).
/// @param mr     Memory resource (nullptr → process default).
/// @return       @ref Wavedec2Result.
/// @see waverec2, appcoef2, detcoef2
Wavedec2Result wavedec2(const Value &X, int n, const std::string &wname,
                        std::pmr::memory_resource *mr = nullptr);

/// Full 2-D multilevel reconstruction (`waverec2(C, S, wname)`), i.e. the
/// approximation at level 0. Inverts @ref wavedec2 via repeated @ref idwt2.
///
/// @see wavedec2, appcoef2
Value waverec2(const Value &C, const Value &S, const std::string &wname,
               std::pmr::memory_resource *mr = nullptr);

/// Extract / reconstruct the approximation coefficients at `level`
/// (`appcoef2(C, S, wname, level)`). `level == N` returns the stored
/// coarsest approximation directly; `level < N` reconstructs down through
/// @ref idwt2 using the stored detail bands; `level == 0` is the full
/// reconstruction. Default `level = -1` → the coarsest level `N`.
///
/// @see wavedec2, waverec2
Value appcoef2(const Value &C, const Value &S, const std::string &wname,
               int level = -1, std::pmr::memory_resource *mr = nullptr);

/// Extract a 2-D detail sub-band (`detcoef2(type, C, S, level)`).
///
/// @param type   `"h"` (horizontal), `"v"` (vertical) or `"d"` (diagonal).
/// @param C      Coefficient vector from @ref wavedec2.
/// @param S      Size bookkeeping matrix from @ref wavedec2.
/// @param level  Decomposition level (1 = finest … N = coarsest).
/// @param mr     Memory resource.
/// @return       The requested detail band, shaped to its 2-D size.
/// @see wavedec2
Value detcoef2(const std::string &type, const Value &C, const Value &S,
               int level, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::wavelet
