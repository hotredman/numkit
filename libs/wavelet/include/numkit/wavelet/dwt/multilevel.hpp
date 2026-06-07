// libs/wavelet/include/numkit/wavelet/dwt/multilevel.hpp
//
// Multi-level discrete wavelet transform: wavedec / waverec, plus the
// appcoef / detcoef extractors that index into the (C, L) bookkeeping
// vectors.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>
#include <utility>

namespace numkit::wavelet {

/// Multi-level 1-D DWT (`[C, L] = wavedec(x, n, wname)`).
///
/// Runs `n` successive single-level @ref dwt passes on the running
/// approximation band. Packs the result into the canonical
/// `(C, L)` layout:
///
///   `C = [cA_n, cD_n, cD_{n-1}, ..., cD_1]`           (concatenated row)
///   `L = [|cA_n|, |cD_n|, ..., |cD_1|, |x|]`         (length n+2)
///
/// @param x      Input signal (vector).
/// @param n      Decomposition depth (≥ 1).
/// @param wname  Wavelet name.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(C, L)`; bind via `auto [C, L] = wavedec(x, n, wname);`.
/// @throws       Error if `n < 1`.
///
/// @code
/// auto [C, L] = wavedec(signal, 3, "sym4");
/// // appcoef(C, L, "sym4", 3)  ≈ coarsest approximation
/// // detcoef(C, L, 1)          ≈ finest detail
/// @endcode
///
/// @see waverec, appcoef, detcoef, wrcoef
std::pair<Value, Value>
wavedec(const Value &x, int n, const std::string &wname,
        std::pmr::memory_resource *mr = nullptr);

/// Inverse of @ref wavedec.
///
/// Iteratively applies @ref idwt from coarsest to finest, using the
/// length bookkeeping in `L` to set each reconstruction target.
///
/// @param C      Coefficient row from @ref wavedec.
/// @param L      Bookkeeping row from @ref wavedec.
/// @param wname  Wavelet name (must match decomposition).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Reconstructed row vector of length `L(end)`.
/// @throws       Error if `length(L) < 3` or C/L sizes are inconsistent.
///
/// @see wavedec
Value waverec(const Value &C, const Value &L, const std::string &wname,
              std::pmr::memory_resource *mr = nullptr);

/// Extract approximation coefficients at a given level (`appcoef`).
///
/// The `appcoef(C, L, wname, level)` operation.
/// - `level == nMax = length(L) - 2` (the default if `level == -1`)
///   returns the coarsest cA stored verbatim at the front of C.
/// - `level < nMax` rebuilds the approximation by running idwt
///   `nMax - level` times.
///
/// @param C      Coefficient row from @ref wavedec.
/// @param L      Bookkeeping row.
/// @param wname  Wavelet name.
/// @param level  Target level (or -1 for default = coarsest).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Row vector of approximation coefficients.
/// @throws       Error on out-of-range level.
///
/// @see detcoef, wrcoef
Value appcoef(const Value &C, const Value &L, const std::string &wname,
              int level,
              std::pmr::memory_resource *mr = nullptr);

/// Internal entry: appcoef with explicit synthesis filters.
///
/// Used by the `appcoef(C, L, Lo_R, Hi_R[, level])` custom-filter form so
/// the register TU can invoke it without re-resolving filters by name.
/// Public so other libs/wavelet TUs can call it; not commonly needed by
/// end users.
///
/// @param C      Coefficient row from @ref wavedec.
/// @param L      Bookkeeping row.
/// @param Lo_R   Synthesis lowpass filter coefficients.
/// @param Hi_R   Synthesis highpass filter coefficients.
/// @param level  Target level (or -1 for default = coarsest).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Row vector of approximation coefficients.
///
/// @see appcoef
Value appcoef_with_filters_pub(const Value &C, const Value &L,
                               const std::vector<double> &Lo_R,
                               const std::vector<double> &Hi_R,
                               int level,
                               std::pmr::memory_resource *mr = nullptr);

/// Extract detail coefficients at a given level (`detcoef(C, L, level)`).
///
/// `level` is 1-based: level = 1 is the finest detail, level =
/// `length(L) - 2` the coarsest. Returns a row vector slice of C.
///
/// The `detcoef(C, L, levels, 'cells')` multi-level form is
/// reachable through the engine-level adapter; the C++ helper here
/// keeps the simple single-level form.
///
/// @param C      Coefficient row from @ref wavedec.
/// @param L      Bookkeeping row.
/// @param level  1-based level (1 = finest).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Row vector of detail coefficients.
/// @throws       Error on out-of-range level.
///
/// @see appcoef, wrcoef
Value detcoef(const Value &C, const Value &L, int level,
              std::pmr::memory_resource *mr = nullptr);

/// Single-band reconstruction (`wrcoef(type, c, l, wname, n)`).
///
/// Reconstructs the approximation (`type = "a"`) or a single detail
/// band (`type = "d"`) of `(c, l)` at level `n`, with all other bands
/// zeroed out before running @ref waverec. Output is a row of length
/// `|x|` (= `l(end)`).
///
/// - `type = "a"`: `n ∈ [0, N]`. `n = 0` reconstructs the full
///   signal (identity); `n = N` reconstructs only the coarsest
///   approximation.
/// - `type = "d"`: `n ∈ [1, N]`. Reconstructs only the detail at
///   level `n`.
///
/// Pass `n = -1` to use the default (n = N = `length(l) - 2`).
///
/// @param type   `"a"` or `"d"`.
/// @param c      Coefficient row from @ref wavedec.
/// @param l      Bookkeeping row.
/// @param wname  Wavelet name.
/// @param n      Level (or -1 for default).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Row vector of reconstructed band.
/// @throws       Error on bad type, level out of range, or C/L mismatch.
///
/// @see appcoef, detcoef, waverec
Value wrcoef(const std::string &type, const Value &c, const Value &l,
             const std::string &wname, int n,
             std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::wavelet
