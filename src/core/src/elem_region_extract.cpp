#include <nk/core/ast.hpp>
#include <nk/core/elem_regions.hpp>

#include <algorithm>
#include <cmath>

namespace nk {
namespace depdetail {

// ── Literal subscript → axis range (tri-state) ──────────────────────
//   FULL       — plain ':' (covers the whole axis)
//   RANGE      — literal bounds; stepped 'a:s:b' covers [a,b]
//                CONTIGUOUSLY (over-dirty, never under-dirty)
//   UNRESOLVED — identifier / call / `end` / arithmetic / logical
//                mask: the caller must collapse the access to ALL.

struct AxisRange
{
    enum Kind { FULL, RANGE, UNRESOLVED } kind = FULL;
    long long lo = 0, hi = 0;
};

static bool literalInt(const ASTNode *n, long long &out)
{
    if (!n || n->type != NodeType::NUMBER_LITERAL
        || std::floor(n->numValue) != n->numValue)
        return false;
    out = (long long)n->numValue;
    return true;
}

AxisRange axisOf(const ASTNode *n)
{
    AxisRange a;
    if (!n)
        return AxisRange{AxisRange::UNRESOLVED, 0, 0};
    if (n->type == NodeType::COLON_EXPR) {
        const size_t nv = n->children.size();
        if (nv == 0)
            return AxisRange{AxisRange::FULL, 0, 0};       // ':'
        if (nv > 3)
            return AxisRange{AxisRange::UNRESOLVED, 0, 0};
        long long v[3] = {0, 0, 0};
        for (size_t k = 0; k < nv; ++k)
            if (!literalInt(n->children[k].get(), v[k]))
                return AxisRange{AxisRange::UNRESOLVED, 0, 0};
        long long lo = v[0], hi = (nv == 2) ? v[1] : v[2];
        if (lo > hi)
            std::swap(lo, hi);
        return AxisRange{AxisRange::RANGE, lo, hi};
    }
    long long x = 0;
    if (literalInt(n, x))
        return AxisRange{AxisRange::RANGE, x, x};
    return AxisRange{AxisRange::UNRESOLVED, 0, 0};
}

/// Record the element regions of ONE container access (`n` is the
/// INDEX / CELL_INDEX node, `base` the container variable name).
void recordAccess(ElemAccessMap &m, const std::string &base,
                  const ASTNode *n)
{
    VarRegions &vr = m[base];
    // Subscripts are children[1..]; children[0] is the base. Exactly 2
    // subscripts → rect; anything else (0, 1 = linear, 3+ = N-D) → ALL
    // (v1 tracks 2-D sheets/matrices only; linear A(7) depends on the
    // runtime shape, unknowable statically).
    if (n->children.size() != 3) {
        vr.setAll();
        return;
    }
    AxisRange rows = axisOf(n->children[1].get());
    AxisRange cols = axisOf(n->children[2].get());
    if (rows.kind == AxisRange::UNRESOLVED
        || cols.kind == AxisRange::UNRESOLVED)
        { vr.setAll(); return; }
    if (rows.kind == AxisRange::FULL && cols.kind == AxisRange::FULL)
        { vr.setAll(); return; }              // C(:, :) — whole var
    ElemRegion r;
    r.r0 = rows.kind == AxisRange::RANGE ? (size_t)rows.lo : (size_t)1;
    r.r1 = rows.kind == AxisRange::RANGE ? (size_t)rows.hi
                                         : (size_t)-1;
    r.c0 = cols.kind == AxisRange::RANGE ? (size_t)cols.lo : (size_t)1;
    r.c1 = cols.kind == AxisRange::RANGE ? (size_t)cols.hi
                                         : (size_t)-1;
    if ((rows.kind == AxisRange::RANGE && rows.lo < 1)
        || (cols.kind == AxisRange::RANGE && cols.lo < 1))
        { vr.setAll(); return; }              // 0/negative index — ALL
    vr.add(r);
}

/// size(A)/numel(A)/length(A)/ndims(A): a merge-write can GROW the
/// variable (changing size/numel/end semantics) — these read ALL.
bool isSizeCallee(const std::string &callee)
{
    return callee == "size" || callee == "numel" || callee == "length"
           || callee == "ndims";
}

}  // namespace depdetail
}  // namespace nk
