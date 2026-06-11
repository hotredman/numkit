// toolboxes/stats/include/numkit/stats/moments/moments.hpp
//
// Higher moments: skewness, kurtosis.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::stats {

/// @brief Sample skewness along `dim` (`y = skewness(X, normFlag, dim)`).
///
/// Standardised third central moment: @f$ E[((X-\mu)/\sigma)^3] @f$.
/// - `normFlag = 1` (default): uncorrected `y = m3 / m2^1.5`
/// - `normFlag = 0`: bias-corrected `y *= sqrt(n·(n-1))/(n-2)`. Requires `n >= 3`.
///
/// @param x         Input array.
/// @param normFlag  Bias correction switch (`0` or `1`).
/// @param dim       1-based dimension; 0 → first non-singleton dim.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Skewness reduced along `dim`.
/// @see kurtosis
Value skewness(const Value &x, int normFlag = 1, int dim = 0,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Sample kurtosis along `dim` (`y = kurtosis(X, normFlag, dim)`).
///
/// **Non-excess** kurtosis (`= 3` for a standard normal).
/// - `normFlag = 1` (default): uncorrected `y = m4 / m2^2`
/// - `normFlag = 0`: bias-corrected
///   `y = ((n-1)/((n-2)(n-3))) · ((n+1)·g2 - 3·(n-1)) + 3` where `g2 = m4/m2^2`.
///   Requires `n >= 4`.
///
/// @param x         Input array.
/// @param normFlag  Bias correction switch (`0` or `1`).
/// @param dim       1-based dimension; 0 → first non-singleton dim.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Kurtosis reduced along `dim`.
/// @see skewness
Value kurtosis(const Value &x, int normFlag = 1, int dim = 0,
               std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
