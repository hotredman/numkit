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
#include <cstdint>
#include <cstring>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace numkit::m::builtin {

namespace {

// Phase P3 — hash-based set ops. The previous sort-based path was
// O(N log N) on every call; for the bench input (1M doubles drawn from
// a small integer range, K ≈ 8000 unique) it spent ~95% of its time
// sorting duplicates. Hash dedupe drops that to O(N) + O(K log K).
//
// MATLAB semantics that the kernel must preserve:
//   * +0 and -0 are equal — collapse to one slot in unique / lookup.
//   * NaN never equals anything (including itself). For unique each NaN
//     is its own slot; for ismember NaN never matches; for
//     union/intersect/setdiff NaN is dropped entirely (matches the
//     previous sortedUnique helper's behaviour).
//   * Output of unique / union / intersect / setdiff is sorted ascending,
//     NaN entries (if any) appended last (matches MATLAB sort convention).

// Hash that normalises -0 → +0 so the two share a bucket. std::hash<double>
// on most STL implementations hashes the bit pattern, which puts +0 and
// -0 in different buckets and breaks lookup-equality (since lookup uses
// the hash first, then operator==).
struct DoubleHashEq0 {
    size_t operator()(double v) const noexcept {
        if (v == 0.0) return 0;          // covers both +0 and -0
        std::uint64_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        // xorshift mix — std::hash<uint64_t> is identity on libstdc++,
        // which clusters double bit patterns badly (sign + exponent dominate).
        bits ^= bits >> 33;
        bits *= 0xff51afd7ed558ccdULL;
        bits ^= bits >> 33;
        bits *= 0xc4ceb9fe1a85ec53ULL;
        bits ^= bits >> 33;
        return static_cast<size_t>(bits);
    }
};

// Stable sorted-with-original-indices buffer. Used by uniqueWithIndices
// to recover ia (= original index of first occurrence per unique val)
// and ic (= unique-output position of each original element).
struct IndexedVal {
    double v;
    size_t origIdx;
};

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

    std::unordered_set<double, DoubleHashEq0> seen;
    seen.reserve(n / 2 + 1);
    size_t nanCount = 0;
    const double *p = x.doubleData();
    for (size_t i = 0; i < n; ++i) {
        if (std::isnan(p[i])) ++nanCount;
        else seen.insert(p[i]);
    }

    std::vector<double> out;
    out.reserve(seen.size() + nanCount);
    out.assign(seen.begin(), seen.end());
    std::sort(out.begin(), out.end());
    // Each NaN in the input stays as its own unique entry (matches
    // MATLAB and the previous sort-then-dedupe behaviour).
    for (size_t i = 0; i < nanCount; ++i)
        out.push_back(std::nan(""));
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

    // Pass 1: hash-track first-occurrence index for non-NaN values, plus
    // ordered list of NaN-occurrence indices (NaN can't go in a hash —
    // each one is its own unique slot in the output).
    std::unordered_map<double, size_t, DoubleHashEq0> firstIdx;
    firstIdx.reserve(n / 2 + 1);
    std::vector<size_t> nanIdxOrder;
    const double *p = x.doubleData();
    for (size_t i = 0; i < n; ++i) {
        if (std::isnan(p[i])) {
            nanIdxOrder.push_back(i);
        } else {
            firstIdx.try_emplace(p[i], i);
        }
    }

    // Sort the non-NaN unique entries (value, first-occurrence-index)
    // by value, then append NaN entries in input order at the end.
    std::vector<IndexedVal> sorted;
    sorted.reserve(firstIdx.size() + nanIdxOrder.size());
    for (const auto &kv : firstIdx)
        sorted.push_back({kv.first, kv.second});
    std::sort(sorted.begin(), sorted.end(),
              [](const IndexedVal &a, const IndexedVal &b) {
                  return a.v < b.v;
              });
    for (size_t idx : nanIdxOrder)
        sorted.push_back({std::nan(""), idx});

    // For ic we need: input-index → rank in `sorted`. Non-NaN values
    // get the rank from a value→rank map; NaN entries get their own
    // slot in the order they appeared in the input.
    std::unordered_map<double, size_t, DoubleHashEq0> rankByValue;
    rankByValue.reserve(firstIdx.size());
    const size_t nanRankBase = sorted.size() - nanIdxOrder.size();
    for (size_t r = 0; r < nanRankBase; ++r)
        rankByValue[sorted[r].v] = r;

    std::vector<double> ic(n);
    size_t nanSeen = 0;
    for (size_t i = 0; i < n; ++i) {
        if (std::isnan(p[i])) {
            ic[i] = static_cast<double>(nanRankBase + nanSeen + 1);
            ++nanSeen;
        } else {
            ic[i] = static_cast<double>(rankByValue[p[i]] + 1);
        }
    }

    // Build outputs.
    auto cOut = MValue::matrix(1, sorted.size(), MType::DOUBLE, &alloc);
    auto iaRow = MValue::matrix(1, sorted.size(), MType::DOUBLE, &alloc);
    for (size_t i = 0; i < sorted.size(); ++i) {
        cOut.doubleDataMut()[i]  = sorted[i].v;
        iaRow.doubleDataMut()[i] = static_cast<double>(sorted[i].origIdx + 1);
    }
    auto icRow = MValue::matrix(1, n, MType::DOUBLE, &alloc);
    std::copy(ic.begin(), ic.end(), icRow.doubleDataMut());

    return std::make_tuple(std::move(cOut), std::move(iaRow), std::move(icRow));
}

// ────────────────────────────────────────────────────────────────────
// ismember
// ────────────────────────────────────────────────────────────────────
//
// Hash B once, then O(1) lookup per element of A. Output shape matches
// A; logical type. NaN in A or B never matches.
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

    std::unordered_set<double, DoubleHashEq0> setB;
    setB.reserve(nb);
    const double *pb = b.doubleData();
    for (size_t i = 0; i < nb; ++i)
        if (!std::isnan(pb[i])) setB.insert(pb[i]);

    const double *pa = a.doubleData();
    uint8_t *out = r.logicalDataMut();
    for (size_t i = 0; i < na; ++i) {
        const double v = pa[i];
        out[i] = (!std::isnan(v) && setB.count(v) != 0) ? 1 : 0;
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

// Build the dedupe'd non-NaN set from x. NaN entries are dropped per
// MATLAB convention for union/intersect/setdiff.
std::unordered_set<double, DoubleHashEq0> hashSetNoNaN(const MValue &x)
{
    std::unordered_set<double, DoubleHashEq0> s;
    s.reserve(x.numel() / 2 + 1);
    const double *p = x.doubleData();
    const size_t n = x.numel();
    for (size_t i = 0; i < n; ++i)
        if (!std::isnan(p[i])) s.insert(p[i]);
    return s;
}

} // namespace

MValue setUnion(Allocator &alloc, const MValue &a, const MValue &b)
{
    auto s = hashSetNoNaN(a);
    const double *pb = b.doubleData();
    for (size_t i = 0; i < b.numel(); ++i)
        if (!std::isnan(pb[i])) s.insert(pb[i]);
    std::vector<double> out(s.begin(), s.end());
    std::sort(out.begin(), out.end());
    return rowFromVec(alloc, out);
}

MValue setIntersect(Allocator &alloc, const MValue &a, const MValue &b)
{
    // Hash the smaller side, walk the larger — minimises hash misses
    // and keeps the dedupe set bounded by min(|A|, |B|).
    const bool aSmaller = a.numel() <= b.numel();
    const MValue &small = aSmaller ? a : b;
    const MValue &large = aSmaller ? b : a;

    auto smallSet = hashSetNoNaN(small);
    std::unordered_set<double, DoubleHashEq0> seenInLarge;
    seenInLarge.reserve(smallSet.size());
    std::vector<double> out;
    out.reserve(smallSet.size());

    const double *pl = large.doubleData();
    for (size_t i = 0; i < large.numel(); ++i) {
        const double v = pl[i];
        if (std::isnan(v)) continue;
        if (smallSet.count(v) && seenInLarge.insert(v).second)
            out.push_back(v);
    }
    std::sort(out.begin(), out.end());
    return rowFromVec(alloc, out);
}

MValue setDiff(Allocator &alloc, const MValue &a, const MValue &b)
{
    auto setB = hashSetNoNaN(b);
    std::unordered_set<double, DoubleHashEq0> seen;
    seen.reserve(a.numel() / 2 + 1);
    std::vector<double> out;
    out.reserve(a.numel());
    const double *pa = a.doubleData();
    for (size_t i = 0; i < a.numel(); ++i) {
        const double v = pa[i];
        if (std::isnan(v)) continue;
        if (setB.count(v) == 0 && seen.insert(v).second)
            out.push_back(v);
    }
    std::sort(out.begin(), out.end());
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

// Phase P4: detect uniform edges so the per-element cost drops from
// O(log B) std::upper_bound to O(1) `(v - e0) * invStep`. Walks the
// edges once (O(B)) — cheap relative to the N >> B input. Returns true
// and sets `outStep` to e[1]-e[0] when uniform.
bool edgesAreUniform(const double *e, size_t nEdges, double &outStep)
{
    if (nEdges < 3) return false;
    const double step = e[1] - e[0];
    if (!(step > 0)) return false;
    // Allow drift of a few ULPs per gap. For the bench-representative
    // input (-3000:300:6000) the tolerance never trips because the edges
    // are exactly representable integers.
    const double tol = std::abs(step) * 1e-12;
    for (size_t i = 2; i < nEdges; ++i) {
        const double g = e[i] - e[i - 1];
        if (std::abs(g - step) > tol) return false;
    }
    outStep = step;
    return true;
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
    const double *p = x.doubleData();
    const size_t n = x.numel();

    double step;
    if (edgesAreUniform(e, edges.numel(), step)) {
        const double e0 = e[0];
        const double eN = e[nBins];
        const double invStep = 1.0 / step;
        for (size_t i = 0; i < n; ++i) {
            const double v = p[i];
            // NaN compares false on both sides → treated as out-of-range.
            if (v >= e0 && v <= eN) {
                size_t bin;
                if (v == eN) {
                    bin = nBins - 1;          // last bin is right-closed
                } else {
                    bin = static_cast<size_t>((v - e0) * invStep);
                    if (bin >= nBins) bin = nBins - 1;  // FP rounding guard
                }
                dst[bin] += 1.0;
            }
        }
        return r;
    }

    for (size_t i = 0; i < n; ++i) {
        const size_t bin = findBin(e, edges.numel(), p[i]);
        if (bin != SIZE_MAX) dst[bin] += 1.0;
    }
    return r;
}

MValue discretize(Allocator &alloc, const MValue &x, const MValue &edges)
{
    validateEdges(edges, "discretize");
    auto r = createLike(x, MType::DOUBLE, &alloc);
    const double *e = edges.doubleData();
    const double *p = x.doubleData();
    double *dst = r.doubleDataMut();
    const size_t n = x.numel();
    const size_t nBins = edges.numel() - 1;

    double step;
    if (edgesAreUniform(e, edges.numel(), step)) {
        const double e0 = e[0];
        const double eN = e[nBins];
        const double invStep = 1.0 / step;
        for (size_t i = 0; i < n; ++i) {
            const double v = p[i];
            if (v >= e0 && v <= eN) {
                size_t bin;
                if (v == eN) {
                    bin = nBins - 1;
                } else {
                    bin = static_cast<size_t>((v - e0) * invStep);
                    if (bin >= nBins) bin = nBins - 1;
                }
                dst[i] = static_cast<double>(bin + 1); // 1-based
            } else {
                dst[i] = std::nan("");
            }
        }
        return r;
    }

    for (size_t i = 0; i < n; ++i) {
        const size_t bin = findBin(e, edges.numel(), p[i]);
        dst[i] = (bin == SIZE_MAX) ? std::nan("")
                                   : static_cast<double>(bin + 1);
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
