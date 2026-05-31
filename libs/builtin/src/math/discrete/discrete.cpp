// libs/builtin/src/math/elementary/discrete.cpp
//
// Discrete-math builtins. Three legacy TUs were merged here, separated
// by section headers: set operations, number theory, combinatorics.

#include <numkit/builtin/math/discrete/discrete.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "rows_helpers.hpp"  // detail::collectRowsByIndex

#include <algorithm>
#include <cmath>
#include <complex>
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

namespace numkit::builtin {

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

inline Value emptyRow(std::pmr::memory_resource *mr)
{
    return Value::matrix(1, 0, ValueType::DOUBLE, mr);
}

inline Value rowFromVec(const double *data, std::size_t n, std::pmr::memory_resource *mr)
{
    auto r = Value::matrix(1, n, ValueType::DOUBLE, mr);
    if (n > 0)
        std::copy(data, data + n, r.doubleDataMut());
    return r;
}

// ── complex unique ─────────────────────────────────────────────────
// MATLAB unique() supports COMPLEX: values are ordered by magnitude |z|
// then phase angle arg(z) (the same key as complex sort); a value with a
// NaN component sorts last and is always distinct. Two complex values are
// "equal" iff identical (real AND imag equal). Dedup is first-occurrence;
// ia/ic match numkit's double-unique row orientation. (Linear first-occur
// scan: O(n*u) — fine for the small complex sets unique() sees in practice.)
inline bool cxUniqLess(Complex a, Complex b)
{
    const double am = std::abs(a), bm = std::abs(b);
    const bool an = std::isnan(am), bn = std::isnan(bm);
    if (an || bn) { if (an && bn) return false; return bn; }   // non-NaN < NaN
    if (am != bm) return am < bm;
    return std::arg(a) < std::arg(b);
}
inline bool cxUniqEqual(Complex a, Complex b)
{
    return a.real() == b.real() && a.imag() == b.imag();        // NaN never equal
}

std::tuple<Value, Value, Value>
uniqueComplexFull(const Value &x, std::pmr::memory_resource *mr, bool stable)
{
    const size_t n = x.numel();
    const Complex *p = x.complexData();
    ScratchArena scratch(mr);

    struct UC { Complex v; size_t firstIdx; };
    auto uniq = ScratchVec<UC>(&scratch);
    auto ic0  = ScratchVec<size_t>(n, &scratch);   // original -> 0-based pos in uniq
    for (size_t i = 0; i < n; ++i) {
        const Complex z = p[i];
        const bool nanComp = std::isnan(z.real()) || std::isnan(z.imag());
        size_t found = static_cast<size_t>(-1);
        if (!nanComp)
            for (size_t k = 0; k < uniq.size(); ++k)
                if (cxUniqEqual(uniq[k].v, z)) { found = k; break; }
        if (found == static_cast<size_t>(-1)) { ic0[i] = uniq.size(); uniq.push_back({z, i}); }
        else                                   { ic0[i] = found; }
    }

    const size_t u = uniq.size();
    auto perm = ScratchVec<size_t>(u, &scratch);
    for (size_t k = 0; k < u; ++k) perm[k] = k;
    if (!stable)
        std::stable_sort(perm.begin(), perm.end(),
                         [&](size_t a, size_t b) { return cxUniqLess(uniq[a].v, uniq[b].v); });
    auto newRank = ScratchVec<size_t>(u, &scratch);
    for (size_t k = 0; k < u; ++k) newRank[perm[k]] = k;

    auto cOut  = Value::matrix(1, u, ValueType::COMPLEX, mr);
    auto iaRow = Value::matrix(1, u, ValueType::DOUBLE, mr);
    auto icRow = Value::matrix(1, n, ValueType::DOUBLE, mr);
    Complex *cd = cOut.complexDataMut();
    double  *ia = iaRow.doubleDataMut();
    double  *ici = icRow.doubleDataMut();
    for (size_t k = 0; k < u; ++k) {
        cd[k] = uniq[perm[k]].v;
        ia[k] = static_cast<double>(uniq[perm[k]].firstIdx + 1);
    }
    for (size_t i = 0; i < n; ++i)
        ici[i] = static_cast<double>(newRank[ic0[i]] + 1);
    return std::make_tuple(std::move(cOut), std::move(iaRow), std::move(icRow));
}

// ── 'rows' helpers ─────────────────────────────────────────────

// Hash-key type for unique('rows') / setops('rows'). pmr-backed so the
// keys themselves bump into the per-call ScratchArena along with the
// std::pmr::unordered_map's hash buckets — no per-row heap mr.
using RowKey = ScratchVec<double>;

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

inline RowKey extractRow(const double *p, size_t cols, size_t rows, size_t r, std::pmr::memory_resource *mr)
{
    RowKey k(cols, mr);
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

inline Value emptyRowsResult(size_t cols, std::pmr::memory_resource *mr)
{
    return Value::matrix(0, cols, ValueType::DOUBLE, mr);
}

// Note: collectRowsByIndex moved to rows_helpers.hpp (shared with
// matrix.cpp's sortRowsImpl). The duplicate definition that used to
// live here has been removed in favour of detail::collectRowsByIndex.

void validateUniqueRowsInput(const Value &x, const char *fn)
{
    if (x.type() != ValueType::DOUBLE)
        throw Error(std::string(fn) + ": 'rows' flag requires a DOUBLE matrix",
                     0, 0, fn, "", std::string("numkit:") + fn + ":rowsType");
    if (x.dims().ndim() > 2)
        throw Error(std::string(fn) + ": 'rows' flag requires a 2D matrix",
                     0, 0, fn, "", std::string("numkit:") + fn + ":rowsND");
}

std::pmr::unordered_set<double, DoubleHashEq0>
hashSetNoNaN(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::unordered_set<double, DoubleHashEq0> s(mr);
    s.reserve(x.numel() / 2 + 1);
    const double *p = x.doubleData();
    const size_t n = x.numel();
    for (size_t i = 0; i < n; ++i)
        if (!std::isnan(p[i])) s.insert(p[i]);
    return s;
}

void validateEdges(const Value &edges, const char *fn)
{
    if (edges.numel() < 2)
        throw Error(std::string(fn) + ": edges must have length >= 2",
                     0, 0, fn, "", std::string("numkit:") + fn + ":shortEdges");
    const double *e = edges.doubleData();
    for (size_t i = 1; i < edges.numel(); ++i)
        if (!(e[i] >= e[i - 1]))
            throw Error(std::string(fn) + ": edges must be ascending",
                         0, 0, fn, "", std::string("numkit:") + fn + ":badEdges");
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

Value unique(const Value &x, std::pmr::memory_resource *mr, bool stable)
{
    const size_t n = x.numel();
    if (n == 0) return emptyRow(mr);
    if (x.type() == ValueType::COMPLEX)
        return std::get<0>(uniqueComplexFull(x, mr, stable));

    ScratchArena scratch(mr);
    const double *p = x.doubleData();

    if (stable) {
        // First-occurrence order, no sort. Each NaN is distinct (kept).
        std::pmr::unordered_set<double, DoubleHashEq0> seen(&scratch);
        seen.reserve(n / 2 + 1);
        auto out = ScratchVec<double>(&scratch);
        out.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            if (std::isnan(p[i])) out.push_back(std::nan(""));
            else if (seen.insert(p[i]).second) out.push_back(p[i]);
        }
        return rowFromVec(out.data(), out.size(), mr);
    }

    std::pmr::unordered_set<double, DoubleHashEq0> seen(&scratch);
    seen.reserve(n / 2 + 1);
    size_t nanCount = 0;
    for (size_t i = 0; i < n; ++i) {
        if (std::isnan(p[i])) ++nanCount;
        else seen.insert(p[i]);
    }

    auto out = ScratchVec<double>(&scratch);
    out.reserve(seen.size() + nanCount);
    out.assign(seen.begin(), seen.end());
    std::sort(out.begin(), out.end());
    for (size_t i = 0; i < nanCount; ++i)
        out.push_back(std::nan(""));
    return rowFromVec(out.data(), out.size(), mr);
}

std::tuple<Value, Value, Value>
uniqueWithIndices(const Value &x, std::pmr::memory_resource *mr, bool stable)
{
    const size_t n = x.numel();
    if (n == 0) {
        return std::make_tuple(emptyRow(mr), emptyRow(mr),
                               emptyRow(mr));
    }
    if (x.type() == ValueType::COMPLEX)
        return uniqueComplexFull(x, mr, stable);

    if (stable) {
        // First-occurrence order. C = X(ia); X = C(ic). Each NaN distinct.
        ScratchArena scratch(mr);
        std::pmr::unordered_map<double, size_t, DoubleHashEq0> posByVal(&scratch);
        posByVal.reserve(n / 2 + 1);
        auto uVals = ScratchVec<double>(&scratch);
        auto iaVec = ScratchVec<double>(&scratch);
        auto ic    = ScratchVec<double>(n, &scratch);
        const double *p = x.doubleData();
        for (size_t i = 0; i < n; ++i) {
            if (std::isnan(p[i])) {
                uVals.push_back(std::nan(""));
                iaVec.push_back(static_cast<double>(i + 1));
                ic[i] = static_cast<double>(uVals.size());      // own position
            } else {
                auto it = posByVal.find(p[i]);
                if (it == posByVal.end()) {
                    posByVal.emplace(p[i], uVals.size());        // 0-based pos
                    uVals.push_back(p[i]);
                    iaVec.push_back(static_cast<double>(i + 1));
                    ic[i] = static_cast<double>(uVals.size());   // 1-based
                } else {
                    ic[i] = static_cast<double>(it->second + 1);
                }
            }
        }
        auto cOut  = Value::matrix(1, uVals.size(), ValueType::DOUBLE, mr);
        auto iaRow = Value::matrix(1, iaVec.size(), ValueType::DOUBLE, mr);
        std::copy(uVals.begin(), uVals.end(), cOut.doubleDataMut());
        std::copy(iaVec.begin(), iaVec.end(), iaRow.doubleDataMut());
        auto icRow = Value::matrix(1, n, ValueType::DOUBLE, mr);
        std::copy(ic.begin(), ic.end(), icRow.doubleDataMut());
        return std::make_tuple(std::move(cOut), std::move(iaRow), std::move(icRow));
    }

    ScratchArena scratch(mr);
    std::pmr::unordered_map<double, size_t, DoubleHashEq0> firstIdx(&scratch);
    firstIdx.reserve(n / 2 + 1);
    auto nanIdxOrder = ScratchVec<size_t>(&scratch);
    const double *p = x.doubleData();
    for (size_t i = 0; i < n; ++i) {
        if (std::isnan(p[i])) {
            nanIdxOrder.push_back(i);
        } else {
            firstIdx.try_emplace(p[i], i);
        }
    }

    auto sorted = ScratchVec<IndexedVal>(&scratch);
    sorted.reserve(firstIdx.size() + nanIdxOrder.size());
    for (const auto &kv : firstIdx)
        sorted.push_back({kv.first, kv.second});
    std::sort(sorted.begin(), sorted.end(),
              [](const IndexedVal &a, const IndexedVal &b) {
                  return a.v < b.v;
              });
    for (size_t idx : nanIdxOrder)
        sorted.push_back({std::nan(""), idx});

    std::pmr::unordered_map<double, size_t, DoubleHashEq0> rankByValue(&scratch);
    rankByValue.reserve(firstIdx.size());
    const size_t nanRankBase = sorted.size() - nanIdxOrder.size();
    for (size_t r = 0; r < nanRankBase; ++r)
        rankByValue[sorted[r].v] = r;

    auto ic = ScratchVec<double>(n, &scratch);
    size_t nanSeen = 0;
    for (size_t i = 0; i < n; ++i) {
        if (std::isnan(p[i])) {
            ic[i] = static_cast<double>(nanRankBase + nanSeen + 1);
            ++nanSeen;
        } else {
            ic[i] = static_cast<double>(rankByValue[p[i]] + 1);
        }
    }

    auto cOut = Value::matrix(1, sorted.size(), ValueType::DOUBLE, mr);
    auto iaRow = Value::matrix(1, sorted.size(), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < sorted.size(); ++i) {
        cOut.doubleDataMut()[i]  = sorted[i].v;
        iaRow.doubleDataMut()[i] = static_cast<double>(sorted[i].origIdx + 1);
    }
    auto icRow = Value::matrix(1, n, ValueType::DOUBLE, mr);
    std::copy(ic.begin(), ic.end(), icRow.doubleDataMut());

    return std::make_tuple(std::move(cOut), std::move(iaRow), std::move(icRow));
}

// ── unique with 'rows' flag ────────────────────────────────────────

Value uniqueRows(const Value &x, std::pmr::memory_resource *mr)
{
    validateUniqueRowsInput(x, "unique");
    const size_t rows = x.dims().rows();
    const size_t cols = x.dims().cols();
    if (rows == 0) return emptyRowsResult(cols, mr);

    const double *src = x.doubleData();
    ScratchArena scratch(mr);
    std::pmr::unordered_map<RowKey, size_t, RowKeyHash, RowKeyEq> firstIdx(&scratch);
    firstIdx.reserve(rows);
    auto nanRows = ScratchVec<size_t>(&scratch);
    for (size_t r = 0; r < rows; ++r) {
        if (rowHasNan(src, cols, rows, r)) {
            nanRows.push_back(r);
        } else {
            firstIdx.try_emplace(extractRow(src, cols, rows, r, &scratch), r);
        }
    }

    auto uniqRows = ScratchVec<size_t>(&scratch);
    uniqRows.reserve(firstIdx.size() + nanRows.size());
    for (const auto &kv : firstIdx) uniqRows.push_back(kv.second);
    std::sort(uniqRows.begin(), uniqRows.end(),
              [src, cols, rows](size_t a, size_t b) {
                  return rowLexCmp(src, cols, rows, a, b) < 0;
              });
    uniqRows.insert(uniqRows.end(), nanRows.begin(), nanRows.end());

    return detail::collectRowsByIndex(mr, x, uniqRows.data(), uniqRows.size());
}

std::tuple<Value, Value, Value>
uniqueRowsWithIndices(const Value &x, std::pmr::memory_resource *mr)
{
    validateUniqueRowsInput(x, "unique");
    const size_t rows = x.dims().rows();
    const size_t cols = x.dims().cols();
    if (rows == 0) {
        return std::make_tuple(emptyRowsResult(cols, mr),
                               emptyRow(mr), emptyRow(mr));
    }

    const double *src = x.doubleData();
    ScratchArena scratch(mr);
    std::pmr::unordered_map<RowKey, size_t, RowKeyHash, RowKeyEq> firstIdx(&scratch);
    firstIdx.reserve(rows);
    auto nanRowOrder = ScratchVec<size_t>(&scratch);
    for (size_t r = 0; r < rows; ++r) {
        if (rowHasNan(src, cols, rows, r)) {
            nanRowOrder.push_back(r);
        } else {
            firstIdx.try_emplace(extractRow(src, cols, rows, r, &scratch), r);
        }
    }

    auto uniqRows = ScratchVec<size_t>(&scratch);
    uniqRows.reserve(firstIdx.size() + nanRowOrder.size());
    for (const auto &kv : firstIdx) uniqRows.push_back(kv.second);
    std::sort(uniqRows.begin(), uniqRows.end(),
              [src, cols, rows](size_t a, size_t b) {
                  return rowLexCmp(src, cols, rows, a, b) < 0;
              });
    const size_t nanRankBase = uniqRows.size();
    uniqRows.insert(uniqRows.end(), nanRowOrder.begin(), nanRowOrder.end());

    std::pmr::unordered_map<RowKey, size_t, RowKeyHash, RowKeyEq> rankByKey(&scratch);
    rankByKey.reserve(nanRankBase);
    for (size_t r = 0; r < nanRankBase; ++r)
        rankByKey[extractRow(src, cols, rows, uniqRows[r], &scratch)] = r;

    auto icRow = Value::matrix(rows, 1, ValueType::DOUBLE, mr);
    double *ic = icRow.doubleDataMut();
    size_t nanSeen = 0;
    for (size_t r = 0; r < rows; ++r) {
        if (rowHasNan(src, cols, rows, r)) {
            ic[r] = static_cast<double>(nanRankBase + nanSeen + 1);
            ++nanSeen;
        } else {
            ic[r] = static_cast<double>(rankByKey[extractRow(src, cols, rows, r, &scratch)] + 1);
        }
    }

    auto iaCol = Value::matrix(uniqRows.size(), 1, ValueType::DOUBLE, mr);
    double *ia = iaCol.doubleDataMut();
    for (size_t i = 0; i < uniqRows.size(); ++i)
        ia[i] = static_cast<double>(uniqRows[i] + 1);

    return std::make_tuple(detail::collectRowsByIndex(mr, x, uniqRows.data(), uniqRows.size()),
                           std::move(iaCol), std::move(icRow));
}

// ── ismember ───────────────────────────────────────────────────────

// ── complex ismember ───────────────────────────────────────────────
// MATLAB ismember supports complex: membership is EXACT equality (real AND
// imag equal); Locb is the LOWEST 1-based index in B of a match (0 if none).
// A NaN component never matches. Reals compared against complex are treated
// as z+0i. Triggered whenever either operand is COMPLEX.
struct CxKey { double re, im; bool operator==(const CxKey &o) const { return re == o.re && im == o.im; } };
struct CxKeyHash {
    std::size_t operator()(const CxKey &k) const {
        const double r = (k.re == 0.0) ? 0.0 : k.re;   // -0 and +0 hash alike
        const double i = (k.im == 0.0) ? 0.0 : k.im;
        std::uint64_t br, bi;
        std::memcpy(&br, &r, sizeof(br));
        std::memcpy(&bi, &i, sizeof(bi));
        return std::hash<std::uint64_t>{}(br) ^ (std::hash<std::uint64_t>{}(bi) * 0x9e3779b97f4a7c15ULL);
    }
};
inline Complex elemAsComplex(const Value &v, std::size_t k)
{
    return (v.type() == ValueType::COMPLEX) ? v.complexData()[k]
                                            : Complex(v.elemAsDouble(k), 0.0);
}
std::pair<Value, Value>
ismemberComplex(const Value &a, const Value &b, bool wantLoc, std::pmr::memory_resource *mr)
{
    const std::size_t na = a.numel(), nb = b.numel();
    Value tf = createLike(a, ValueType::LOGICAL, mr);
    Value loc;
    double *lo = nullptr;
    if (wantLoc) { loc = createLike(a, ValueType::DOUBLE, mr); lo = loc.doubleDataMut(); }
    if (na == 0) return {std::move(tf), std::move(loc)};
    uint8_t *out = tf.logicalDataMut();

    ScratchArena scratch(mr);
    std::pmr::unordered_map<CxKey, double, CxKeyHash> idxB(&scratch);
    idxB.reserve(nb);
    for (std::size_t i = 0; i < nb; ++i) {
        const Complex z = elemAsComplex(b, i);
        if (std::isnan(z.real()) || std::isnan(z.imag())) continue;
        idxB.emplace(CxKey{z.real(), z.imag()}, static_cast<double>(i + 1)); // lowest index wins
    }
    for (std::size_t i = 0; i < na; ++i) {
        const Complex z = elemAsComplex(a, i);
        if (std::isnan(z.real()) || std::isnan(z.imag())) { out[i] = 0; if (lo) lo[i] = 0.0; continue; }
        auto it = idxB.find(CxKey{z.real(), z.imag()});
        const bool found = (it != idxB.end());
        out[i] = found ? 1 : 0;
        if (lo) lo[i] = found ? it->second : 0.0;
    }
    return {std::move(tf), std::move(loc)};
}

// Complex set operations (C output only, matching numkit's real setops which
// produce just C — ia/ib are not implemented for real either). Equality is
// EXACT (re&im); ordering is |z| then angle ('sorted', default); 'stable'
// keeps first-occurrence order (A then, for union, B). A NaN component is
// skipped (matches the real setops). Reals vs complex compare as z+0i.
enum class CxSetOp { Intersect, Union, Diff };
Value complexSetOp(const Value &a, const Value &b, CxSetOp op, bool stable,
                   std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    std::pmr::unordered_set<CxKey, CxKeyHash> setB(&scratch);
    setB.reserve(b.numel());
    for (std::size_t i = 0; i < b.numel(); ++i) {
        const Complex z = elemAsComplex(b, i);
        if (std::isnan(z.real()) || std::isnan(z.imag())) continue;
        setB.insert(CxKey{z.real(), z.imag()});
    }
    std::pmr::unordered_set<CxKey, CxKeyHash> seen(&scratch);
    seen.reserve(a.numel() + b.numel());
    auto out = ScratchVec<Complex>(&scratch);

    auto consider = [&](Complex z) {
        if (std::isnan(z.real()) || std::isnan(z.imag())) return;   // skip NaN (as real setops do)
        const CxKey k{z.real(), z.imag()};
        const bool inB = setB.count(k) != 0;
        bool keep = (op == CxSetOp::Union) ? true
                  : (op == CxSetOp::Intersect) ? inB
                                               : !inB;            // Diff
        if (keep && seen.insert(k).second) out.push_back(z);
    };
    for (std::size_t i = 0; i < a.numel(); ++i) consider(elemAsComplex(a, i));
    if (op == CxSetOp::Union)
        for (std::size_t i = 0; i < b.numel(); ++i) consider(elemAsComplex(b, i));

    if (!stable)
        std::stable_sort(out.begin(), out.end(),
                         [](Complex x, Complex y) { return cxUniqLess(x, y); });

    auto r = Value::matrix(1, out.size(), ValueType::COMPLEX, mr);
    if (!out.empty())
        std::copy(out.begin(), out.end(), r.complexDataMut());
    return r;
}

Value ismember(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    if (a.type() == ValueType::COMPLEX || b.type() == ValueType::COMPLEX)
        return ismemberComplex(a, b, /*wantLoc=*/false, mr).first;
    const size_t na = a.numel();
    const size_t nb = b.numel();

    auto r = createLike(a, ValueType::LOGICAL, mr);
    if (na == 0) return r;
    if (nb == 0) {
        std::fill(r.logicalDataMut(), r.logicalDataMut() + na, 0);
        return r;
    }

    ScratchArena scratch(mr);
    std::pmr::unordered_set<double, DoubleHashEq0> setB(&scratch);
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

Value setUnion(const Value &a, const Value &b, std::pmr::memory_resource *mr, bool stable)
{
    if (a.type() == ValueType::COMPLEX || b.type() == ValueType::COMPLEX)
        return complexSetOp(a, b, CxSetOp::Union, stable, mr);
    ScratchArena scratch(mr);
    if (stable) {
        // MATLAB 'stable': unique(A) in A-order, then B's new values in
        // B-order (first occurrence wins).
        std::pmr::unordered_set<double, DoubleHashEq0> seen(&scratch);
        auto out = ScratchVec<double>(&scratch);
        out.reserve(a.numel() + b.numel());
        const double *pa = a.doubleData();
        for (size_t i = 0; i < a.numel(); ++i)
            if (!std::isnan(pa[i]) && seen.insert(pa[i]).second) out.push_back(pa[i]);
        const double *pbS = b.doubleData();
        for (size_t i = 0; i < b.numel(); ++i)
            if (!std::isnan(pbS[i]) && seen.insert(pbS[i]).second) out.push_back(pbS[i]);
        return rowFromVec(out.data(), out.size(), mr);
    }
    auto s = hashSetNoNaN(a, &scratch);
    const double *pb = b.doubleData();
    for (size_t i = 0; i < b.numel(); ++i)
        if (!std::isnan(pb[i])) s.insert(pb[i]);
    ScratchVec<double> out(s.begin(), s.end(), &scratch);
    std::sort(out.begin(), out.end());
    return rowFromVec(out.data(), out.size(), mr);
}

Value setIntersect(const Value &a, const Value &b, std::pmr::memory_resource *mr, bool stable)
{
    if (a.type() == ValueType::COMPLEX || b.type() == ValueType::COMPLEX)
        return complexSetOp(a, b, CxSetOp::Intersect, stable, mr);
    if (stable) {
        // MATLAB 'stable': values present in BOTH, in A-order (first
        // occurrence wins).
        ScratchArena scr(mr);
        auto setB = hashSetNoNaN(b, &scr);
        std::pmr::unordered_set<double, DoubleHashEq0> seen(&scr);
        auto out = ScratchVec<double>(&scr);
        const double *pa = a.doubleData();
        for (size_t i = 0; i < a.numel(); ++i) {
            const double v = pa[i];
            if (!std::isnan(v) && setB.count(v) && seen.insert(v).second)
                out.push_back(v);
        }
        return rowFromVec(out.data(), out.size(), mr);
    }
    const bool aSmaller = a.numel() <= b.numel();
    const Value &small = aSmaller ? a : b;
    const Value &large = aSmaller ? b : a;

    ScratchArena scratch(mr);
    auto smallSet = hashSetNoNaN(small, &scratch);
    std::pmr::unordered_set<double, DoubleHashEq0> seenInLarge(&scratch);
    seenInLarge.reserve(smallSet.size());
    auto out = ScratchVec<double>(&scratch);
    out.reserve(smallSet.size());

    const double *pl = large.doubleData();
    for (size_t i = 0; i < large.numel(); ++i) {
        const double v = pl[i];
        if (std::isnan(v)) continue;
        if (smallSet.count(v) && seenInLarge.insert(v).second)
            out.push_back(v);
    }
    std::sort(out.begin(), out.end());
    return rowFromVec(out.data(), out.size(), mr);
}

Value setDiff(const Value &a, const Value &b, std::pmr::memory_resource *mr, bool stable)
{
    if (a.type() == ValueType::COMPLEX || b.type() == ValueType::COMPLEX)
        return complexSetOp(a, b, CxSetOp::Diff, stable, mr);
    ScratchArena scratch(mr);
    auto setB = hashSetNoNaN(b, &scratch);
    std::pmr::unordered_set<double, DoubleHashEq0> seen(&scratch);
    seen.reserve(a.numel() / 2 + 1);
    auto out = ScratchVec<double>(&scratch);
    out.reserve(a.numel());
    const double *pa = a.doubleData();
    // This loop already walks A in order keeping first occurrences, so for
    // 'stable' we simply skip the final sort.
    for (size_t i = 0; i < a.numel(); ++i) {
        const double v = pa[i];
        if (std::isnan(v)) continue;
        if (setB.count(v) == 0 && seen.insert(v).second)
            out.push_back(v);
    }
    if (!stable) std::sort(out.begin(), out.end());
    return rowFromVec(out.data(), out.size(), mr);
}

// ── histcounts / discretize ────────────────────────────────────────

Value histcounts(const Value &x, const Value &edges, std::pmr::memory_resource *mr)
{
    validateEdges(edges, "histcounts");
    const size_t nBins = edges.numel() - 1;
    auto r = Value::matrix(1, nBins, ValueType::DOUBLE, mr);
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

Value histcounts(const Value &x, const Value &edges, HistNorm norm, std::pmr::memory_resource *mr)
{
    Value counts = histcounts(x, edges, mr);   // raw count row vector
    if (norm == HistNorm::Count) return counts;

    const size_t nBins = counts.numel();
    double *c = counts.doubleDataMut();
    const double N = static_cast<double>(x.numel());
    const double *e = edges.doubleData();

    switch (norm) {
    case HistNorm::Count:
        break;                              // handled above
    case HistNorm::Probability:
        if (N > 0) for (size_t i = 0; i < nBins; ++i) c[i] /= N;
        break;
    case HistNorm::CountDensity:
        for (size_t i = 0; i < nBins; ++i) c[i] /= (e[i + 1] - e[i]);
        break;
    case HistNorm::Pdf:
        if (N > 0)
            for (size_t i = 0; i < nBins; ++i) c[i] /= (N * (e[i + 1] - e[i]));
        break;
    case HistNorm::CumCount: {
        double acc = 0.0;
        for (size_t i = 0; i < nBins; ++i) { acc += c[i]; c[i] = acc; }
        break;
    }
    case HistNorm::Cdf: {
        double acc = 0.0;
        for (size_t i = 0; i < nBins; ++i) {
            acc += c[i];
            c[i] = (N > 0) ? acc / N : 0.0;
        }
        break;
    }
    }
    return counts;
}

Value discretize(const Value &x, const Value &edges, std::pmr::memory_resource *mr)
{
    validateEdges(edges, "discretize");
    auto r = createLike(x, ValueType::DOUBLE, mr);
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

// ── histc (legacy) ──────────────────────────────────────────────────
//
// 0-based bin for v given ascending edges e[0..nE-1]: k in [0, nE-2] when
// e[k] <= v < e[k+1]; nE-1 when v == e[nE-1] (last bin = exact equal to
// last edge). Returns SIZE_MAX when v is NaN or outside [e[0], e[nE-1]].
namespace {
inline std::size_t histcBin(double v, const double *e, std::size_t nE)
{
    if (std::isnan(v) || v < e[0] || v > e[nE - 1]) return SIZE_MAX;
    if (v == e[nE - 1]) return nE - 1;
    std::size_t lo = 0, hi = nE;
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (e[mid] <= v) lo = mid + 1; else hi = mid;
    }
    return lo - 1;            // e[lo-1] <= v < e[lo]
}
} // namespace

Value histc(const Value &x, const Value &edges, std::pmr::memory_resource *mr)
{
    validateEdges(edges, "histc");
    const std::size_t nE = edges.numel();
    const double *e = edges.doubleData();

    const std::size_t rows = static_cast<std::size_t>(x.dims().rows());
    const std::size_t cols = x.numel() == 0 ? 0 : x.numel() / std::max<std::size_t>(rows, 1);
    const double *p = x.doubleData();

    // Row vector -> count along the row, return a 1 × nE row. Otherwise
    // (column vector / matrix) count each column -> nE × ncols.
    if (rows == 1) {
        Value r = Value::matrix(1, nE, ValueType::DOUBLE, mr);
        double *dst = r.doubleDataMut();
        std::fill(dst, dst + nE, 0.0);
        const std::size_t n = x.numel();
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t b = histcBin(p[i], e, nE);
            if (b != SIZE_MAX) dst[b] += 1.0;
        }
        return r;
    }

    const std::size_t H = rows, W = cols;
    Value r = Value::matrix(nE, W, ValueType::DOUBLE, mr);
    double *dst = r.doubleDataMut();
    std::fill(dst, dst + nE * W, 0.0);
    for (std::size_t c = 0; c < W; ++c)
        for (std::size_t i = 0; i < H; ++i) {
            const std::size_t b = histcBin(p[c * H + i], e, nE);
            if (b != SIZE_MAX) dst[c * nE + b] += 1.0;
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

Value primes(double n, std::pmr::memory_resource *mr)
{
    if (!std::isfinite(n) || n < 2)
        return Value::matrix(1, 0, ValueType::DOUBLE, mr);
    const std::uint64_t N = static_cast<std::uint64_t>(std::floor(n));
    ScratchArena scratch(mr);
    // Sieve mask — uint8_t rather than bool to avoid std::pmr::vector<bool>'s
    // bit-packed proxy reference (MSVC's specialisation has caused subtle
    // initialisation bugs here in the past). One byte per slot is also
    // friendlier on the cache for the inner mark loop.
    auto composite = ScratchVec<std::uint8_t>(N + 1, &scratch);
    for (std::uint64_t i = 2; i * i <= N; ++i)
        if (!composite[i])
            for (std::uint64_t j = i * i; j <= N; j += i)
                composite[j] = 1;

    auto primesVec = ScratchVec<double>(&scratch);
    primesVec.reserve(static_cast<size_t>(N / std::log(static_cast<double>(N) + 1.0)) + 1);
    for (std::uint64_t i = 2; i <= N; ++i)
        if (!composite[i])
            primesVec.push_back(static_cast<double>(i));

    auto out = Value::matrix(1, primesVec.size(), ValueType::DOUBLE, mr);
    if (!primesVec.empty())
        std::memcpy(out.doubleDataMut(), primesVec.data(),
                    primesVec.size() * sizeof(double));
    return out;
}

Value isprime(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.type() == ValueType::COMPLEX)
        throw Error("isprime: complex inputs are not supported",
                     0, 0, "isprime", "", "numkit:isprime:complex");
    auto out = createLike(x, ValueType::LOGICAL, mr);
    uint8_t *dst = out.logicalDataMut();
    const size_t N = x.numel();
    for (size_t i = 0; i < N; ++i) {
        const double v = x.elemAsDouble(i);
        dst[i] = isPrimeDouble(v) ? 1 : 0;
    }
    return out;
}

Value factor(double n, std::pmr::memory_resource *mr)
{
    std::uint64_t u;
    if (!isExactNonnegInt(n, u))
        throw Error("factor: argument must be a non-negative integer scalar",
                     0, 0, "factor", "", "numkit:factor:badArg");
    if (u == 0 || u == 1) {
        auto r = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        r.doubleDataMut()[0] = static_cast<double>(u);
        return r;
    }
    ScratchArena scratch(mr);
    auto factors = ScratchVec<double>(&scratch);
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

    auto out = Value::matrix(1, factors.size(), ValueType::DOUBLE, mr);
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
        throw Error(std::string(fn)
                     + ": entries must be non-negative integers",
                     0, 0, fn, "", std::string("numkit:") + fn + ":badArg");
    if (v > 170.0)
        return std::numeric_limits<double>::infinity();
    double r = 1.0;
    const int n = static_cast<int>(v);
    for (int i = 2; i <= n; ++i) r *= static_cast<double>(i);
    return r;
}

} // namespace (combinatorics helpers)

Value perms(const Value &v, std::pmr::memory_resource *mr)
{
    if (v.type() == ValueType::COMPLEX)
        throw Error("perms: complex inputs are not supported",
                     0, 0, "perms", "", "numkit:perms:complex");
    if (v.isEmpty()) {
        return Value::matrix(1, 0, ValueType::DOUBLE, mr);
    }
    if (!v.dims().isVector())
        throw Error("perms: argument must be a vector",
                     0, 0, "perms", "", "numkit:perms:notVector");

    const size_t n = v.numel();
    if (n > static_cast<size_t>(kPermMaxN))
        throw Error("perms: numel(v) > 11 is not supported (n! is too large)",
                     0, 0, "perms", "", "numkit:perms:tooLarge");

    ScratchArena scratch(mr);
    auto vals = ScratchVec<double>(n, &scratch);
    for (size_t i = 0; i < n; ++i)
        vals[i] = v.elemAsDouble(i);

    ScratchVec<double> cur(vals, &scratch);
    std::sort(cur.begin(), cur.end(), std::greater<double>());

    const size_t totalRows = static_cast<size_t>(permFactorial(static_cast<int>(n)));
    auto out = Value::matrix(totalRows, n, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();

    size_t row = 0;
    do {
        for (size_t c = 0; c < n; ++c)
            dst[c * totalRows + row] = cur[c];
        ++row;
    } while (std::prev_permutation(cur.begin(), cur.end()));

    return out;
}

Value factorial(const Value &n, std::pmr::memory_resource *mr)
{
    if (n.type() == ValueType::COMPLEX)
        throw Error("factorial: complex inputs are not supported",
                     0, 0, "factorial", "", "numkit:factorial:complex");
    auto out = createLike(n, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t N = n.numel();
    for (size_t i = 0; i < N; ++i)
        dst[i] = factorialDouble(n.elemAsDouble(i), "factorial");
    return out;
}

Value nchoosek(double n, double k, std::pmr::memory_resource *mr)
{
    if (!std::isfinite(n) || !std::isfinite(k))
        throw Error("nchoosek: arguments must be finite",
                     0, 0, "nchoosek", "", "numkit:nchoosek:badArg");
    if (n < 0 || k < 0 || n != std::floor(n) || k != std::floor(k))
        throw Error("nchoosek: arguments must be non-negative integers",
                     0, 0, "nchoosek", "", "numkit:nchoosek:badArg");
    if (k > n)
        throw Error("nchoosek: k must satisfy 0 ≤ k ≤ n",
                     0, 0, "nchoosek", "", "numkit:nchoosek:kTooLarge");

    double kk = (k > n - k) ? n - k : k;
    if (kk == 0.0)
        return Value::scalar(1.0, mr);

    double r = 1.0;
    const int kInt = static_cast<int>(kk);
    for (int i = 0; i < kInt; ++i) {
        r = r * (n - static_cast<double>(i)) / static_cast<double>(i + 1);
    }
    return Value::scalar(std::round(r), mr);
}

// nchoosek(v, k) where v is a vector: all k-combinations of the elements of v,
// one per ROW, in lexicographic order of element indices (MATLAB R2025b):
// nchoosek([1 2 3 4],2) = [1 2;1 3;1 4;2 3;2 4;3 4]. k==0 -> 1x0; k==numel ->
// a single row of all elements. Result is DOUBLE (numeric input flattened).
Value nchoosekCombinations(const Value &v, double kd, std::pmr::memory_resource *mr)
{
    const size_t n = v.numel();
    if (!std::isfinite(kd) || kd < 0 || kd != std::floor(kd))
        throw Error("nchoosek: K must be a non-negative integer",
                     0, 0, "nchoosek", "", "numkit:nchoosek:badArg");
    const size_t k = static_cast<size_t>(kd);
    if (k > n)
        throw Error("nchoosek: K must satisfy 0 <= K <= numel(V)",
                     0, 0, "nchoosek", "", "numkit:nchoosek:kTooLarge");
    if (k == 0)
        return Value::matrix(1, 0, ValueType::DOUBLE, mr);

    ScratchArena scratch(mr);
    auto vd = ScratchVec<double>(n, &scratch);
    for (size_t i = 0; i < n; ++i) vd[i] = v.elemAsDouble(i);

    // Row count R = C(n,k) (computed with the symmetric product to limit error).
    const size_t kk = (k > n - k) ? n - k : k;
    double rd = 1.0;
    for (size_t i = 0; i < kk; ++i)
        rd = rd * static_cast<double>(n - i) / static_cast<double>(i + 1);
    const size_t R = static_cast<size_t>(std::round(rd));

    auto out = Value::matrix(R, k, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    auto idx = ScratchVec<size_t>(k, &scratch);
    for (size_t i = 0; i < k; ++i) idx[i] = i;
    size_t row = 0;
    while (true) {
        for (size_t j = 0; j < k; ++j) od[j * R + row] = vd[idx[j]];
        ++row;
        // Advance to the next lexicographic combination of indices.
        size_t i = k;
        while (i-- > 0) {
            if (idx[i] != n - k + i) {
                ++idx[i];
                for (size_t j = i + 1; j < k; ++j) idx[j] = idx[j - 1] + 1;
                break;
            }
            if (i == 0) { row = R; }   // exhausted (sentinel: stop outer loop)
        }
        if (row >= R) break;
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════════
// Pack 16: setxor / allunique / numunique / ismembertol / uniquetol
// ════════════════════════════════════════════════════════════════════════
//
// Builds on the same hash-set / sort+merge primitives used by `unique` /
// `union` / `intersect`. The tolerant variants do an O(N²) sweep — fine
// for the typical cell sizes (≤ 1e4) where these get called.

Value setxor(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    // Symmetric difference: { x : x ∈ A xor x ∈ B }.
    if (a.numel() == 0 && b.numel() == 0) return emptyRow(mr);

    ScratchArena scratch(mr);
    std::pmr::unordered_set<double, DoubleHashEq0> sa(&scratch), sb(&scratch);
    sa.reserve(a.numel());
    sb.reserve(b.numel());
    for (size_t i = 0; i < a.numel(); ++i) sa.insert(a.doubleData()[i]);
    for (size_t i = 0; i < b.numel(); ++i) sb.insert(b.doubleData()[i]);

    auto out = ScratchVec<double>(&scratch);
    out.reserve(sa.size() + sb.size());
    for (double v : sa) if (!sb.count(v)) out.push_back(v);
    for (double v : sb) if (!sa.count(v)) out.push_back(v);
    std::sort(out.begin(), out.end());
    return rowFromVec(out.data(), out.size(), mr);
}

Value allunique(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t n = x.numel();
    if (n <= 1) return Value::logicalScalar(true, mr);
    ScratchArena scratch(mr);
    std::pmr::unordered_set<double, DoubleHashEq0> seen(&scratch);
    seen.reserve(n);
    const double *p = x.doubleData();
    for (size_t i = 0; i < n; ++i) {
        // NaN compares unequal to itself; MATLAB treats two NaNs as
        // distinct in allunique, so we let them all pass through.
        if (std::isnan(p[i])) continue;
        if (!seen.insert(p[i]).second)
            return Value::logicalScalar(false, mr);
    }
    return Value::logicalScalar(true, mr);
}

Value numunique(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t n = x.numel();
    if (n == 0) return Value::scalar(0.0, mr);
    ScratchArena scratch(mr);
    std::pmr::unordered_set<double, DoubleHashEq0> seen(&scratch);
    seen.reserve(n);
    size_t nanCount = 0;
    const double *p = x.doubleData();
    for (size_t i = 0; i < n; ++i) {
        if (std::isnan(p[i])) ++nanCount;
        else seen.insert(p[i]);
    }
    return Value::scalar(static_cast<double>(seen.size() + nanCount), mr);
}

namespace {
inline bool nearlyEqualTol(double x, double y, double tol)
{
    if (x == y) return true;
    if (std::isnan(x) || std::isnan(y)) return false;
    const double s = std::max(std::abs(x), std::abs(y));
    return std::abs(x - y) <= tol * std::max(1.0, s);
}
} // anon

Value ismembertol(const Value &a, const Value &s, double tol, std::pmr::memory_resource *mr)
{
    // Returns logical of size(a). For each a[i], true if there exists
    // s[j] with |a[i] - s[j]| ≤ tol * max(1, |a|, |s|). Naive O(|a||s|).
    auto r = createLike(a, ValueType::LOGICAL, mr);
    const size_t na = a.numel(), ns = s.numel();
    const double *pa = a.doubleData();
    const double *ps = s.doubleData();
    for (size_t i = 0; i < na; ++i) {
        bool hit = false;
        for (size_t j = 0; j < ns; ++j) {
            if (nearlyEqualTol(pa[i], ps[j], tol)) { hit = true; break; }
        }
        r.logicalDataMut()[i] = hit ? 1 : 0;
    }
    return r;
}

Value uniquetol(const Value &x, double tol, std::pmr::memory_resource *mr)
{
    // MATLAB convention (doc uniquetol): two values u and v are within
    // tolerance iff
    //     |u - v| <= tol * max(|A(:)|)
    // i.e. an ABSOLUTE global tolerance scaled by the input's largest
    // magnitude. Cluster representatives are kept in input/sort order
    // and a candidate joins an existing cluster iff it is within
    // tol*DS of the cluster's representative (i.e. the previously
    // emitted unique value, since input is sorted ascending).
    //
    // Per-pair relative scaling (the previous numkit behaviour) gave
    // wildly different cluster counts on dense inputs because the
    // scale shifted with each comparison.
    const size_t n = x.numel();
    if (n == 0) return emptyRow(mr);
    ScratchArena scratch(mr);
    auto vals = ScratchVec<double>(&scratch);
    vals.reserve(n);
    const double *p = x.doubleData();
    double dataScale = 0.0;
    for (size_t i = 0; i < n; ++i) {
        vals.push_back(p[i]);
        if (!std::isnan(p[i])) {
            const double m = std::abs(p[i]);
            if (m > dataScale) dataScale = m;
        }
    }
    std::sort(vals.begin(), vals.end(),
              [](double a, double b) {
                  if (std::isnan(b)) return !std::isnan(a);
                  if (std::isnan(a)) return false;
                  return a < b;
              });
    const double tolAbs = tol * dataScale;
    auto out = ScratchVec<double>(&scratch);
    out.reserve(n);
    for (double v : vals) {
        if (std::isnan(v)) {
            // NaNs are unique to themselves -- always emit.
            out.push_back(v);
            continue;
        }
        if (out.empty() || std::isnan(out.back())
            || std::abs(v - out.back()) > tolAbs) {
            out.push_back(v);
        }
    }
    return rowFromVec(out.data(), out.size(), mr);
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════════
namespace detail {

// Reshape a 1-D result to the requested orientation (column or row),
// preserving element order and type. MATLAB's unique returns ia/ic always as
// column vectors and the unique values matching the input orientation.
static Value orientUniqueVec(const Value &v, bool column,
                             std::pmr::memory_resource *mr)
{
    const size_t k = v.numel();
    if (k == 0) return v; // leave empties untouched
    const bool isCol = (v.dims().cols() == 1 && v.dims().rows() == k);
    if (column == isCol) return v; // already in the desired orientation
    Value out = Value::matrix(column ? k : 1, column ? 1 : k, v.type(), mr);
    if (v.type() == ValueType::COMPLEX)
        std::copy(v.complexData(), v.complexData() + k, out.complexDataMut());
    else
        std::copy(v.doubleData(), v.doubleData() + k, out.doubleDataMut());
    return out;
}

void unique_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("unique: requires 1 argument",
                     0, 0, "unique", "", "numkit:unique:nargin");
    auto *mr = ctx.engine->resource();

    // Unique values are a row vector only when the input is a row vector;
    // a column vector or matrix input produces a column.
    const bool cIsRow =
        !args[0].dims().is3D() && args[0].dims().rows() == 1;

    bool useRows = false;
    bool stable  = false;
    for (size_t i = 1; i < args.size(); ++i) {
        const Value &a = args[i];
        if (a.type() != ValueType::CHAR)
            throw Error("unique: extra arguments must be string flags",
                         0, 0, "unique", "", "numkit:unique:badArg");
        std::string s = a.toString();
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (s == "rows") useRows = true;
        else if (s == "stable") stable = true;
        else if (s == "sorted") stable = false;
        else if (s == "first" || s == "last") {
            // occurrence selector — accepted but no-op (first is the default).
        } else {
            throw Error("unique: unknown flag '" + s + "'",
                         0, 0, "unique", "", "numkit:unique:badFlag");
        }
    }

    if (useRows) {
        // 'rows' + 'stable' not yet combined; rows path stays sorted. C is a
        // matrix of unique rows; ia/ic are column vectors.
        if (nargout <= 1) { outs[0] = uniqueRows(args[0], mr); return; }
        auto [c, ia, ic] = uniqueRowsWithIndices(args[0], mr);
        outs[0] = std::move(c);
        if (nargout > 1) outs[1] = orientUniqueVec(ia, /*column=*/true, mr);
        if (nargout > 2) outs[2] = orientUniqueVec(ic, /*column=*/true, mr);
        return;
    }

    if (nargout <= 1) {
        outs[0] = orientUniqueVec(unique(args[0], mr, stable), !cIsRow, mr);
        return;
    }
    auto [c, ia, ic] = uniqueWithIndices(args[0], mr, stable);
    outs[0] = orientUniqueVec(c, !cIsRow, mr);
    if (nargout > 1) outs[1] = orientUniqueVec(ia, /*column=*/true, mr);
    if (nargout > 2) outs[2] = orientUniqueVec(ic, /*column=*/true, mr);
}

#define NK_BIN_SETOP_REG(name, fn)                                             \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,               \
                    Span<Value> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.size() < 2)                                                   \
            throw Error(#name ": requires 2 arguments",                       \
                         0, 0, #name, "", "numkit:" #name ":nargin");               \
        outs[0] = fn(args[0], args[1], ctx.engine->resource());               \
    }

// histcounts(x, edges[, name, value...]): bin counts, optionally normalized.
// Edges may be passed positionally (histcounts(x, edges)) or via the
// 'BinEdges' name-value pair (histcounts(x, 'BinEdges', edges)). The second
// output returns the bin edges as a row vector: [n, e] = histcounts(...).
// 'Normalization' mode ∈ {count, probability, countdensity, pdf, cumcount,
// cdf}. Automatic binning (nbins / 'BinWidth' / 'BinLimits' / 'BinMethod')
// is not supported — edges must be given explicitly.
void histcounts_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("histcounts: requires at least 2 arguments",
                     0, 0, "histcounts", "", "numkit:histcounts:nargin");
    auto *mr = ctx.engine->resource();

    HistNorm norm = HistNorm::Count;
    Value edges = Value::Empty;
    bool haveEdges = false;

    // A non-char second argument is the positional edges vector; otherwise
    // every trailing argument is a name-value pair (incl. 'BinEdges').
    size_t optStart = 1;
    if (args[1].type() != ValueType::CHAR) {
        edges = args[1];
        haveEdges = true;
        optStart = 2;
    }

    for (size_t i = optStart; i + 1 < args.size(); ++i) {
        if (args[i].type() != ValueType::CHAR) continue;
        std::string key = args[i].toString();
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (key == "normalization") {
            if (args[i + 1].type() != ValueType::CHAR)
                throw Error("histcounts: 'Normalization' value must be a string",
                             0, 0, "histcounts", "", "numkit:histcounts:badNorm");
            std::string m = args[i + 1].toString();
            std::transform(m.begin(), m.end(), m.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if      (m == "count")        norm = HistNorm::Count;
            else if (m == "probability")  norm = HistNorm::Probability;
            else if (m == "countdensity") norm = HistNorm::CountDensity;
            else if (m == "pdf")          norm = HistNorm::Pdf;
            else if (m == "cumcount")     norm = HistNorm::CumCount;
            else if (m == "cdf")          norm = HistNorm::Cdf;
            else
                throw Error("histcounts: unknown Normalization '" + m + "'",
                             0, 0, "histcounts", "", "numkit:histcounts:badNorm");
            ++i;   // consume the value
        } else if (key == "binedges") {
            edges = args[i + 1];
            haveEdges = true;
            ++i;   // consume the value
        } else {
            throw Error("histcounts: option '" + args[i].toString() +
                            "' not supported (use explicit edges or 'BinEdges')",
                         0, 0, "histcounts", "", "numkit:histcounts:badOption");
        }
    }

    if (!haveEdges)
        throw Error("histcounts: bin edges required — automatic binning "
                     "(nbins / 'BinWidth' / 'BinLimits') is not supported",
                     0, 0, "histcounts", "", "numkit:histcounts:noEdges");

    outs[0] = histcounts(args[0], edges, norm, mr);

    // [n, edges] = histcounts(...): return the edges as a row vector.
    if (nargout >= 2) {
        validateEdges(edges, "histcounts");
        const size_t ne = edges.numel();
        auto e = Value::matrix(1, ne, ValueType::DOUBLE, mr);
        const double *src = edges.doubleData();
        std::copy(src, src + ne, e.doubleDataMut());
        outs[1] = e;
    }
}

// histc(x, edges): legacy bin counts (length(edges) bins, last = exact
// equal to edges(end)). [n, bin] = histc(...) also returns the 1-based bin
// index of each element (0 if out of range), same shape as x.
void histc_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("histc: requires (x, edges)",
                     0, 0, "histc", "", "numkit:histc:nargin");
    auto *mr = ctx.engine->resource();
    outs[0] = histc(args[0], args[1], mr);
    if (nargout > 1) {
        const Value &x = args[0], &edges = args[1];
        const std::size_t nE = edges.numel();
        const double *e = edges.doubleData();
        Value binOut = createLike(x, ValueType::DOUBLE, mr);
        double *bd = binOut.doubleDataMut();
        const double *p = x.doubleData();
        const std::size_t n = x.numel();
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t b = histcBin(p[i], e, nE);
            bd[i] = (b == SIZE_MAX) ? 0.0 : static_cast<double>(b + 1);
        }
        outs[1] = std::move(binOut);
    }
}

// ismember(a,b): tf membership mask; [tf,loc] = ismember(...) also returns
// loc(i) = the LOWEST 1-based index of a(i) in b (0 if absent), per MATLAB.
void ismember_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ismember: requires 2 arguments", 0, 0, "ismember", "", "numkit:ismember:nargin");
    auto *mr = ctx.engine->resource();
    if (args[0].type() == ValueType::COMPLEX || args[1].type() == ValueType::COMPLEX) {
        auto [tf, loc] = ismemberComplex(args[0], args[1], /*wantLoc=*/nargout > 1, mr);
        outs[0] = std::move(tf);
        if (nargout > 1) outs[1] = std::move(loc);
        return;
    }
    outs[0] = ismember(args[0], args[1], mr);
    if (nargout > 1) {
        const Value &a = args[0], &b = args[1];
        ScratchArena scratch(mr);
        std::pmr::unordered_map<double, double, DoubleHashEq0> idxB(&scratch);
        idxB.reserve(b.numel());
        const double *pb = b.doubleData();
        for (size_t i = 0; i < b.numel(); ++i)
            if (!std::isnan(pb[i]))
                idxB.emplace(pb[i], static_cast<double>(i + 1));  // emplace keeps the lowest index
        Value loc = createLike(a, ValueType::DOUBLE, mr);
        double *lo = loc.doubleDataMut();
        const double *pa = a.doubleData();
        for (size_t i = 0; i < a.numel(); ++i) {
            const double v = pa[i];
            auto it = std::isnan(v) ? idxB.end() : idxB.find(v);
            lo[i] = (it != idxB.end()) ? it->second : 0.0;
        }
        outs[1] = std::move(loc);
    }
}

// union / intersect / setdiff accept a trailing 'sorted' (default) or
// 'stable' setOrder flag; 'stable' keeps first-occurrence (A-then-B) order.
namespace {
bool wantsStable(Span<const Value> args, size_t start)
{
    for (size_t i = start; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            std::string s = args[i].toString();
            for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (s == "stable") return true;
        }
    }
    return false;
}

// ── set-operation index outputs (ia / ib) ──────────────────────────────
enum class SetOpKind { Intersect, Setdiff, Union };

void buildFirstIndexMap(const Value &x,
                        std::pmr::unordered_map<double, size_t, DoubleHashEq0> &m)
{
    const size_t n = x.numel();
    for (size_t i = 0; i < n; ++i) {
        const double v = x.elemAsDouble(i);
        if (!std::isnan(v)) m.try_emplace(v, i + 1); // 1-based first occurrence
    }
}

// 1-based index of value v in x (finite via map, NaN via linear scan); 0 if absent.
size_t setopFirstIndex(const Value &x,
                       const std::pmr::unordered_map<double, size_t, DoubleHashEq0> &m,
                       double v)
{
    if (std::isnan(v)) {
        const size_t n = x.numel();
        for (size_t i = 0; i < n; ++i)
            if (std::isnan(x.elemAsDouble(i))) return i + 1;
        return 0;
    }
    auto it = m.find(v);
    return it == m.end() ? 0 : it->second;
}

// Emit ia (and ib for intersect/union) for a set operation, matching MATLAB:
// the index vectors are always columns. intersect/setdiff: ia indexes A,
// ib indexes B; union: ia indexes the A-sourced result elements, ib the
// B-only ones.
void emitSetopIndices(SetOpKind kind, const Value &A, const Value &B,
                      const Value &result, size_t nargout, Span<Value> outs,
                      std::pmr::memory_resource *mr, const char *fn)
{
    if (A.type() == ValueType::COMPLEX || B.type() == ValueType::COMPLEX)
        throw Error(std::string(fn) + ": index outputs are not supported for "
                    "complex inputs", 0, 0, fn, "",
                    std::string("numkit:") + fn + ":complexIdx");

    ScratchArena scratch(mr);
    std::pmr::unordered_map<double, size_t, DoubleHashEq0> mapA(&scratch);
    buildFirstIndexMap(A, mapA);

    if (kind == SetOpKind::Union) {
        std::pmr::unordered_map<double, size_t, DoubleHashEq0> mapB(&scratch);
        buildFirstIndexMap(B, mapB);
        auto iaVec = ScratchVec<double>(&scratch);
        auto ibVec = ScratchVec<double>(&scratch);
        const size_t k = result.numel();
        for (size_t i = 0; i < k; ++i) {
            const double v = result.elemAsDouble(i);
            const size_t ai = setopFirstIndex(A, mapA, v);
            if (ai != 0) iaVec.push_back(static_cast<double>(ai));
            else ibVec.push_back(static_cast<double>(setopFirstIndex(B, mapB, v)));
        }
        auto colOf = [&](const ScratchVec<double> &v) {
            Value c = Value::matrix(v.size(), 1, ValueType::DOUBLE, mr);
            if (!v.empty()) std::copy(v.begin(), v.end(), c.doubleDataMut());
            return c;
        };
        if (nargout >= 2) outs[1] = colOf(iaVec);
        if (nargout >= 3) outs[2] = colOf(ibVec);
        return;
    }

    const size_t k = result.numel();
    Value ia = Value::matrix(k, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < k; ++i)
        ia.doubleDataMut()[i] =
            static_cast<double>(setopFirstIndex(A, mapA, result.elemAsDouble(i)));
    if (nargout >= 2) outs[1] = ia;

    if (kind == SetOpKind::Intersect && nargout >= 3) {
        std::pmr::unordered_map<double, size_t, DoubleHashEq0> mapB(&scratch);
        buildFirstIndexMap(B, mapB);
        Value ib = Value::matrix(k, 1, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < k; ++i)
            ib.doubleDataMut()[i] =
                static_cast<double>(setopFirstIndex(B, mapB, result.elemAsDouble(i)));
        outs[2] = ib;
    }
}

// True if a trailing 'rows' flag is present (case-insensitive).
bool wantsRows(Span<const Value> args, size_t start)
{
    for (size_t i = start; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            std::string s = args[i].toString();
            for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (s == "rows") return true;
        }
    }
    return false;
}

// Row-wise union/intersect/setdiff (MATLAB ..(A,B,'rows')). Treats each row
// as an element; the result is the sorted set of unique rows (NaN-containing
// rows are distinct and sort last, matching unique('rows')). 2-D DOUBLE only.
Value setOpRows(const Value &A, const Value &B, SetOpKind kind,
                const char *fn, std::pmr::memory_resource *mr)
{
    validateUniqueRowsInput(A, fn);
    validateUniqueRowsInput(B, fn);
    const size_t ar = A.dims().rows(), ac = A.dims().cols();
    const size_t br = B.dims().rows(), bc = B.dims().cols();
    if (ar > 0 && br > 0 && ac != bc)
        throw Error(std::string(fn) + ": 'rows' inputs must have the same "
                    "number of columns", 0, 0, fn, "",
                    std::string("numkit:") + fn + ":rowsCols");
    const size_t cols = (ar > 0) ? ac : bc;

    // union(A,B,'rows') == unique rows of the vertical concatenation [A; B].
    if (kind == SetOpKind::Union) {
        const size_t nr = ar + br;
        if (nr == 0) return emptyRowsResult(cols, mr);
        auto combined = Value::matrix(nr, cols, ValueType::DOUBLE, mr);
        double *cd = combined.doubleDataMut();
        const double *ad = (ar > 0) ? A.doubleData() : nullptr;
        const double *bd = (br > 0) ? B.doubleData() : nullptr;
        for (size_t c = 0; c < cols; ++c) {
            for (size_t r = 0; r < ar; ++r)      cd[c * nr + r]      = ad[c * ar + r];
            for (size_t r = 0; r < br; ++r)      cd[c * nr + ar + r] = bd[c * br + r];
        }
        return uniqueRows(combined, mr);
    }

    // intersect / setdiff: unique rows of A that ARE / ARE NOT present in B.
    const bool wantInB = (kind == SetOpKind::Intersect);
    if (ar == 0) return emptyRowsResult(cols, mr);
    const double *ad = A.doubleData();
    const double *bd = (br > 0) ? B.doubleData() : nullptr;

    ScratchArena scratch(mr);
    std::pmr::unordered_map<RowKey, char, RowKeyHash, RowKeyEq> bset(&scratch);
    for (size_t r = 0; r < br; ++r)
        if (!rowHasNan(bd, bc, br, r))
            bset.try_emplace(extractRow(bd, bc, br, r, &scratch), char{1});

    std::pmr::unordered_map<RowKey, char, RowKeyHash, RowKeyEq> seen(&scratch);
    auto nonNan = ScratchVec<size_t>(&scratch);
    auto nanIdx = ScratchVec<size_t>(&scratch);
    for (size_t r = 0; r < ar; ++r) {
        if (rowHasNan(ad, ac, ar, r)) {
            // NaN row: never equal to anything, so never "in B".
            if (!wantInB) nanIdx.push_back(r);  // setdiff keeps; intersect drops
            continue;
        }
        RowKey key = extractRow(ad, ac, ar, r, &scratch);
        if ((bset.count(key) > 0) != wantInB) continue;
        if (seen.try_emplace(std::move(key), char{1}).second) nonNan.push_back(r);
    }
    std::sort(nonNan.begin(), nonNan.end(),
              [ad, ac, ar](size_t a, size_t b) { return rowLexCmp(ad, ac, ar, a, b) < 0; });
    nonNan.insert(nonNan.end(), nanIdx.begin(), nanIdx.end());
    if (nonNan.empty()) return emptyRowsResult(cols, mr);
    return detail::collectRowsByIndex(mr, A, nonNan.data(), nonNan.size());
}
} // namespace

void union_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("union: requires 2 arguments", 0, 0, "union", "", "numkit:union:nargin");
    auto *mr = ctx.engine->resource();
    if (wantsRows(args, 2)) {
        if (nargout >= 2)
            throw Error("union: 'rows' index outputs (ia, ib) are not yet "
                        "supported in this revision", 0, 0, "union", "",
                        "numkit:union:rowsIdx");
        outs[0] = setOpRows(args[0], args[1], SetOpKind::Union, "union", mr);
        return;
    }
    outs[0] = setUnion(args[0], args[1], mr, wantsStable(args, 2));
    if (nargout >= 2)
        emitSetopIndices(SetOpKind::Union, args[0], args[1], outs[0], nargout, outs,
                         mr, "union");
}

void intersect_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("intersect: requires 2 arguments", 0, 0, "intersect", "", "numkit:intersect:nargin");
    auto *mr = ctx.engine->resource();
    if (wantsRows(args, 2)) {
        if (nargout >= 2)
            throw Error("intersect: 'rows' index outputs (ia, ib) are not yet "
                        "supported in this revision", 0, 0, "intersect", "",
                        "numkit:intersect:rowsIdx");
        outs[0] = setOpRows(args[0], args[1], SetOpKind::Intersect, "intersect", mr);
        return;
    }
    outs[0] = setIntersect(args[0], args[1], mr, wantsStable(args, 2));
    if (nargout >= 2)
        emitSetopIndices(SetOpKind::Intersect, args[0], args[1], outs[0], nargout, outs,
                         mr, "intersect");
}

void setdiff_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("setdiff: requires 2 arguments", 0, 0, "setdiff", "", "numkit:setdiff:nargin");
    auto *mr = ctx.engine->resource();
    if (wantsRows(args, 2)) {
        if (nargout >= 2)
            throw Error("setdiff: 'rows' index output (ia) is not yet "
                        "supported in this revision", 0, 0, "setdiff", "",
                        "numkit:setdiff:rowsIdx");
        outs[0] = setOpRows(args[0], args[1], SetOpKind::Setdiff, "setdiff", mr);
        return;
    }
    outs[0] = setDiff(args[0], args[1], mr, wantsStable(args, 2));
    if (nargout >= 2)
        emitSetopIndices(SetOpKind::Setdiff, args[0], args[1], outs[0], nargout, outs,
                         mr, "setdiff");
}
NK_BIN_SETOP_REG(discretize, discretize)

#undef NK_BIN_SETOP_REG

void primes_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("primes: requires 1 argument",
                     0, 0, "primes", "", "numkit:primes:nargin");
    outs[0] = primes(args[0].toScalar(), ctx.engine->resource());
}

void isprime_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isprime: requires 1 argument",
                     0, 0, "isprime", "", "numkit:isprime:nargin");
    outs[0] = isprime(args[0], ctx.engine->resource());
}

void factor_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("factor: requires 1 argument",
                     0, 0, "factor", "", "numkit:factor:nargin");
    if (!args[0].isScalar())
        throw Error("factor: argument must be a scalar",
                     0, 0, "factor", "", "numkit:factor:notScalar");
    outs[0] = factor(args[0].toScalar(), ctx.engine->resource());
}

void perms_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("perms: requires 1 argument",
                     0, 0, "perms", "", "numkit:perms:nargin");
    outs[0] = perms(args[0], ctx.engine->resource());
}

void factorial_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("factorial: requires 1 argument",
                     0, 0, "factorial", "", "numkit:factorial:nargin");
    outs[0] = factorial(args[0], ctx.engine->resource());
}

void nchoosek_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("nchoosek: requires 2 arguments (n, k)",
                     0, 0, "nchoosek", "", "numkit:nchoosek:nargin");
    auto *mr = ctx.engine->resource();
    // Scalar N -> binomial coefficient; vector V -> all K-combinations (rows).
    if (args[0].isScalar())
        outs[0] = nchoosek(args[0].toScalar(), args[1].toScalar(), mr);
    else
        outs[0] = nchoosekCombinations(args[0], args[1].toScalar(), mr);
}

// ── Pack 16 adapters ─────────────────────────────────────────────────

void setxor_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("setxor: requires 2 arguments",
                     0, 0, "setxor", "", "numkit:setxor:nargin");
    outs[0] = setxor(args[0], args[1], ctx.engine->resource());
}

void allunique_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.empty())
        throw Error("allunique: requires 1 argument",
                     0, 0, "allunique", "", "numkit:allunique:nargin");
    outs[0] = allunique(args[0], ctx.engine->resource());
}

void numunique_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.empty())
        throw Error("numunique: requires 1 argument",
                     0, 0, "numunique", "", "numkit:numunique:nargin");
    outs[0] = numunique(args[0], ctx.engine->resource());
}

void ismembertol_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                     CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ismembertol: requires (A, S, [tol])",
                     0, 0, "ismembertol", "", "numkit:ismembertol:nargin");
    double tol = (args.size() >= 3 && !args[2].isEmpty())
                     ? args[2].toScalar()
                     : 1e-6;
    outs[0] = ismembertol(args[0], args[1], tol, ctx.engine->resource());
}

void uniquetol_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.empty())
        throw Error("uniquetol: requires (A, [tol])",
                     0, 0, "uniquetol", "", "numkit:uniquetol:nargin");
    double tol = (args.size() >= 2 && !args[1].isEmpty())
                     ? args[1].toScalar()
                     : 1e-6;
    outs[0] = uniquetol(args[0], tol, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
