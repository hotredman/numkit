// toolboxes/.../discrete_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by discrete.cpp + discrete_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include "reduction_helpers.hpp"  // engine-free numkit::builtin::detail dim-infra (ops re-export)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::math {

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
uniqueComplexFull(const Value &x, std::pmr::memory_resource *mr, bool stable,
                  bool last = false)
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
        else { ic0[i] = found; if (last) uniq[found].firstIdx = i; }  // 'last': keep last occurrence
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
namespace {
inline bool nearlyEqualTol(double x, double y, double tol)
{
    if (x == y) return true;
    if (std::isnan(x) || std::isnan(y)) return false;
    const double s = std::max(std::abs(x), std::abs(y));
    return std::abs(x - y) <= tol * std::max(1.0, s);
}
} // anon

// file-scope workers used by the reg (defs in discrete.cpp, external).
std::pair<Value, Value> ismemberComplex(const Value &a, const Value &b, bool wantLoc, std::pmr::memory_resource *mr);
Value nchoosekCombinations(const Value &v, double kd, std::pmr::memory_resource *mr);

} // namespace numkit::math
