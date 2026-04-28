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

// ── 'rows' helpers ─────────────────────────────────────────────
//
// Row signature for hashing: an N-double vector with ±0 normalised
// to +0 (so [-0 1] and [0 1] compare equal, matching MATLAB).

using RowKey = std::vector<double>;

struct RowKeyHash {
    size_t operator()(const RowKey &k) const noexcept {
        // Mix all lanes via the same xorshift mixer used for scalar
        // double hashing — keeps the per-element distribution good.
        std::uint64_t h = 0xcbf29ce484222325ULL;  // FNV-ish seed
        for (double v : k) {
            std::uint64_t bits;
            if (v == 0.0) bits = 0;  // collapse +0 / -0
            else          std::memcpy(&bits, &v, sizeof(bits));
            bits ^= bits >> 33;
            bits *= 0xff51afd7ed558ccdULL;
            bits ^= bits >> 33;
            h ^= bits + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        }
        return static_cast<size_t>(h);
    }
};

struct RowKeyEq {
    bool operator()(const RowKey &a, const RowKey &b) const noexcept {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            // Treat ±0 as equal but NaN as never-equal (this
            // path is only hit for NaN-free rows — caller filters).
            if (a[i] != b[i]) return false;
        }
        return true;
    }
};

inline bool rowHasNan(const double *p, size_t cols, size_t rows, size_t r)
{
    for (size_t c = 0; c < cols; ++c)
        if (std::isnan(p[c * rows + r])) return true;
    return false;
}

inline RowKey extractRow(const double *p, size_t cols, size_t rows, size_t r)
{
    RowKey k(cols);
    for (size_t c = 0; c < cols; ++c) {
        const double v = p[c * rows + r];
        // Normalise ±0 to +0 in the key so the hash equality also matches.
        k[c] = (v == 0.0) ? 0.0 : v;
    }
    return k;
}

// Lex-compare two rows (column-major underlying buffer).
inline int rowLexCmp(const double *p, size_t cols, size_t rows, size_t a, size_t b)
{
    for (size_t c = 0; c < cols; ++c) {
        const double av = p[c * rows + a];
        const double bv = p[c * rows + b];
        if (av < bv) return -1;
        if (av > bv) return  1;
    }
    return 0;
}

inline MValue emptyRowsResult(Allocator &alloc, size_t cols)
{
    // 0×C matrix — preserves column count so callers can still
    // introspect the original row width.
    return MValue::matrix(0, cols, MType::DOUBLE, &alloc);
}

inline MValue collectRowsByIndex(Allocator &alloc, const MValue &x,
                                 const std::vector<size_t> &origRows)
{
    const size_t rows = x.dims().rows();
    const size_t cols = x.dims().cols();
    const size_t outRows = origRows.size();
    auto r = MValue::matrix(outRows, cols, MType::DOUBLE, &alloc);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    for (size_t newRow = 0; newRow < outRows; ++newRow) {
        const size_t srcRow = origRows[newRow];
        for (size_t c = 0; c < cols; ++c)
            dst[c * outRows + newRow] = src[c * rows + srcRow];
    }
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
// unique with 'rows' flag
// ────────────────────────────────────────────────────────────────────
//
// Treats each row of x as a single key. Matches MATLAB:
//   * lex-sorted output for non-NaN rows
//   * NaN rows kept distinct (each its own slot, appended after the
//     sorted rows in input order — same convention as scalar unique)
//   * ±0 normalised to +0 in the row signature
// Inputs are validated as 2D — 3D+ throws (matches MATLAB).

namespace {

void validateUniqueRowsInput(const MValue &x, const char *fn)
{
    if (x.type() != MType::DOUBLE)
        throw MError(std::string(fn) + ": 'rows' flag requires a DOUBLE matrix",
                     0, 0, fn, "", std::string("m:") + fn + ":rowsType");
    if (x.dims().ndim() > 2)
        throw MError(std::string(fn) + ": 'rows' flag requires a 2D matrix",
                     0, 0, fn, "", std::string("m:") + fn + ":rowsND");
}

} // namespace

MValue uniqueRows(Allocator &alloc, const MValue &x)
{
    validateUniqueRowsInput(x, "unique");
    const size_t rows = x.dims().rows();
    const size_t cols = x.dims().cols();
    if (rows == 0) return emptyRowsResult(alloc, cols);

    const double *src = x.doubleData();
    std::unordered_map<RowKey, size_t, RowKeyHash, RowKeyEq> firstIdx;
    firstIdx.reserve(rows);
    std::vector<size_t> nanRows;
    for (size_t r = 0; r < rows; ++r) {
        if (rowHasNan(src, cols, rows, r)) {
            nanRows.push_back(r);
        } else {
            firstIdx.try_emplace(extractRow(src, cols, rows, r), r);
        }
    }

    std::vector<size_t> uniqRows;
    uniqRows.reserve(firstIdx.size() + nanRows.size());
    for (const auto &kv : firstIdx) uniqRows.push_back(kv.second);
    std::sort(uniqRows.begin(), uniqRows.end(),
              [src, cols, rows](size_t a, size_t b) {
                  return rowLexCmp(src, cols, rows, a, b) < 0;
              });
    uniqRows.insert(uniqRows.end(), nanRows.begin(), nanRows.end());

    return collectRowsByIndex(alloc, x, uniqRows);
}

std::tuple<MValue, MValue, MValue>
uniqueRowsWithIndices(Allocator &alloc, const MValue &x)
{
    validateUniqueRowsInput(x, "unique");
    const size_t rows = x.dims().rows();
    const size_t cols = x.dims().cols();
    if (rows == 0) {
        return std::make_tuple(emptyRowsResult(alloc, cols),
                               emptyRow(alloc), emptyRow(alloc));
    }

    const double *src = x.doubleData();
    // Pass 1: hash non-NaN rows; collect NaN rows in input order.
    std::unordered_map<RowKey, size_t, RowKeyHash, RowKeyEq> firstIdx;
    firstIdx.reserve(rows);
    std::vector<size_t> nanRowOrder;
    for (size_t r = 0; r < rows; ++r) {
        if (rowHasNan(src, cols, rows, r)) {
            nanRowOrder.push_back(r);
        } else {
            firstIdx.try_emplace(extractRow(src, cols, rows, r), r);
        }
    }

    // Sort non-NaN rows lex; NaN rows appended in input order.
    std::vector<size_t> uniqRows;
    uniqRows.reserve(firstIdx.size() + nanRowOrder.size());
    for (const auto &kv : firstIdx) uniqRows.push_back(kv.second);
    std::sort(uniqRows.begin(), uniqRows.end(),
              [src, cols, rows](size_t a, size_t b) {
                  return rowLexCmp(src, cols, rows, a, b) < 0;
              });
    const size_t nanRankBase = uniqRows.size();
    uniqRows.insert(uniqRows.end(), nanRowOrder.begin(), nanRowOrder.end());

    // Pass 2: build value→rank map for non-NaN keys.
    std::unordered_map<RowKey, size_t, RowKeyHash, RowKeyEq> rankByKey;
    rankByKey.reserve(nanRankBase);
    for (size_t r = 0; r < nanRankBase; ++r)
        rankByKey[extractRow(src, cols, rows, uniqRows[r])] = r;

    // Pass 3: ic — every original row → its rank in the unique output.
    auto icRow = MValue::matrix(rows, 1, MType::DOUBLE, &alloc);
    double *ic = icRow.doubleDataMut();
    size_t nanSeen = 0;
    for (size_t r = 0; r < rows; ++r) {
        if (rowHasNan(src, cols, rows, r)) {
            ic[r] = static_cast<double>(nanRankBase + nanSeen + 1);
            ++nanSeen;
        } else {
            ic[r] = static_cast<double>(rankByKey[extractRow(src, cols, rows, r)] + 1);
        }
    }

    // ia: original row index per unique row (column vector, MATLAB convention).
    auto iaCol = MValue::matrix(uniqRows.size(), 1, MType::DOUBLE, &alloc);
    double *ia = iaCol.doubleDataMut();
    for (size_t i = 0; i < uniqRows.size(); ++i)
        ia[i] = static_cast<double>(uniqRows[i] + 1);

    return std::make_tuple(collectRowsByIndex(alloc, x, uniqRows),
                           std::move(iaCol), std::move(icRow));
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

    // Phase P6 followup: irregular-edges path. Inline the bounds check +
    // upper_bound as an unrolled binary search; for very small B (<= 8)
    // a straight linear scan is faster than std::upper_bound's branchy
    // dispatch. Keeps the +1 per match flat in the hot loop instead of
    // bouncing through findBin's call+throw scaffolding.
    const double e0  = e[0];
    const double eN  = e[nBins];
    const size_t nE  = edges.numel();
    if (nBins <= 8) {
        // Linear scan unrolls cleanly under -O2; no branch mispredict on
        // typical Gaussian-into-uniform-bin patterns.
        for (size_t i = 0; i < n; ++i) {
            const double v = p[i];
            if (!(v >= e0 && v <= eN)) continue;
            // Find bin: largest k such that e[k] <= v. Last bin closed.
            if (v == eN) { dst[nBins - 1] += 1.0; continue; }
            size_t k = 0;
            while (k + 1 < nBins && e[k + 1] <= v) ++k;
            dst[k] += 1.0;
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            const double v = p[i];
            if (!(v >= e0 && v <= eN)) continue;
            if (v == eN) { dst[nBins - 1] += 1.0; continue; }
            // Inline std::upper_bound: first index `mid` such that
            // e[mid] > v. The bin is mid - 1.
            size_t lo = 0, hi = nE;
            while (lo < hi) {
                const size_t mid = lo + (hi - lo) / 2;
                if (e[mid] <= v) lo = mid + 1;
                else hi = mid;
            }
            dst[lo - 1] += 1.0;
        }
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

    // Same inlined-binary-search / linear-scan split as histcounts.
    const double e0 = e[0];
    const double eN = e[nBins];
    const size_t nE = edges.numel();
    if (nBins <= 8) {
        for (size_t i = 0; i < n; ++i) {
            const double v = p[i];
            if (!(v >= e0 && v <= eN)) { dst[i] = std::nan(""); continue; }
            if (v == eN) { dst[i] = static_cast<double>(nBins); continue; }
            size_t k = 0;
            while (k + 1 < nBins && e[k + 1] <= v) ++k;
            dst[i] = static_cast<double>(k + 1); // 1-based
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            const double v = p[i];
            if (!(v >= e0 && v <= eN)) { dst[i] = std::nan(""); continue; }
            if (v == eN) { dst[i] = static_cast<double>(nBins); continue; }
            size_t lo = 0, hi = nE;
            while (lo < hi) {
                const size_t mid = lo + (hi - lo) / 2;
                if (e[mid] <= v) lo = mid + 1;
                else hi = mid;
            }
            dst[i] = static_cast<double>(lo); // (lo-1)+1
        }
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

    // Detect a trailing string flag. Currently only 'rows' is recognised
    // — 'first'/'last'/'sorted'/'stable' are accepted in MATLAB but
    // map to no-ops here since our scalar-unique already returns sorted-
    // first (MATLAB's default). Bad strings throw.
    bool useRows = false;
    for (size_t i = 1; i < args.size(); ++i) {
        const MValue &a = args[i];
        if (a.type() != MType::CHAR)
            throw MError("unique: extra arguments must be string flags",
                         0, 0, "unique", "", "m:unique:badArg");
        std::string s = a.toString();
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (s == "rows") useRows = true;
        else if (s == "first" || s == "last" || s == "sorted" || s == "stable") {
            // accepted but no-op for now
        } else {
            throw MError("unique: unknown flag '" + s + "'",
                         0, 0, "unique", "", "m:unique:badFlag");
        }
    }

    if (useRows) {
        if (nargout <= 1) { outs[0] = uniqueRows(alloc, args[0]); return; }
        auto [c, ia, ic] = uniqueRowsWithIndices(alloc, args[0]);
        outs[0] = std::move(c);
        if (nargout > 1) outs[1] = std::move(ia);
        if (nargout > 2) outs[2] = std::move(ic);
        return;
    }

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
