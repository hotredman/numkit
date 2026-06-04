// libs/wavelet/include/numkit/wavelet/dwt/haart.hpp
//
// 1-D Haar discrete wavelet transform (haart).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::wavelet {

/// @brief Result of `haart` — `[a, d]`.
struct HaartResult {
    Value a;  ///< Final-level approximation (column-oriented; per-column for a matrix).
    Value d;  ///< Detail coefficients: a plain matrix when `level == 1`,
              ///< else a length-`level` cell column (`d{1}` is the finest scale).
};

/// @brief 1-D Haar wavelet transform (`[a, d] = haart(x, level, integerflag)`).
///
/// Decomposes `x` (vector or per-column matrix; output is column-oriented)
/// into `level` Haar levels. `noninteger` mode (default) uses the orthogonal
/// pair (factor `1/√2`); `integer` mode uses the lifting integer Haar
/// (`d = x2 - x1`, `a = x1 + floor(d/2)`). The data length must be even and
/// divisible by `2^level`.
///
/// @param x        Input vector or matrix (even length per column).
/// @param level    Number of levels; `<= 0` (default) → the maximum level
///                 (largest `k` with `2^k | length`).
/// @param integer  `false` (default) → orthogonal; `true` → integer lifting
///                 (not supported for complex input).
/// @param mr       Memory resource (nullptr → process default).
/// @return         @ref HaartResult `{ a, d }`.
/// @throws Error on empty/odd-length input, `level` above the maximum, or
///         `integer == true` with complex input.
HaartResult haart(const Value &x, int level = 0, bool integer = false,
                  std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::wavelet
