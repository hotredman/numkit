// toolboxes/builtin/src/math/elementary/discrete.cpp
//
// Discrete-math builtins. Three legacy TUs were merged here, separated
// by section headers: set operations, number theory, combinatorics.

#include <numkit/builtin/math/discrete/discrete.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

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

#include "discrete_detail.hpp"

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Set operations
// ════════════════════════════════════════════════════════════════════════
//
// Inputs flatten to a vector (column-major) before processing; outputs
// are 1×N row vectors of sorted unique values for the set ops, ismember
// preserves the shape of A, histcounts/discretize work on the flat input.


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
uniqueWithIndices(const Value &x, std::pmr::memory_resource *mr, bool stable,
                  bool last)
{
    const size_t n = x.numel();
    if (n == 0) {
        return std::make_tuple(emptyRow(mr), emptyRow(mr),
                               emptyRow(mr));
    }
    if (x.type() == ValueType::COMPLEX)
        return uniqueComplexFull(x, mr, stable, last);

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
        } else if (last) {
            firstIdx[p[i]] = i;            // 'last': keep the last occurrence
        } else {
            firstIdx.try_emplace(p[i], i); // default: keep the first occurrence
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

Value uniqueRows(const Value &x, std::pmr::memory_resource *mr, bool stable)
{
    validateUniqueRowsInput(x, "unique");
    const size_t rows = x.dims().rows();
    const size_t cols = x.dims().cols();
    if (rows == 0) return emptyRowsResult(cols, mr);

    const double *src = x.doubleData();
    ScratchArena scratch(mr);

    if (stable) {
        // 'rows','stable': keep the first occurrence of each distinct row in
        // appearance order (NaN-containing rows are each distinct — NaN never
        // equals itself — so they are always kept, interleaved in place).
        std::pmr::unordered_map<RowKey, size_t, RowKeyHash, RowKeyEq> seen(&scratch);
        seen.reserve(rows);
        auto uniqRows = ScratchVec<size_t>(&scratch);
        for (size_t r = 0; r < rows; ++r) {
            if (rowHasNan(src, cols, rows, r)) {
                uniqRows.push_back(r);
            } else if (seen.try_emplace(extractRow(src, cols, rows, r, &scratch), r).second) {
                uniqRows.push_back(r);
            }
        }
        return detail::collectRowsByIndex(mr, x, uniqRows.data(), uniqRows.size());
    }

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
uniqueRowsWithIndices(const Value &x, std::pmr::memory_resource *mr, bool stable,
                      bool last)
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

    if (stable) {
        // 'rows','stable': first occurrences in appearance order. ia indexes
        // those first occurrences; ic maps every row back to its unique entry.
        // NaN-containing rows are each distinct (kept in place).
        std::pmr::unordered_map<RowKey, size_t, RowKeyHash, RowKeyEq> posByKey(&scratch);
        std::pmr::unordered_map<size_t, size_t> posByNanRow(&scratch);
        posByKey.reserve(rows);
        auto uniqRows = ScratchVec<size_t>(&scratch);
        for (size_t r = 0; r < rows; ++r) {
            if (rowHasNan(src, cols, rows, r)) {
                posByNanRow[r] = uniqRows.size();
                uniqRows.push_back(r);
            } else if (posByKey.try_emplace(extractRow(src, cols, rows, r, &scratch),
                                            uniqRows.size()).second) {
                uniqRows.push_back(r);
            }
        }

        auto icRow = Value::matrix(rows, 1, ValueType::DOUBLE, mr);
        double *ic = icRow.doubleDataMut();
        for (size_t r = 0; r < rows; ++r) {
            const size_t pos = rowHasNan(src, cols, rows, r)
                ? posByNanRow[r]
                : posByKey[extractRow(src, cols, rows, r, &scratch)];
            ic[r] = static_cast<double>(pos + 1);
        }

        auto iaCol = Value::matrix(uniqRows.size(), 1, ValueType::DOUBLE, mr);
        double *ia = iaCol.doubleDataMut();
        for (size_t i = 0; i < uniqRows.size(); ++i)
            ia[i] = static_cast<double>(uniqRows[i] + 1);

        return std::make_tuple(
            detail::collectRowsByIndex(mr, x, uniqRows.data(), uniqRows.size()),
            std::move(iaCol), std::move(icRow));
    }

    std::pmr::unordered_map<RowKey, size_t, RowKeyHash, RowKeyEq> firstIdx(&scratch);
    firstIdx.reserve(rows);
    auto nanRowOrder = ScratchVec<size_t>(&scratch);
    for (size_t r = 0; r < rows; ++r) {
        if (rowHasNan(src, cols, rows, r)) {
            nanRowOrder.push_back(r);
        } else if (last) {
            firstIdx[extractRow(src, cols, rows, r, &scratch)] = r;  // 'last'
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

} // namespace numkit::builtin
