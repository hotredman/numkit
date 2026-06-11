// toolboxes/builtin/src/math/elementary/reductions.cpp
//
// Reductions (sum / prod / mean / max / min — single-return forms,
// NaN-aware variants, complex variants) plus the typed-output dispatcher
// used by their adapters. Also hosts linspace / logspace.
//
// trigonometry / exponents / rounding / misc / special live in sibling
// files under math/elementary/. Random generators (rand/randn) live in
// math/random/rng.cpp.

#include <numkit/lang/arrays/matrix.hpp>       // reshape (for 'all')
#include <numkit/math/exp_log/exponents.hpp>      // exp / log adapters
#include <numkit/math/arithmetic/reductions.hpp>
#include <numkit/math/arithmetic/rounding.hpp>       // abs adapter
#include <numkit/math/trig/trigonometry.hpp>   // sin / cos adapters

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/helpers.hpp>
#include <numkit/ops/reductions.hpp>
#include "arithmetic/var_reduction.hpp"  // for sumScan + addInto
#include "../_unary_hint.hpp"  // 3-arg sin/cos/exp/log/abs hint overloads

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <string>
#include <type_traits>

#include "reductions_detail.hpp"

namespace numkit::math {

// ════════════════════════════════════════════════════════════════════════
// Reductions (single-return) — sum / prod / mean
// ════════════════════════════════════════════════════════════════════════

Value sum(const Value &x, std::pmr::memory_resource *mr)
{
    return reduce(x, [](double a, double b) { return a + b; }, 0.0, mr);
}

Value sum(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (dim <= 0) return sum(x, mr);
    const int d = numkit::ops::resolveDim(x, dim, "sum");

    // Phase P6 followup: 2D dim=2 column-pass row reduction. The
    // applyAlongDim path gathers each row into a scratch buffer with a
    // strided per-element copy (R reads at stride R per row, R rows ->
    // O(R^2) accesses with bad cache locality), then scalar-sums each
    // slice. Column-pass reads each input column contiguously and
    // accumulates into a row-totals vector with SIMD addInto. Net cost
    // floors at 1 read of M + 1 write of totals = roughly memory
    // bandwidth.
    if (d == 2 && x.type() == ValueType::DOUBLE && x.dims().ndim() == 2
        && !x.isScalar() && !x.dims().isVector()) {
        const size_t R = x.dims().rows(), C = x.dims().cols();
        auto r = Value::matrix(R, 1, ValueType::DOUBLE, mr);
        double *totals = r.doubleDataMut();
        std::fill(totals, totals + R, 0.0);
        const double *src = x.doubleData();
        for (size_t c = 0; c < C; ++c)
            addInto(totals, src + c * R, R);
        return r;
    }

    // Generic dim path (1D, 3D, dim=1, dim=3) — slice through scratch.
    return numkit::ops::applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            double acc = 0.0;
            for (size_t i = 0; i < n; ++i) acc += slice[i];
            return acc;
        }, mr);
}

Value prod(const Value &x, std::pmr::memory_resource *mr)
{
    return reduce(x, [](double a, double b) { return a * b; }, 1.0, mr);
}

Value prod(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (dim <= 0) return prod(x, mr);
    const int d = numkit::ops::resolveDim(x, dim, "prod");
    return numkit::ops::applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            double acc = 1.0;
            for (size_t i = 0; i < n; ++i) acc *= slice[i];
            return acc;
        }, mr);
}

Value mean(const Value &x, std::pmr::memory_resource *mr)
{
    return reduce(x, [](double a, double b) { return a + b; }, 0.0, mr, /*meanMode=*/true);
}

Value mean(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (dim <= 0) return mean(x, mr);
    const int d = numkit::ops::resolveDim(x, dim, "mean");
    return numkit::ops::applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            double acc = 0.0;
            for (size_t i = 0; i < n; ++i) acc += slice[i];
            return acc / static_cast<double>(n);
        }, mr);
}

// ── max/min with index ───────────────────────────────────────────────
//
// Per MATLAB semantics, min/max preserve the input element type
// (default 'native' mode). The value array is T-typed; the index
// array is always DOUBLE. COMPLEX inputs are rejected (no order
// defined on complex). Dispatch over ValueType picks the right T
// instantiation for DOUBLE, SINGLE, INT8..INT64, UINT8..UINT64,
// LOGICAL (storage = uint8) and CHAR (storage = char).


std::tuple<Value, Value> max(const Value &x, std::pmr::memory_resource *mr)
{
    return dispatchMinMaxAll<true>(x, [](auto v, auto best) { return v > best; }, mr, "max");
}

std::tuple<Value, Value> min(const Value &x, std::pmr::memory_resource *mr)
{
    return dispatchMinMaxAll<false>(x, [](auto v, auto best) { return v < best; }, mr, "min");
}

std::tuple<Value, Value> max(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (dim <= 0) return max(x, mr);
    const int d = numkit::ops::resolveDim(x, dim, "max");
    return dispatchMinMaxAlongDim<true>(x, d, [](auto v, auto best) { return v > best; }, mr, "max");
}

std::tuple<Value, Value> maxOmitNan(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (dim <= 0)
        return dispatchMinMaxNanAll<true>(x, [](auto v, auto best) { return v > best; }, mr, "max");
    const int d = numkit::ops::resolveDim(x, dim, "max");
    return dispatchMinMaxNanAlongDim<true>(x, d, [](auto v, auto best) { return v > best; }, mr, "max");
}

std::tuple<Value, Value> minOmitNan(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (dim <= 0)
        return dispatchMinMaxNanAll<false>(x, [](auto v, auto best) { return v < best; }, mr, "min");
    const int d = numkit::ops::resolveDim(x, dim, "min");
    return dispatchMinMaxNanAlongDim<false>(x, d, [](auto v, auto best) { return v < best; }, mr, "min");
}

std::tuple<Value, Value> min(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (dim <= 0) return min(x, mr);
    const int d = numkit::ops::resolveDim(x, dim, "min");
    return dispatchMinMaxAlongDim<false>(x, d, [](auto v, auto best) { return v < best; }, mr, "min");
}

Value max(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    // Integer / single binary form: result follows MATLAB type promotion
    // (integer wins over double; single wins over double; same-class
    // integers stay; mixed-class integers throw).
    {
        auto r = dispatchIntegerBinaryOp(a, b,
            [](auto x, auto y) { return x > y ? x : y; }, p);
        if (!r.isUnset()) return r;
    }
    // fmax (not std::max) ignores NaN like MATLAB: max([NaN 6],[5 7]) = [5 7]
    // not [NaN 7]. std::max is order-dependent for NaN and returned NaN here.
    return elementwiseDouble(a, b, [](double aa, double bb) { return std::fmax(aa, bb); }, p);
}

Value min(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    {
        auto r = dispatchIntegerBinaryOp(a, b,
            [](auto x, auto y) { return x < y ? x : y; }, p);
        if (!r.isUnset()) return r;
    }
    return elementwiseDouble(a, b, [](double aa, double bb) { return std::fmin(aa, bb); }, p);
}

// Binary nan-aware variants. For floating types, NaN propagates as
// "missing": when one arg is NaN, take the other; both NaN → NaN.
// For integer types, NaN can't occur so omitnan is a no-op (same as
// the regular max/min).
Value maxOmitNanBinary(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    {
        auto r = dispatchIntegerBinaryOp(a, b,
            [](auto x, auto y) {
                using T = decltype(x);
                if constexpr (std::is_floating_point_v<T>) {
                    if (std::isnan(x)) return y;
                    if (std::isnan(y)) return x;
                }
                return x > y ? x : y;
            }, p);
        if (!r.isUnset()) return r;
    }
    return elementwiseDouble(a, b, [](double aa, double bb) {
        if (std::isnan(aa)) return bb;
        if (std::isnan(bb)) return aa;
        return std::max(aa, bb);
    }, p);
}

Value minOmitNanBinary(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    {
        auto r = dispatchIntegerBinaryOp(a, b,
            [](auto x, auto y) {
                using T = decltype(x);
                if constexpr (std::is_floating_point_v<T>) {
                    if (std::isnan(x)) return y;
                    if (std::isnan(y)) return x;
                }
                return x < y ? x : y;
            }, p);
        if (!r.isUnset()) return r;
    }
    return elementwiseDouble(a, b, [](double aa, double bb) {
        if (std::isnan(aa)) return bb;
        if (std::isnan(bb)) return aa;
        return std::min(aa, bb);
    }, p);
}

// ── Generators ───────────────────────────────────────────────────────
Value linspace(double a, double b, size_t n, std::pmr::memory_resource *mr)
{
    auto r = Value::matrix(1, n, ValueType::DOUBLE, mr);
    if (n == 0)
        return r;
    if (n == 1) {
        r.doubleDataMut()[0] = b;
        return r;
    }
    for (size_t i = 0; i < n; ++i)
        r.doubleDataMut()[i] = a + (b - a) * static_cast<double>(i) / static_cast<double>(n - 1);
    return r;
}

Value logspace(double a, double b, size_t n, std::pmr::memory_resource *mr)
{
    auto r = Value::matrix(1, n, ValueType::DOUBLE, mr);
    if (n == 0)
        return r;
    if (n == 1) {
        r.doubleDataMut()[0] = std::pow(10.0, b);
        return r;
    }
    for (size_t i = 0; i < n; ++i) {
        const double exponent = a + (b - a) * static_cast<double>(i) / static_cast<double>(n - 1);
        r.doubleDataMut()[i] = std::pow(10.0, exponent);
    }
    return r;
}

} // namespace numkit::math
