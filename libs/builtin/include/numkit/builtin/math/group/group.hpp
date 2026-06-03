// libs/builtin/include/numkit/builtin/math/group/group.hpp
//
// Group-based operations: findgroups (assign group IDs) and groupcounts
// (count elements per group). The grouping-with-callback operations
// (splitapply / groupsummary / grouptransform / groupfilter) stay
// adapter-only — they require engine function-handle callbacks.

#pragma once

#include <memory_resource>
#include <string>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// @brief Result of `findgroups` — `[G, ID]`.
struct FindgroupsResult {
    Value G;   ///< Group index per element (shape of input; NaN = missing).
    Value ID;  ///< Column of unique non-NaN values (sorted ascending).
};

/// @brief Assign 1-based group IDs (`[G, ID] = findgroups(g)`).
///
/// `G[i]` is the group ID of `g[i]`, based on the sorted-unique order of the
/// finite values of `g`; `ID` is the column of those unique values. `NaN`
/// entries are treated as missing (`G[i] = NaN`, excluded from `ID`),
/// matching MATLAB R2025b. `G` takes the shape of `g`.
///
/// @param g   Grouping variable (numeric / logical / char via elemAsDouble).
/// @param mr  Memory resource (nullptr → process default).
/// @return    @ref FindgroupsResult `{ G, ID }`.
/// @see groupcounts
FindgroupsResult findgroups(const Value &g,
                            std::pmr::memory_resource *mr = nullptr);

/// @brief Result of `groupcounts` — `[C, GR, P]`.
struct GroupcountsResult {
    Value C;   ///< Count per group (trailing NaN bucket if `g` has any NaN).
    Value GR;  ///< Group representatives (sorted unique; NaN trailing).
    Value P;   ///< Percentage `100 * count / total` per group.
};

/// @brief Count elements per group (`[C, GR, P] = groupcounts(g)`).
///
/// `C` is the per-group element count over the sorted-unique values of `g`;
/// `GR` the matching representatives; `P` the percentages. `NaN` entries
/// form a single trailing bucket (matching MATLAB R2025b).
///
/// @param g   Grouping variable.
/// @param mr  Memory resource (nullptr → process default).
/// @return    @ref GroupcountsResult `{ C, GR, P }`.
/// @see findgroups
GroupcountsResult groupcounts(const Value &g,
                              std::pmr::memory_resource *mr = nullptr);

/// @brief Result of the array form of `groupsummary` — `[B, BG, BC]`.
struct GroupsummaryResult {
    Value B;   ///< `nGroups × cols(A)` per-group, per-column reduction.
    Value BG;  ///< Group representatives (sorted unique; NaN trailing).
    Value BC;  ///< Element count per group.
};

/// @brief Per-group reduction (`[B, BG, BC] = groupsummary(A, G, method)`).
///
/// Groups the rows of `A` (length-`size(A,1)` grouping vector `G`) and
/// applies `method` per group, per column. Supported `method` strings:
/// `"sum"`, `"mean"`, `"median"`, `"max"`, `"min"`, `"std"`, `"var"`,
/// `"numunique"`, `"nnz"`, `"mode"`, `"all"`, `"any"`. `NaN` group values
/// form a single trailing bucket. (Table inputs, `groupbins`, multi-group
/// vars and function-handle methods are deferred — they need the table type
/// / engine plumbing not available here.)
///
/// @param A       Column vector or matrix (DOUBLE).
/// @param G       Grouping vector, length `size(A,1)`.
/// @param method  Reduction method string (see list above).
/// @param mr      Memory resource (nullptr → process default).
/// @return        @ref GroupsummaryResult `{ B, BG, BC }`.
/// @throws Error on `G` length mismatch or an unsupported `method`.
/// @see findgroups, groupcounts
GroupsummaryResult groupsummary(const Value &A, const Value &G,
                                const std::string &method,
                                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
