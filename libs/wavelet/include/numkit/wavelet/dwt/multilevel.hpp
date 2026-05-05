// libs/wavelet/include/numkit/wavelet/dwt/multilevel.hpp
//
// Multi-level discrete wavelet transform: wavedec / waverec, plus the
// appcoef / detcoef extractors that index into the (C, L) bookkeeping
// vectors.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>

namespace numkit::wavelet {

/// `[C, L] = wavedec(x, n, wname)` — multi-level 1-D DWT.
/// C concatenates [cA_n, cD_n, cD_{n-1}, ..., cD_1] as a single row.
/// L = [length(cA_n), length(cD_n), ..., length(cD_1), length(x)].
void wavedec(std::pmr::memory_resource *mr,
             const Value &x, int n, const std::string &wname,
             Value *C, Value *L);

/// Inverse of wavedec.
Value waverec(std::pmr::memory_resource *mr,
              const Value &C, const Value &L, const std::string &wname);

/// `appcoef(C, L, wname [, level])` — extract the approximation at
/// `level`. `level == -1` (the default in this API) returns the
/// coarsest approximation (cA at the deepest level).
Value appcoef(std::pmr::memory_resource *mr,
              const Value &C, const Value &L, const std::string &wname,
              int level);

/// `detcoef(C, L, level)` — extract the detail vector at `level`
/// (1-based; level=1 is the finest). MATLAB also accepts a 'cells'
/// keyword to return all details at once — we expose that via
/// `level == 0` returning a cell-like row of `Value`s, but the C++
/// helper here keeps the simple single-level form.
Value detcoef(std::pmr::memory_resource *mr,
              const Value &C, const Value &L, int level);

/// `wrcoef(type, c, l, wname[, n])` — single-band reconstruction.
/// type ∈ {'a', 'd'}; n is the level (default = max = length(l)-2;
/// 'a' allows n=0 for full reconstruction; 'd' requires n ∈ [1, max]).
/// Pass n = -1 to request the default. Output is a row of length |x|.
Value wrcoef(std::pmr::memory_resource *mr,
             const std::string &type, const Value &c, const Value &l,
             const std::string &wname, int n);

} // namespace numkit::wavelet
