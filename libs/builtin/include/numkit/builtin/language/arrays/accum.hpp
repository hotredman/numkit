// libs/builtin/include/numkit/builtin/language/arrays/accum.hpp
//
// accumarray — group-by reduction.

#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <cstddef>

namespace numkit::builtin {

/// @brief Built-in reducers recognised by @ref accumarray.
///
/// Custom function handles are rejected at the adapter layer with a
/// clear error — this enum covers the 99% MATLAB use case.
enum class AccumReducer {
    Sum,   ///< `@sum` (additive identity 0).
    Max,   ///< `@max`.
    Min,   ///< `@min`.
    Prod,  ///< `@prod` (multiplicative identity 1).
    Mean,  ///< `@mean`.
    Any,   ///< `@any` (logical OR).
    All    ///< `@all` (logical AND).
};

/// @brief Group-by reduction (`A = accumarray(subs, vals, sz, fn, fillVal)`).
///
/// Each row of `subs` is a 1-based multi-dim index into the output `A`.
/// `vals(i)` is contributed to `A(subs(i, :))` via the reducer `op`.
/// Output shape is `outShape`; pass empty span to auto-derive from
/// `max(subs)` per column. Cells with no contributions get `fillVal`.
///
/// @param subs      `N × D` matrix of 1-based subscripts.
/// @param vals      Length-`N` vector of contributions, or a scalar
///                  (broadcast to every row).
/// @param outShape  Output shape; empty span → derive from `max(subs)`.
/// @param op        Built-in reducer (see @ref AccumReducer).
/// @param fillVal   Default value for untouched cells (matches MATLAB's
///                  5-arg form, default 0).
/// @param mr        Memory resource (nullptr → process default).
/// @return          Accumulated output array of shape `outShape`
///                  (or auto-derived).
Value accumarray(const Value &subs, const Value &vals, Span<const size_t> outShape,
                 AccumReducer op, double fillVal,
                 std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
