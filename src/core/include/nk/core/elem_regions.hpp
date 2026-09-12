/// @file elem_regions.hpp
/// @ingroup group_core
///
/// Element-access regions for index-aware dependency tracking
/// (Play portion B1; drives the reactive table editor where editing
/// `C{5,2}` must recompute only the formulas reading that cell).
///
/// A REGION is an axis-aligned rectangle in (row, col) space over a
/// container variable, or ALL (the whole variable). Extraction rules
/// (conservative-correct, never wrong-fast):
///   `C{5,2}`      → elem (5,2)
///   `C(1:5, 3)`   → rows 1..5 × col 3      (literal bounds)
///   `C(1:2:9, 3)` → rows 1..9 × col 3      (stepped range → COVERING
///                                          contiguous range: over-dirties,
///                                          never under-dirties)
///   `C(:, 2)`     → col 2 (full rows)
///   `C(i, 1)`, `C(end,1)`, `C(7)` (linear), logical masks
///                → ALL (index not statically literal)
///   `size(C)` / `numel(C)` / `length(C)` / `ndims(C)`
///                → ALL (a merge-write can GROW the variable, changing
///                  size/numel/end semantics)
///
/// Index-aware tracking applies to BOTH INDEX (`A(..)`) and CELL_INDEX
/// (`C{..}`) nodes — one machinery, two consumers: matrices ride along
/// for free (rarely element-edited in Play scripts), cell sheets are
/// the primary beneficiary.

#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace nk {

/// One rectangle, inclusive 1-based bounds. Empty (r1 < r0) = nothing.
struct ElemRegion
{
    size_t r0 = 1, r1 = 0, c0 = 1, c1 = 0;

    bool empty() const { return r1 < r0 || c1 < c0; }
    bool intersects(const ElemRegion &o) const
    {
        return !empty() && !o.empty() && r0 <= o.r1 && o.r0 <= r1
               && c0 <= o.c1 && o.c0 <= c1;
    }
};

/// The set of regions accessed on ONE variable by one statement.
/// `all` (whole-variable access seen) dominates any rectangles.
struct VarRegions
{
    bool all = false;
    std::vector<ElemRegion> rects;

    void setAll() { all = true; }
    void add(const ElemRegion &r)
    {
        if (!r.empty())
            rects.push_back(r);
    }
    /// True when this access set could touch `r` (or a superset of it).
    bool touches(const ElemRegion &r) const
    {
        if (all)
            return true;
        for (const auto &x : rects)
            if (x.intersects(r))
                return true;
        return false;
    }
    bool touchesAll() const { return all || !rects.empty(); }
};

/// Per-statement fine-grained access map: variable → regions.
/// Empty map entry with all=false and no rects = no access.
using ElemAccessMap = std::map<std::string, VarRegions>;

}  // namespace nk
