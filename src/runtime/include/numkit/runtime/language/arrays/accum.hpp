// src/runtime/include/numkit/runtime/language/arrays/accum.hpp
//
// accumarray — group-by reduction.

#pragma once

#include <memory_resource>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <cstddef>

namespace numkit {
class Engine;
}

namespace numkit::runtime {

void registerArraysRuntime(Engine &engine);

/// @brief Built-in reducers recognised by @ref accumarray.
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
/// @param subs      `N x D` matrix of 1-based subscripts.
/// @param vals      Length-`N` vector of contributions, or a scalar (broadcast).
/// @param outShape  Output shape; empty span -> derive from `max(subs)`.
/// @param op        Built-in reducer (see @ref AccumReducer).
/// @param fillVal   Default value for untouched cells (default 0).
/// @param mr        Memory resource (nullptr -> process default).
/// @return          Accumulated output array of shape `outShape`.
Value accumarray(const Value &subs, const Value &vals, Span<const size_t> outShape,
                 AccumReducer op, double fillVal,
                 std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::runtime
