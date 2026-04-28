// libs/builtin/src/math/elementary/discrete.cpp
//
// Discrete-math builtins. Three legacy TUs merged here, separated by
// section headers: set operations (was MStdSetOps), number theory (was
// MStdNumberTheory), combinatorics (was MStdCombinatorics).

#include <numkit/m/builtin/math/elementary/discrete.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Set operations
// ════════════════════════════════════════════════════════════════════════
//
// Inputs flatten to a vector (column-major) before processing; outputs
// are 1×N row vectors of sorted unique values for the set ops, ismember
// preserves the shape of A, histcounts/discretize work on the flat input.

namespace {

// Phase P3 — hash-based set ops. The previous sort-based path was
// O(N log N) on every call; for the bench input (1M doubles drawn from
// a small integer range, K ≈ 8000 unique) it spent ~95% of its time
// sorting duplicates. Hash dedupe drops that to O(N) + O(K log K).

// Hash that normalises -0 → +0 so the two share a bucket.
struct DoubleHashEq0 {
    size_t operator()(double v) const noexcept {
        if (v == 0.0) return 0;          // covers both +0 and -0
        std::uint64_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        bits ^= bits >> 33;
        bits *= 0xff51afd7ed558ccdULL;
        bits ^= bits >> 33;
        bits *= 0xc4ceb9fe1a85ec53ULL;
        bits ^= bits >> 33;
        return static_cast<size_t>(bits);
    }
};

struct IndexedVal {
    double v;
    size_t origIdx;
};

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

using RowKey = std::vector<double>;

struct RowKeyHash {
    size_t operator()(const RowKey &k) const noexcept {
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
        k[c] = (v == 0.0) ? 0.0 : v;
    }
    return k;
}

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

void validateUniqueRowsInput(const MValue &x, const char *fn)
{
    if (x.type() != MType::DOUBLE)
        throw MError(std::string(fn) + ": 'rows' flag requires a DOUBLE matrix",
                     0, 0, fn, "", std::string("m:") + fn + ":rowsType");
    if (x.dims().ndim() > 2)
        throw MError(std::string(fn) + ": 'rows' flag requires a 2D matrix",
                     0, 0, fn, "", std::string("m:") + fn + ":rowsND");
}

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

bool edgesAreUniform(const double *e, size_t nEdges, double &outStep)
{
    if (nEdges < 3) return false;
    const double step = e[1] - e[0];
    if (!(step > 0)) return false;
    const double tol = std::abs(step) * 1e-12;
    for (size_t i = 2; i < nEdges; ++i) {
        const double g = e[i] - e[i - 1];
        if (std::abs(g - step) > tol) return false;
    }
    outStep = step;
    return true;
}

} // namespace (set ops helpers)

// ── unique ─────────────────────────────────────────────────────────

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

// ── unique with 'rows' flag ────────────────────────────────────────

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

    std::vector<size_t> uniqRows;
    uniqRows.reserve(firstIdx.size() + nanRowOrder.size());
    for (const auto &kv : firstIdx) uniqRows.push_back(kv.second);
    std::sort(uniqRows.begin(), uniqRows.end(),
              [src, cols, rows](size_t a, size_t b) {
                  return rowLexCmp(src, cols, rows, a, b) < 0;
              });
    const size_t nanRankBase = uniqRows.size();
    uniqRows.insert(uniqRows.end(), nanRowOrder.begin(), nanRowOrder.end());

    std::unordered_map<RowKey, size_t, RowKeyHash, RowKeyEq> rankByKey;
    rankByKey.reserve(nanRankBase);
    for (size_t r = 0; r < nanRankBase; ++r)
        rankByKey[extractRow(src, cols, rows, uniqRows[r])] = r;

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

    auto iaCol = MValue::matrix(uniqRows.size(), 1, MType::DOUBLE, &alloc);
    double *ia = iaCol.doubleDataMut();
    for (size_t i = 0; i < uniqRows.size(); ++i)
        ia[i] = static_cast<double>(uniqRows[i] + 1);

    return std::make_tuple(collectRowsByIndex(alloc, x, uniqRows),
                           std::move(iaCol), std::move(icRow));
}

// ── ismember ───────────────────────────────────────────────────────

MValue ismember(Allocator &alloc, const MValue &a, const MValue &b)
{
    const size_t na = a.numel();
    const size_t nb = b.numel();

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

// ── union / intersect / setdiff ────────────────────────────────────

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

// ── histcounts / discretize ────────────────────────────────────────

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

    // Irregular-edges path. Inline bounds check + upper_bound.
    const double e0  = e[0];
    const double eN  = e[nBins];
    const size_t nE  = edges.numel();
    if (nBins <= 8) {
        for (size_t i = 0; i < n; ++i) {
            const double v = p[i];
            if (!(v >= e0 && v <= eN)) continue;
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

// ════════════════════════════════════════════════════════════════════════
// Number theory
// ════════════════════════════════════════════════════════════════════════

namespace {

bool isExactNonnegInt(double v, std::uint64_t &outU)
{
    if (!std::isfinite(v) || v < 0) return false;
    if (v != std::floor(v))         return false;
    if (v > static_cast<double>(std::numeric_limits<std::uint64_t>::max()))
        return false;
    outU = static_cast<std::uint64_t>(v);
    return true;
}

bool isPrimeU64(std::uint64_t n)
{
    if (n < 2)                return false;
    if (n == 2 || n == 3)     return true;
    if (n % 2 == 0)           return false;
    if (n % 3 == 0)           return false;
    // 6k ± 1 trial division.
    for (std::uint64_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0)        return false;
        if (n % (i + 2) == 0)  return false;
    }
    return true;
}

bool isPrimeDouble(double v)
{
    std::uint64_t u;
    if (!isExactNonnegInt(v, u)) return false;
    return isPrimeU64(u);
}

} // namespace (number-theory helpers)

MValue primes(Allocator &alloc, double n)
{
    if (!std::isfinite(n) || n < 2)
        return MValue::matrix(1, 0, MType::DOUBLE, &alloc);
    const std::uint64_t N = static_cast<std::uint64_t>(std::floor(n));
    std::vector<bool> composite(N + 1, false);
    for (std::uint64_t i = 2; i * i <= N; ++i)
        if (!composite[i])
            for (std::uint64_t j = i * i; j <= N; j += i)
                composite[j] = true;

    std::vector<double> primesVec;
    primesVec.reserve(static_cast<size_t>(N / std::log(static_cast<double>(N) + 1.0)) + 1);
    for (std::uint64_t i = 2; i <= N; ++i)
        if (!composite[i])
            primesVec.push_back(static_cast<double>(i));

    auto out = MValue::matrix(1, primesVec.size(), MType::DOUBLE, &alloc);
    if (!primesVec.empty())
        std::memcpy(out.doubleDataMut(), primesVec.data(),
                    primesVec.size() * sizeof(double));
    return out;
}

MValue isprime(Allocator &alloc, const MValue &x)
{
    if (x.type() == MType::COMPLEX)
        throw MError("isprime: complex inputs are not supported",
                     0, 0, "isprime", "", "m:isprime:complex");
    auto out = createLike(x, MType::LOGICAL, &alloc);
    uint8_t *dst = out.logicalDataMut();
    const size_t N = x.numel();
    for (size_t i = 0; i < N; ++i) {
        const double v = x.elemAsDouble(i);
        dst[i] = isPrimeDouble(v) ? 1 : 0;
    }
    return out;
}

MValue factor(Allocator &alloc, double n)
{
    std::uint64_t u;
    if (!isExactNonnegInt(n, u))
        throw MError("factor: argument must be a non-negative integer scalar",
                     0, 0, "factor", "", "m:factor:badArg");
    if (u == 0 || u == 1) {
        auto r = MValue::matrix(1, 1, MType::DOUBLE, &alloc);
        r.doubleDataMut()[0] = static_cast<double>(u);
        return r;
    }
    std::vector<double> factors;
    std::uint64_t m = u;
    while (m % 2 == 0) { factors.push_back(2.0); m /= 2; }
    for (std::uint64_t p = 3; p * p <= m; p += 2) {
        while (m % p == 0) {
            factors.push_back(static_cast<double>(p));
            m /= p;
        }
    }
    if (m > 1)
        factors.push_back(static_cast<double>(m));

    auto out = MValue::matrix(1, factors.size(), MType::DOUBLE, &alloc);
    if (!factors.empty())
        std::memcpy(out.doubleDataMut(), factors.data(),
                    factors.size() * sizeof(double));
    return out;
}

// ════════════════════════════════════════════════════════════════════════
// Combinatorics
// ════════════════════════════════════════════════════════════════════════

namespace {

// 11! = 39 916 800 rows of doubles ≈ 3.5 GB if we let n=12 through.
// Capping at n=11 is exactly MATLAB's documented hard limit on perms.
constexpr int kPermMaxN = 11;

std::uint64_t permFactorial(int n)
{
    std::uint64_t f = 1;
    for (int i = 2; i <= n; ++i) f *= static_cast<std::uint64_t>(i);
    return f;
}

double factorialDouble(double v, const char *fn)
{
    if (!std::isfinite(v) || v < 0 || v != std::floor(v))
        throw MError(std::string(fn)
                     + ": entries must be non-negative integers",
                     0, 0, fn, "", std::string("m:") + fn + ":badArg");
    if (v > 170.0)
        return std::numeric_limits<double>::infinity();
    double r = 1.0;
    const int n = static_cast<int>(v);
    for (int i = 2; i <= n; ++i) r *= static_cast<double>(i);
    return r;
}

} // namespace (combinatorics helpers)

MValue perms(Allocator &alloc, const MValue &v)
{
    if (v.type() == MType::COMPLEX)
        throw MError("perms: complex inputs are not supported",
                     0, 0, "perms", "", "m:perms:complex");
    if (v.isEmpty()) {
        return MValue::matrix(1, 0, MType::DOUBLE, &alloc);
    }
    if (!v.dims().isVector())
        throw MError("perms: argument must be a vector",
                     0, 0, "perms", "", "m:perms:notVector");

    const size_t n = v.numel();
    if (n > static_cast<size_t>(kPermMaxN))
        throw MError("perms: numel(v) > 11 is not supported (n! is too large)",
                     0, 0, "perms", "", "m:perms:tooLarge");

    std::vector<double> vals(n);
    for (size_t i = 0; i < n; ++i)
        vals[i] = v.elemAsDouble(i);

    std::vector<double> cur(vals);
    std::sort(cur.begin(), cur.end(), std::greater<double>());

    const size_t totalRows = static_cast<size_t>(permFactorial(static_cast<int>(n)));
    auto out = MValue::matrix(totalRows, n, MType::DOUBLE, &alloc);
    double *dst = out.doubleDataMut();

    size_t row = 0;
    do {
        for (size_t c = 0; c < n; ++c)
            dst[c * totalRows + row] = cur[c];
        ++row;
    } while (std::prev_permutation(cur.begin(), cur.end()));

    return out;
}

MValue factorial(Allocator &alloc, const MValue &n)
{
    if (n.type() == MType::COMPLEX)
        throw MError("factorial: complex inputs are not supported",
                     0, 0, "factorial", "", "m:factorial:complex");
    auto out = createLike(n, MType::DOUBLE, &alloc);
    double *dst = out.doubleDataMut();
    const size_t N = n.numel();
    for (size_t i = 0; i < N; ++i)
        dst[i] = factorialDouble(n.elemAsDouble(i), "factorial");
    return out;
}

MValue nchoosek(Allocator &alloc, double n, double k)
{
    if (!std::isfinite(n) || !std::isfinite(k))
        throw MError("nchoosek: arguments must be finite",
                     0, 0, "nchoosek", "", "m:nchoosek:badArg");
    if (n < 0 || k < 0 || n != std::floor(n) || k != std::floor(k))
        throw MError("nchoosek: arguments must be non-negative integers",
                     0, 0, "nchoosek", "", "m:nchoosek:badArg");
    if (k > n)
        throw MError("nchoosek: k must satisfy 0 ≤ k ≤ n",
                     0, 0, "nchoosek", "", "m:nchoosek:kTooLarge");

    double kk = (k > n - k) ? n - k : k;
    if (kk == 0.0)
        return MValue::scalar(1.0, &alloc);

    double r = 1.0;
    const int kInt = static_cast<int>(kk);
    for (int i = 0; i < kInt; ++i) {
        r = r * (n - static_cast<double>(i)) / static_cast<double>(i + 1);
    }
    return MValue::scalar(std::round(r), &alloc);
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════════
namespace detail {

void unique_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw MError("unique: requires 1 argument",
                     0, 0, "unique", "", "m:unique:nargin");
    auto &alloc = ctx.engine->allocator();

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

void primes_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("primes: requires 1 argument",
                     0, 0, "primes", "", "m:primes:nargin");
    outs[0] = primes(ctx.engine->allocator(), args[0].toScalar());
}

void isprime_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("isprime: requires 1 argument",
                     0, 0, "isprime", "", "m:isprime:nargin");
    outs[0] = isprime(ctx.engine->allocator(), args[0]);
}

void factor_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("factor: requires 1 argument",
                     0, 0, "factor", "", "m:factor:nargin");
    if (!args[0].isScalar())
        throw MError("factor: argument must be a scalar",
                     0, 0, "factor", "", "m:factor:notScalar");
    outs[0] = factor(ctx.engine->allocator(), args[0].toScalar());
}

void perms_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("perms: requires 1 argument",
                     0, 0, "perms", "", "m:perms:nargin");
    outs[0] = perms(ctx.engine->allocator(), args[0]);
}

void factorial_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("factorial: requires 1 argument",
                     0, 0, "factorial", "", "m:factorial:nargin");
    outs[0] = factorial(ctx.engine->allocator(), args[0]);
}

void nchoosek_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("nchoosek: requires 2 arguments (n, k)",
                     0, 0, "nchoosek", "", "m:nchoosek:nargin");
    if (!args[0].isScalar() || !args[1].isScalar())
        throw MError("nchoosek: vector input form is not yet supported "
                     "(nchoosek(v, k) for k-combinations of v)",
                     0, 0, "nchoosek", "", "m:nchoosek:vectorForm");
    outs[0] = nchoosek(ctx.engine->allocator(),
                       args[0].toScalar(), args[1].toScalar());
}

} // namespace detail

} // namespace numkit::m::builtin
