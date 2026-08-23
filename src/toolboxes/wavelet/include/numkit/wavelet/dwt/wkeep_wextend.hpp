/// @file wkeep_wextend.hpp
/// @ingroup group_wavelet
// toolboxes/wavelet/include/numkit/wavelet/dwt/wkeep_wextend.hpp
//
// Wavelet Toolbox boundary helpers: wkeep (keep central/edge part) and
// wextend (signal extension) — declared as they are lifted from
// adapter-only to a public C++ API.

#pragma once

#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>

namespace numkit::wavelet {

/// @addtogroup group_wavelet
/// @{


/// @brief Keep part of a vector / a sub-matrix (`y = wkeep(x, len, opt)`).
///
/// 1-D form (`len` is a scalar `n`): keep `n` elements of the vector `x` —
/// centered (default, or `opt == 'c'`), the first `n` (`'l'`), the last `n`
/// (`'r'`), or `n` starting at the 1-based index given by a numeric `opt`.
/// 2-D form (`len == [R C]`): extract an `R × C` sub-matrix of `x` — centered
/// (default) or anchored at the 1-based corner `opt == [fr fc]`.
///
/// This is a genuinely polymorphic MATLAB function (the second and third
/// arguments switch shape and type), so the public entry takes `Value`
/// arguments and dispatches internally rather than splitting into overloads.
///
/// @param x    Input vector or matrix.
/// @param len  Length `n` (1-D) or `[R C]` (2-D).
/// @param opt  Keep-mode: `Value::Empty` (centered, default), the strings
///             `'c'`/`'l'`/`'r'`, a numeric 1-based start (1-D), or the
///             corner `[fr fc]` (2-D).
/// @param mr   Memory resource (nullptr → process default).
/// @return     The kept vector / sub-matrix.
/// @throws Error on an out-of-range window or an unknown `opt` string.
Value wkeep(const Value &x, const Value &len,
            const Value &opt = Value::Empty,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Extend a signal at its boundaries (`y = wextend(type, mode, x, lf, side)`).
///
/// 1-D (`type` 1) extends a vector; 2-D (`type` 2 / `'ar'` / `'ac'`) extends
/// a matrix (both axes, columns-only for `'ar'`, rows-only for `'ac'`) by
/// `lf` samples per active end. `mode` is the boundary rule: `sym`/`symh`
/// (half-point symmetric), `symw` (whole-point), `asym`/`asymh`/`asymw`
/// (antisymmetric), `sp0`/`sp1` (order-0/1 spline), `per` (periodic, edge-
/// padded on odd length), `zpd` (zero), `ppd` (pure periodic). `side`
/// selects which end(s): `'b'` (both, default), `'l'`, or `'r'`.
///
/// `type` stays a `Value` because MATLAB accepts `1`/`2`/`'1'`/`'ar'`/`'ac'`.
///
/// @param type  Extension type: `1`, `2`, `'ar'`, or `'ac'`.
/// @param mode  Boundary mode string (see list above).
/// @param x     Input vector or matrix.
/// @param lf    Extension length per active end (`>= 0`).
/// @param side  `"b"` (default), `"l"`, or `"r"`.
/// @param mr    Memory resource (nullptr → process default).
/// @return      The extended signal.
/// @throws Error on an unknown `type`/`mode`/`side` or negative `lf`.
/// @see wkeep
Value wextend(const Value &type, const std::string &mode, const Value &x,
              long long lf, const std::string &side = "b",
              std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::wavelet
