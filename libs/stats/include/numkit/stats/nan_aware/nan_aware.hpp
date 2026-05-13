// libs/stats/include/numkit/stats/nan_aware/nan_aware.hpp
//
// NaN-aware reductions.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::stats {

/// @file
/// @brief NaN-aware reductions.
///
/// **All-NaN slice handling** matches MATLAB:
/// - `nansum` → returns 0 (NaN is the additive identity)
/// - all others → return `NaN` (no defined value when nothing is observed)
///
/// For partial-NaN slices, `NaN` entries are skipped and the count of
/// valid observations becomes the divisor for `nanmean` / `nanvar` /
/// `nanstd`.

/// @brief NaN-aware sum along `dim` (`y = nansum(x, dim)`).
///
/// All-NaN slices return 0.
///
/// @param x    Input array.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Sum along `dim`, NaN entries skipped.
/// @see nanmean
Value nansum(const Value &x, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// @brief NaN-aware mean along `dim` (`y = nanmean(x, dim)`).
///
/// All-NaN slices return `NaN`.
///
/// @param x    Input array.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Mean along `dim`, NaN entries skipped.
/// @see nansum, nanvar
Value nanmean(const Value &x, int dim = 0,
              std::pmr::memory_resource *mr = nullptr);

/// @brief NaN-aware maximum along `dim` (`y = nanmax(x, dim)`).
///
/// @param x    Input array.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Maximum along `dim`, NaN entries skipped.
/// @see nanmin
Value nanmax(const Value &x, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// @brief NaN-aware minimum along `dim` (`y = nanmin(x, dim)`).
///
/// @param x    Input array.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Minimum along `dim`, NaN entries skipped.
/// @see nanmax
Value nanmin(const Value &x, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// @brief NaN-aware variance (`y = nanvar(x, normFlag, dim)`).
///
/// `normFlag` matches `var`'s semantics: 0 → divide by `N - 1` (where
/// `N` is the count of non-NaN observations in the slice), 1 → divide
/// by `N`.
///
/// @param x         Input array.
/// @param normFlag  Bias flag (0 = unbiased, 1 = population).
/// @param dim       1-based dimension; 0 → first non-singleton dim.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Variance along `dim`, NaN entries skipped.
/// @see nanstdev, nanmean
Value nanvar(const Value &x, int normFlag = 0, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// @brief NaN-aware standard deviation (`y = nanstdev(x, normFlag, dim)`).
///
/// Computed as `sqrt(nanvar(x, normFlag, dim))`.
///
/// @param x         Input array.
/// @param normFlag  Bias flag.
/// @param dim       1-based dimension; 0 → first non-singleton dim.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Standard deviation along `dim`, NaN entries skipped.
/// @see nanvar
Value nanstdev(const Value &x, int normFlag = 0, int dim = 0,
               std::pmr::memory_resource *mr = nullptr);

/// @brief NaN-aware median along `dim` (`y = nanmedian(x, dim)`).
///
/// @param x    Input array.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Median along `dim`, NaN entries skipped.
/// @see nanmean
Value nanmedian(const Value &x, int dim = 0,
                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
