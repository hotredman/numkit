// libs/builtin/src/MStdSetOps.cpp
//
// Phase-8: search and set operations. Inputs flatten to a vector
// (column-major) before processing; outputs are 1×N row vectors of
// sorted unique values for the set ops, ismember preserves the
// shape of A, histcounts/discretize work on the flat input.

#include <numkit/m/builtin/MStdSetOps.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

namespace numkit::m::builtin {

namespace {

// Stable sorted-with-original-indices buffer. Used by uniqueWithIndices
// to recover ia (= original index of first occurrence per unique val)
// and ic (= unique-output position of each original element).
struct IndexedVal {
    double v;
    size_t origIdx;
};

// Standard MATLAB sort treats NaN as larger than any number. We follow
// that convention — NaN values land at the end of the sorted output.
inline bool lessForSet(double a, double b)
{
    if (std::isnan(a)) return false;
    if (std::isnan(b)) return true;
    return a < b;
}

// MATLAB convention: equal up to bit identity. NaN != NaN here too,
// so each NaN is its own unique value.
inline bool equalForSet(double a, double b)
{
    if (std::isnan(a) || std::isnan(b)) return false;
    return a == b;
}

// Empty result (1×0 row) — MATLAB returns this for "no elements".
inline MValue emptyRow(Allocator &alloc)
{
    return MValue::matrix(1, 0, MType::DOUBLE, &alloc);
}

inline MValue rowFromVec(Allocator &alloc, const std::vector<double> &v)
{
    auto r = MValue::matrix(1, v.size(), MType::DOUBLE, &alloc);
    if (!v.empty())
        std::copy(v.begin(), v.end(), r.doubleDataMut());
    return r;
}

} // namespace

// ────────────────────────────────────────────────────────────────────
// unique
// ────────────────────────────────────────────────────────────────────

MValue unique(Allocator &alloc, const MValue &x)
{
    const size_t n = x.numel();
    if (n == 0) return emptyRow(alloc);

    std::vector<double> buf(n);
    std::copy(x.doubleData(), x.doubleData() + n, buf.data());
    std::sort(buf.begin(), buf.end(), lessForSet);

    std::vector<double> out;
    out.reserve(n);
    out.push_back(buf[0]);
    for (size_t i = 1; i < n; ++i) {
        // Append if this value is distinct from the last one written.
        // For NaNs we always append (NaN != NaN by equalForSet).
        if (!equalForSet(buf[i], out.back()))
            out.push_back(buf[i]);
    }
    return rowFromVec(alloc, out);
}

std::tuple<MValue, MValue, MValue>
uniqueWithIndices(Allocator &alloc, const MValue &x)
{
    const size_t n = x.numel();
    if (n == 0) {
        return std::make_tuple(emptyRow(alloc), emptyRow(alloc),
                               emptyRow(alloc));
    }

    // Sort with original-index tracking to recover ia / ic.
    std::vector<IndexedVal> sorted(n);
    for (size_t i = 0; i < n; ++i)
        sorted[i] = {x.doubleData()[i], i};
    std::sort(sorted.begin(), sorted.end(),
              [](const IndexedVal &a, const IndexedVal &b) {
                  return lessForSet(a.v, b.v);
              });

    std::vector<double> uniq;
    std::vector<size_t> ia;            // original idx of first occurrence
    std::vector<size_t> rankInUniq(n); // for each sorted element, its slot in uniq
    uniq.reserve(n);
    ia.reserve(n);

    uniq.push_back(sorted[0].v);
    ia.push_back(sorted[0].origIdx);
    rankInUniq[0] = 0;
    for (size_t i = 1; i < n; ++i) {
        if (!equalForSet(sorted[i].v, uniq.back())) {
            uniq.push_back(sorted[i].v);
            ia.push_back(sorted[i].origIdx);
        }
        rankInUniq[i] = uniq.size() - 1;
    }

    // ic[origIdx] = rankInUniq for that original element. We have
    // sorted[i].origIdx and rankInUniq[i]; invert.
    std::vector<double> ic(n);
    for (size_t i = 0; i < n; ++i)
        ic[sorted[i].origIdx] = static_cast<double>(rankInUniq[i] + 1);

    auto cOut = rowFromVec(alloc, uniq);
    auto iaRow = MValue::matrix(1, ia.size(), MType::DOUBLE, &alloc);
    for (size_t i = 0; i < ia.size(); ++i)
        iaRow.doubleDataMut()[i] = static_cast<double>(ia[i] + 1);
    auto icRow = MValue::matrix(1, n, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < n; ++i)
        icRow.doubleDataMut()[i] = ic[i];

    return std::make_tuple(std::move(cOut), std::move(iaRow), std::move(icRow));
}

// ────────────────────────────────────────────────────────────────────
// ismember
// ────────────────────────────────────────────────────────────────────
//
// Sort B once, then binary-search each element of A. Output shape
// matches A; logical type. NaN in A or B never matches.
MValue ismember(Allocator &alloc, const MValue &a, const MValue &b)
{
    const size_t na = a.numel();
    const size_t nb = b.numel();

    // Output shape preserved (use the typed createLike helper).
    auto r = createLike(a, MType::LOGICAL, &alloc);
    if (na == 0) return r;
    if (nb == 0) {
        std::fill(r.logicalDataMut(), r.logicalDataMut() + na, 0);
        return r;
    }

    // Build sorted, NaN-stripped copy of B.
    std::vector<double> sortedB;
    sortedB.reserve(nb);
    for (size_t i = 0; i < nb; ++i) {
        const double v = b.doubleData()[i];
        if (!std::isnan(v)) sortedB.push_back(v);
    }
    std::sort(sortedB.begin(), sortedB.end());

    for (size_t i = 0; i < na; ++i) {
        const double v = a.doubleData()[i];
        bool present = false;
        if (!std::isnan(v) && !sortedB.empty()) {
            present = std::binary_search(sortedB.begin(), sortedB.end(), v);
        }
        r.logicalDataMut()[i] = present ? 1 : 0;
    }
    return r;
}

// ────────────────────────────────────────────────────────────────────
// union / intersect / setdiff
// ────────────────────────────────────────────────────────────────────
//
// Each takes the unique-then-sorted form of both inputs, then walks
// the two sorted vectors with two pointers. Output is a row vector.

namespace {

std::vector<double> sortedUnique(const MValue &x)
{
    std::vector<double> v;
    v.reserve(x.numel());
    for (size_t i = 0; i < x.numel(); ++i) {
        const double d = x.doubleData()[i];
        if (!std::isnan(d)) v.push_back(d);
    }
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

} // namespace

MValue setUnion(Allocator &alloc, const MValue &a, const MValue &b)
{
    auto sa = sortedUnique(a);
    auto sb = sortedUnique(b);
    std::vector<double> out;
    out.reserve(sa.size() + sb.size());
    std::set_union(sa.begin(), sa.end(),
                   sb.begin(), sb.end(),
                   std::back_inserter(out));
    return rowFromVec(alloc, out);
}

MValue setIntersect(Allocator &alloc, const MValue &a, const MValue &b)
{
    auto sa = sortedUnique(a);
    auto sb = sortedUnique(b);
    std::vector<double> out;
    out.reserve(std::min(sa.size(), sb.size()));
    std::set_intersection(sa.begin(), sa.end(),
                          sb.begin(), sb.end(),
                          std::back_inserter(out));
    return rowFromVec(alloc, out);
}

MValue setDiff(Allocator &alloc, const MValue &a, const MValue &b)
{
    auto sa = sortedUnique(a);
    auto sb = sortedUnique(b);
    std::vector<double> out;
    out.reserve(sa.size());
    std::set_difference(sa.begin(), sa.end(),
                        sb.begin(), sb.end(),
                        std::back_inserter(out));
    return rowFromVec(alloc, out);
}

// ────────────────────────────────────────────────────────────────────
// histcounts / discretize
// ────────────────────────────────────────────────────────────────────
//
// Edges: a length-(N+1) vector defining N bins. Bin i covers
// [edges(i), edges(i+1)) except bin N which is closed [edges(N), edges(N+1)].
// Edges must be ascending.

namespace {

void validateEdges(const MValue &edges, const char *fn)
{
    if (edges.numel() < 2)
        throw MError(std::string(fn) + ": edges must have length >= 2",
                     0, 0, fn, "", std::string("m:") + fn + ":shortEdges");
    const double *e = edges.doubleData();
    for (size_t i = 1; i < edges.numel(); ++i)
        if (!(e[i] >= e[i - 1]))
            throw MError(std::string(fn) + ": edges must be ascending",
                         0, 0, fn, "", std::string("m:") + fn + ":badEdges");
}

// Find the 0-based bin index for value v given sorted edges.
// Returns SIZE_MAX if v is out of range or NaN.
size_t findBin(const double *edges, size_t nEdges, double v)
{
    if (std::isnan(v)) return SIZE_MAX;
    const size_t nBins = nEdges - 1;
    const double last = edges[nEdges - 1];
    if (v < edges[0]) return SIZE_MAX;
    // Last bin is closed at the right.
    if (v == last) return nBins - 1;
    if (v > last) return SIZE_MAX;
    // upper_bound finds first edge > v; bin index = pos - 1.
    auto it = std::upper_bound(edges, edges + nEdges, v);
    return static_cast<size_t>(it - edges) - 1;
}

} // namespace

MValue histcounts(Allocator &alloc, const MValue &x, const MValue &edges)
{
    validateEdges(edges, "histcounts");
    const size_t nBins = edges.numel() - 1;
    auto r = MValue::matrix(1, nBins, MType::DOUBLE, &alloc);
    double *dst = r.doubleDataMut();
    std::fill(dst, dst + nBins, 0.0);

    const double *e = edges.doubleData();
    for (size_t i = 0; i < x.numel(); ++i) {
        const size_t bin = findBin(e, edges.numel(), x.doubleData()[i]);
        if (bin != SIZE_MAX) dst[bin] += 1.0;
    }
    return r;
}

MValue discretize(Allocator &alloc, const MValue &x, const MValue &edges)
{
    validateEdges(edges, "discretize");
    auto r = createLike(x, MType::DOUBLE, &alloc);
    const double *e = edges.doubleData();
    for (size_t i = 0; i < x.numel(); ++i) {
        const size_t bin = findBin(e, edges.numel(), x.doubleData()[i]);
        r.doubleDataMut()[i] = (bin == SIZE_MAX)
            ? std::nan("")
            : static_cast<double>(bin + 1);  // 1-based
    }
    return r;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════
namespace detail {

void unique_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw MError("unique: requires 1 argument",
                     0, 0, "unique", "", "m:unique:nargin");
    auto &alloc = ctx.engine->allocator();
    if (nargout <= 1) {
        outs[0] = unique(alloc, args[0]);
        return;
    }
    auto [c, ia, ic] = uniqueWithIndices(alloc, args[0]);
    outs[0] = std::move(c);
    if (nargout > 1) outs[1] = std::move(ia);
    if (nargout > 2) outs[2] = std::move(ic);
}

#define NK_BIN_SETOP_REG(name, fn)                                             \
    void name##_reg(Span<const MValue> args, size_t /*nargout*/,               \
                    Span<MValue> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.size() < 2)                                                   \
            throw MError(#name ": requires 2 arguments",                       \
                         0, 0, #name, "", "m:" #name ":nargin");               \
        outs[0] = fn(ctx.engine->allocator(), args[0], args[1]);               \
    }

NK_BIN_SETOP_REG(ismember,  ismember)
NK_BIN_SETOP_REG(union,     setUnion)
NK_BIN_SETOP_REG(intersect, setIntersect)
NK_BIN_SETOP_REG(setdiff,   setDiff)
NK_BIN_SETOP_REG(histcounts, histcounts)
NK_BIN_SETOP_REG(discretize, discretize)

#undef NK_BIN_SETOP_REG

} // namespace detail

} // namespace numkit::m::builtin
