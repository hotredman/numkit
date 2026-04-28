// libs/builtin/src/math/elementary/reductions.cpp
//
// Reductions (sum / prod / mean / max / min — single-return forms,
// NaN-aware variants, complex variants) plus the typed-output dispatcher
// used by their adapters. Also hosts linspace / logspace.
//
// trigonometry / exponents / rounding / misc / special live in sibling
// files under math/elementary/. Random generators (rand/randn) live in
// math/random/rng.cpp.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/math/elementary/exponents.hpp>      // exp / log adapters
#include <numkit/builtin/math/elementary/reductions.hpp>
#include <numkit/builtin/math/elementary/rounding.hpp>       // abs adapter
#include <numkit/builtin/math/elementary/trigonometry.hpp>   // sin / cos adapters

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "reduction_helpers.hpp"
#include "backends/var_reduction.hpp"  // for sumScan + addInto

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <string>
#include <type_traits>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Reductions (single-return) — sum / prod / mean
// ════════════════════════════════════════════════════════════════════════
namespace {

// Generic column-/dim-wise reducer: applies op(acc, x) and initializes
// acc with init. For 2D: reduces across rows → row vector of cols. For 3D:
// reduces along first non-singleton dimension. For vectors/scalars: scalar.
template<typename Op>
Value reduce(const Value &x, Op op, double init, Allocator *alloc, bool meanMode = false)
{
    // Reads each element as double — supports DOUBLE / SINGLE / integer
    // / LOGICAL transparently. DOUBLE keeps the contiguous fast path.
    const bool fastDouble = (x.type() == ValueType::DOUBLE);
    auto readD = [&](size_t i) -> double {
        return fastDouble ? x.doubleData()[i] : x.elemAsDouble(i);
    };

    if (x.dims().isVector() || x.isScalar()) {
        double acc = init;
        for (size_t i = 0; i < x.numel(); ++i)
            acc = op(acc, readD(i));
        if (meanMode)
            acc /= static_cast<double>(x.numel());
        return Value::scalar(acc, alloc);
    }

    const size_t R = x.dims().rows(), C = x.dims().cols();

    if (x.dims().is3D()) {
        const size_t P = x.dims().pages();
        const int redDim = (R > 1) ? 0 : (C > 1) ? 1 : 2;
        const size_t outR = (redDim == 0) ? 1 : R;
        const size_t outC = (redDim == 1) ? 1 : C;
        const size_t outP = (redDim == 2) ? 1 : P;
        const size_t N = (redDim == 0) ? R : (redDim == 1) ? C : P;
        auto r = Value::matrix3d(outR, outC, outP, ValueType::DOUBLE, alloc);
        for (size_t pp = 0; pp < outP; ++pp)
            for (size_t c = 0; c < outC; ++c)
                for (size_t rr = 0; rr < outR; ++rr) {
                    double acc = init;
                    for (size_t k = 0; k < N; ++k) {
                        const size_t rIdx = (redDim == 0) ? k : rr;
                        const size_t cIdx = (redDim == 1) ? k : c;
                        const size_t pIdx = (redDim == 2) ? k : pp;
                        acc = op(acc, readD(pIdx * R * C + cIdx * R + rIdx));
                    }
                    if (meanMode)
                        acc /= static_cast<double>(N);
                    r.doubleDataMut()[pp * outR * outC + c * outR + rr] = acc;
                }
        return r;
    }

    auto r = Value::matrix(1, C, ValueType::DOUBLE, alloc);
    for (size_t c = 0; c < C; ++c) {
        double acc = init;
        for (size_t rr = 0; rr < R; ++rr)
            acc = op(acc, readD(c * R + rr));
        if (meanMode)
            acc /= static_cast<double>(R);
        r.doubleDataMut()[c] = acc;
    }
    return r;
}

} // anonymous namespace

Value sum(Allocator &alloc, const Value &x)
{
    return reduce(x, [](double a, double b) { return a + b; }, 0.0, &alloc);
}

Value sum(Allocator &alloc, const Value &x, int dim)
{
    if (dim <= 0) return sum(alloc, x);
    const int d = detail::resolveDim(x, dim, "sum");

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
        auto r = Value::matrix(R, 1, ValueType::DOUBLE, &alloc);
        double *totals = r.doubleDataMut();
        std::fill(totals, totals + R, 0.0);
        const double *src = x.doubleData();
        for (size_t c = 0; c < C; ++c)
            addInto(totals, src + c * R, R);
        return r;
    }

    // Generic dim path (1D, 3D, dim=1, dim=3) — slice through scratch.
    return detail::applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            double acc = 0.0;
            for (size_t i = 0; i < n; ++i) acc += slice[i];
            return acc;
        }, &alloc);
}

Value prod(Allocator &alloc, const Value &x)
{
    return reduce(x, [](double a, double b) { return a * b; }, 1.0, &alloc);
}

Value prod(Allocator &alloc, const Value &x, int dim)
{
    if (dim <= 0) return prod(alloc, x);
    const int d = detail::resolveDim(x, dim, "prod");
    return detail::applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            double acc = 1.0;
            for (size_t i = 0; i < n; ++i) acc *= slice[i];
            return acc;
        }, &alloc);
}

Value mean(Allocator &alloc, const Value &x)
{
    return reduce(x, [](double a, double b) { return a + b; }, 0.0, &alloc, /*meanMode=*/true);
}

Value mean(Allocator &alloc, const Value &x, int dim)
{
    if (dim <= 0) return mean(alloc, x);
    const int d = detail::resolveDim(x, dim, "mean");
    return detail::applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            double acc = 0.0;
            for (size_t i = 0; i < n; ++i) acc += slice[i];
            return acc / static_cast<double>(n);
        }, &alloc);
}

// ── max/min with index ───────────────────────────────────────────────
//
// Per MATLAB semantics, min/max preserve the input element type
// (default 'native' mode). The value array is T-typed; the index
// array is always DOUBLE. COMPLEX inputs are rejected (no order
// defined on complex). Dispatch over ValueType picks the right T
// instantiation for DOUBLE, SINGLE, INT8..INT64, UINT8..UINT64,
// LOGICAL (storage = uint8) and CHAR (storage = char).

namespace {

// Read x[i] as T. Direct buffer access when source storage matches T;
// otherwise convert via elemAsDouble (with saturating cast for
// integers) — this branch only fires for the (rare) typeMatch=false
// dispatch error case, but kept for safety.
template <typename T>
inline T readSrcAsT(const Value &x, size_t i, bool typeMatch)
{
    if (typeMatch)
        return static_cast<const T *>(x.rawData())[i];
    if constexpr (std::is_floating_point_v<T>) {
        return static_cast<T>(x.elemAsDouble(i));
    } else {
        const double d = x.elemAsDouble(i);
        return static_cast<T>(std::clamp(std::round(d),
            static_cast<double>(std::numeric_limits<T>::lowest()),
            static_cast<double>(std::numeric_limits<T>::max())));
    }
}

template <typename T>
inline Value makeScalarT(T v, ValueType outType, Allocator *alloc)
{
    if (outType == ValueType::DOUBLE)
        return Value::scalar(static_cast<double>(v), alloc);
    if (outType == ValueType::LOGICAL)
        return Value::logicalScalar(v != 0, alloc);
    auto r = Value::matrix(1, 1, outType, alloc);
    static_cast<T *>(r.rawDataMut())[0] = v;
    return r;
}

// Walk every output cell along dim `redDim` (1-based). For each cell,
// gather the slice into `scratch`, find best via cmp, write best to
// dst[outIdx] and (1-based) source position to dstI[outIdx]. Handles
// 2D / 3D / ND uniformly via stride arithmetic.
template <typename T, typename Cmp>
void minMaxAlongDim(const Value &x, int redDim, T *dst, double *dstI, Cmp cmp,
                    bool typeMatch)
{
    const auto &d = x.dims();
    const int redAxis = redDim - 1;
    const size_t sliceLen = d.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < d.ndim(); ++i) O *= d.dim(i);

    if (sliceLen == 0) return;  // empty slice — caller has set output to defaults

    std::vector<T> scratch(sliceLen);
    auto runSlice = [&](size_t outIdx, size_t baseOff, size_t stride) {
        for (size_t k = 0; k < sliceLen; ++k)
            scratch[k] = readSrcAsT<T>(x, baseOff + k * stride, typeMatch);
        T best = scratch[0];
        size_t bi = 0;
        for (size_t k = 1; k < sliceLen; ++k)
            if (cmp(scratch[k], best)) { best = scratch[k]; bi = k; }
        dst[outIdx] = best;
        dstI[outIdx] = static_cast<double>(bi + 1);
    };

    if (B == 1) {
        // Reducing axis 0 → contiguous gather (stride = 1).
        for (size_t o = 0; o < O; ++o)
            runSlice(o, o * sliceLen, 1);
        return;
    }
    for (size_t o = 0; o < O; ++o) {
        for (size_t b = 0; b < B; ++b) {
            const size_t base = o * sliceLen * B + b;
            const size_t outIdx = o * B + b;
            runSlice(outIdx, base, B);
        }
    }
}

// Construct the right-shaped (value, idx) output pair for reduction
// along `redDim` of x: value array has `outType`, idx array has DOUBLE.
inline std::pair<Value, Value>
allocMinMaxOutputs(const Value &x, int redDim, ValueType outType, Allocator *alloc)
{
    if (x.dims().ndim() >= 4 && redDim >= 1 && redDim <= x.dims().ndim()) {
        auto shape = detail::outShapeForDimND(x, redDim);
        return {Value::matrixND(shape.data(), (int) shape.size(), outType, alloc),
                Value::matrixND(shape.data(), (int) shape.size(), ValueType::DOUBLE, alloc)};
    }
    auto outShape = detail::outShapeForDim(x, redDim);
    return {createMatrix(outShape, outType, alloc),
            createMatrix(outShape, ValueType::DOUBLE, alloc)};
}

template <typename T, typename Cmp>
std::tuple<Value, Value>
reduceMinMaxAllT(const Value &x, Cmp cmp, ValueType outType, Allocator *alloc)
{
    const bool typeMatch = (x.type() == outType);
    if (x.numel() == 0)
        throw std::runtime_error("min/max of empty array is not supported");
    if (x.isScalar() || x.dims().isVector()) {
        T best = readSrcAsT<T>(x, 0, typeMatch);
        size_t bi = 0;
        for (size_t i = 1; i < x.numel(); ++i) {
            const T v = readSrcAsT<T>(x, i, typeMatch);
            if (cmp(v, best)) { best = v; bi = i; }
        }
        return std::make_tuple(makeScalarT<T>(best, outType, alloc),
                               Value::scalar(static_cast<double>(bi + 1), alloc));
    }
    // Multi-dim: reduce along first non-singleton dim (MATLAB rule).
    const int redDim = detail::firstNonSingletonDim(x);
    auto [out, outIdx] = allocMinMaxOutputs(x, redDim, outType, alloc);
    minMaxAlongDim<T>(x, redDim,
                     static_cast<T *>(out.rawDataMut()),
                     outIdx.doubleDataMut(),
                     cmp, typeMatch);
    return std::make_tuple(std::move(out), std::move(outIdx));
}

template <typename T, typename Cmp>
std::tuple<Value, Value>
reduceMinMaxAlongDimT(const Value &x, int dim, Cmp cmp, ValueType outType, Allocator *alloc)
{
    const bool typeMatch = (x.type() == outType);
    if (x.isScalar() || x.dims().isVector()) {
        // For vectors, MATLAB ignores explicit dim and reduces all elements
        // when dim == firstNonSingleton; otherwise (dim past ndim or singleton
        // dim) it returns the input unchanged with idx = ones.
        if (dim != detail::firstNonSingletonDim(x)) {
            // Identity reduction: copy x as outType (cast where needed) and
            // return ones as idx.
            const size_t n = x.numel();
            Value out, outIdx;
            if (x.dims().isVector()) {
                out    = createMatrix({x.dims().rows(), x.dims().cols(), 0}, outType, alloc);
                outIdx = createMatrix({x.dims().rows(), x.dims().cols(), 0}, ValueType::DOUBLE, alloc);
            } else {
                out    = makeScalarT<T>(readSrcAsT<T>(x, 0, typeMatch), outType, alloc);
                outIdx = Value::scalar(1.0, alloc);
                return std::make_tuple(std::move(out), std::move(outIdx));
            }
            T *dst = static_cast<T *>(out.rawDataMut());
            double *dstI = outIdx.doubleDataMut();
            for (size_t i = 0; i < n; ++i) {
                dst[i]  = readSrcAsT<T>(x, i, typeMatch);
                dstI[i] = 1.0;
            }
            return std::make_tuple(std::move(out), std::move(outIdx));
        }
        // Reduce-all on a vector → scalar (matches the no-dim form).
        return reduceMinMaxAllT<T>(x, cmp, outType, alloc);
    }
    auto [out, outIdx] = allocMinMaxOutputs(x, dim, outType, alloc);
    minMaxAlongDim<T>(x, dim,
                     static_cast<T *>(out.rawDataMut()),
                     outIdx.doubleDataMut(),
                     cmp, typeMatch);
    return std::make_tuple(std::move(out), std::move(outIdx));
}

// COMPLEX min/max — MATLAB rule: compare by |z| (modulus) primary key,
// angle(z) (argument) secondary key for ties. Special case: when the
// entire input has all-zero imaginary parts, MATLAB falls back to
// comparing the real parts directly (so min([1 -3 2]) = -3, not the
// element with smallest |z|).
//
// Output value array is COMPLEX; index array is always DOUBLE (matches
// the round-3 typed reducers). The all-real check is one O(n) scan over
// the input — fast and amortised across the per-slice work.
inline bool allImagZero(const Complex *data, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        if (data[i].imag() != 0.0) return false;
    return true;
}

template <bool IsMax>
inline bool complexBetter(Complex v, Complex best, bool allReal)
{
    if (allReal) {
        if constexpr (IsMax) return v.real() > best.real();
        else                 return v.real() < best.real();
    }
    const double absV = std::abs(v), absB = std::abs(best);
    if (absV != absB) {
        if constexpr (IsMax) return absV > absB;
        else                 return absV < absB;
    }
    const double angV = std::arg(v), angB = std::arg(best);
    if constexpr (IsMax) return angV > angB;
    else                 return angV < angB;
}

inline std::pair<Value, Value>
allocComplexMinMaxOutputs(const Value &x, int redDim, Allocator *alloc)
{
    if (x.dims().ndim() >= 4 && redDim >= 1 && redDim <= x.dims().ndim()) {
        auto shape = detail::outShapeForDimND(x, redDim);
        return {Value::matrixND(shape.data(), (int) shape.size(), ValueType::COMPLEX, alloc),
                Value::matrixND(shape.data(), (int) shape.size(), ValueType::DOUBLE,  alloc)};
    }
    auto outShape = detail::outShapeForDim(x, redDim);
    return {createMatrix(outShape, ValueType::COMPLEX, alloc),
            createMatrix(outShape, ValueType::DOUBLE,  alloc)};
}

template <bool IsMax>
std::tuple<Value, Value>
reduceMinMaxComplexAll(const Value &x, Allocator *alloc, const char *fn)
{
    if (x.numel() == 0)
        throw Error(std::string(fn) + " of empty array is not supported",
                     0, 0, fn, "", std::string("m:") + fn + ":empty");
    const Complex *data = x.complexData();
    const bool allReal = allImagZero(data, x.numel());
    if (x.isScalar() || x.dims().isVector()) {
        Complex best = data[0];
        size_t bi = 0;
        for (size_t i = 1; i < x.numel(); ++i)
            if (complexBetter<IsMax>(data[i], best, allReal)) { best = data[i]; bi = i; }
        return std::make_tuple(Value::complexScalar(best, alloc),
                               Value::scalar(static_cast<double>(bi + 1), alloc));
    }
    const int redDim = detail::firstNonSingletonDim(x);
    auto [out, outIdx] = allocComplexMinMaxOutputs(x, redDim, alloc);
    Complex *dst  = out.complexDataMut();
    double  *dstI = outIdx.doubleDataMut();

    const auto &d = x.dims();
    const int redAxis = redDim - 1;
    const size_t sliceLen = d.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < d.ndim(); ++i) O *= d.dim(i);

    auto runSlice = [&](size_t outOff, size_t baseOff, size_t stride) {
        Complex best = data[baseOff];
        size_t bi = 0;
        for (size_t k = 1; k < sliceLen; ++k) {
            const Complex v = data[baseOff + k * stride];
            if (complexBetter<IsMax>(v, best, allReal)) { best = v; bi = k; }
        }
        dst[outOff] = best;
        dstI[outOff] = static_cast<double>(bi + 1);
    };
    if (B == 1) {
        for (size_t o = 0; o < O; ++o) runSlice(o, o * sliceLen, 1);
    } else {
        for (size_t o = 0; o < O; ++o)
            for (size_t b = 0; b < B; ++b)
                runSlice(o * B + b, o * sliceLen * B + b, B);
    }
    return std::make_tuple(std::move(out), std::move(outIdx));
}

template <bool IsMax>
std::tuple<Value, Value>
reduceMinMaxComplexAlongDim(const Value &x, int dim, Allocator *alloc, const char *fn)
{
    if (x.isScalar() || x.dims().isVector()) {
        if (dim != detail::firstNonSingletonDim(x)) {
            // Identity reduction: copy x as COMPLEX, idx = ones.
            const size_t n = x.numel();
            Value out, outIdx;
            if (x.dims().isVector()) {
                out    = createMatrix({x.dims().rows(), x.dims().cols(), 0}, ValueType::COMPLEX, alloc);
                outIdx = createMatrix({x.dims().rows(), x.dims().cols(), 0}, ValueType::DOUBLE,  alloc);
            } else {
                out    = Value::complexScalar(x.complexData()[0], alloc);
                outIdx = Value::scalar(1.0, alloc);
                return std::make_tuple(std::move(out), std::move(outIdx));
            }
            Complex *dst = out.complexDataMut();
            double  *dstI = outIdx.doubleDataMut();
            const Complex *src = x.complexData();
            for (size_t i = 0; i < n; ++i) { dst[i] = src[i]; dstI[i] = 1.0; }
            return std::make_tuple(std::move(out), std::move(outIdx));
        }
        return reduceMinMaxComplexAll<IsMax>(x, alloc, fn);
    }
    const Complex *data = x.complexData();
    const bool allReal = allImagZero(data, x.numel());
    auto [out, outIdx] = allocComplexMinMaxOutputs(x, dim, alloc);
    Complex *dst  = out.complexDataMut();
    double  *dstI = outIdx.doubleDataMut();

    const auto &d = x.dims();
    const int redAxis = dim - 1;
    const size_t sliceLen = d.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < d.ndim(); ++i) O *= d.dim(i);

    auto runSlice = [&](size_t outOff, size_t baseOff, size_t stride) {
        Complex best = data[baseOff];
        size_t bi = 0;
        for (size_t k = 1; k < sliceLen; ++k) {
            const Complex v = data[baseOff + k * stride];
            if (complexBetter<IsMax>(v, best, allReal)) { best = v; bi = k; }
        }
        dst[outOff] = best;
        dstI[outOff] = static_cast<double>(bi + 1);
    };
    if (B == 1) {
        for (size_t o = 0; o < O; ++o) runSlice(o, o * sliceLen, 1);
    } else {
        for (size_t o = 0; o < O; ++o)
            for (size_t b = 0; b < B; ++b)
                runSlice(o * B + b, o * sliceLen * B + b, B);
    }
    return std::make_tuple(std::move(out), std::move(outIdx));
}

// Forward declarations for the regular dispatchers (defined further
// down). Needed because the nan-aware dispatchers fall through to
// these for integer types — clang's strict two-phase lookup requires
// the declaration to be visible at the point of (template) reference.
template <bool IsMax, typename Cmp>
std::tuple<Value, Value>
dispatchMinMaxAll(const Value &x, Cmp cmp, Allocator *alloc, const char *fn);

template <bool IsMax, typename Cmp>
std::tuple<Value, Value>
dispatchMinMaxAlongDim(const Value &x, int dim, Cmp cmp, Allocator *alloc, const char *fn);

// ── NaN-aware min/max (omitnan flag) ─────────────────────────────────
//
// Per-ValueType dispatch identical to round-3 minMaxAlongDim, but skips
// NaN values during comparison. For floating types (DOUBLE/SINGLE) and
// COMPLEX (NaN if real or imag is NaN), filter out NaNs. For integer
// types, NaN can't occur so the regular kernel is used.
//
// All-NaN slice handling: the value array is set to NaN (for float/
// complex) and the index array to 1 (matching MATLAB convention).

template <typename T>
inline bool isElemNan(T v)
{
    if constexpr (std::is_floating_point_v<T>) return std::isnan(v);
    else                                       return false;
}

template <typename T, typename Cmp>
void minMaxNanAlongDim(const Value &x, int redDim, T *dst, double *dstI,
                       Cmp cmp, bool typeMatch)
{
    const auto &d = x.dims();
    const int redAxis = redDim - 1;
    const size_t sliceLen = d.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < d.ndim(); ++i) O *= d.dim(i);

    if (sliceLen == 0) return;

    auto runSlice = [&](size_t outIdx, size_t baseOff, size_t stride) {
        size_t firstValid = SIZE_MAX;
        T best{};
        for (size_t k = 0; k < sliceLen; ++k) {
            const T v = readSrcAsT<T>(x, baseOff + k * stride, typeMatch);
            if (isElemNan(v)) continue;
            if (firstValid == SIZE_MAX) {
                best = v; firstValid = k;
            } else if (cmp(v, best)) {
                best = v; firstValid = k;
            }
        }
        if (firstValid == SIZE_MAX) {
            // All-NaN slice: NaN value, idx = 1 (MATLAB convention).
            if constexpr (std::is_floating_point_v<T>)
                dst[outIdx] = std::numeric_limits<T>::quiet_NaN();
            else
                dst[outIdx] = T{};
            dstI[outIdx] = 1.0;
        } else {
            dst[outIdx] = best;
            dstI[outIdx] = static_cast<double>(firstValid + 1);
        }
    };
    if (B == 1) {
        for (size_t o = 0; o < O; ++o) runSlice(o, o * sliceLen, 1);
        return;
    }
    for (size_t o = 0; o < O; ++o)
        for (size_t b = 0; b < B; ++b)
            runSlice(o * B + b, o * sliceLen * B + b, B);
}

template <typename T, typename Cmp>
std::tuple<Value, Value>
reduceMinMaxNanAllT(const Value &x, Cmp cmp, ValueType outType, Allocator *alloc)
{
    if (x.numel() == 0)
        throw std::runtime_error("min/max of empty array is not supported");
    const bool typeMatch = (x.type() == outType);
    if (x.isScalar() || x.dims().isVector()) {
        size_t firstValid = SIZE_MAX;
        T best{};
        for (size_t i = 0; i < x.numel(); ++i) {
            const T v = readSrcAsT<T>(x, i, typeMatch);
            if (isElemNan(v)) continue;
            if (firstValid == SIZE_MAX) { best = v; firstValid = i; }
            else if (cmp(v, best))      { best = v; firstValid = i; }
        }
        if (firstValid == SIZE_MAX) {
            if constexpr (std::is_floating_point_v<T>)
                return std::make_tuple(makeScalarT<T>(std::numeric_limits<T>::quiet_NaN(), outType, alloc),
                                       Value::scalar(1.0, alloc));
            else
                return std::make_tuple(makeScalarT<T>(T{}, outType, alloc),
                                       Value::scalar(1.0, alloc));
        }
        return std::make_tuple(makeScalarT<T>(best, outType, alloc),
                               Value::scalar(static_cast<double>(firstValid + 1), alloc));
    }
    const int redDim = detail::firstNonSingletonDim(x);
    auto [out, outIdx] = allocMinMaxOutputs(x, redDim, outType, alloc);
    minMaxNanAlongDim<T>(x, redDim,
                         static_cast<T *>(out.rawDataMut()),
                         outIdx.doubleDataMut(),
                         cmp, typeMatch);
    return std::make_tuple(std::move(out), std::move(outIdx));
}

template <typename T, typename Cmp>
std::tuple<Value, Value>
reduceMinMaxNanAlongDimT(const Value &x, int dim, Cmp cmp, ValueType outType, Allocator *alloc)
{
    const bool typeMatch = (x.type() == outType);
    if (x.isScalar() || x.dims().isVector()) {
        if (dim != detail::firstNonSingletonDim(x))
            return reduceMinMaxAlongDimT<T>(x, dim, cmp, outType, alloc);
        return reduceMinMaxNanAllT<T>(x, cmp, outType, alloc);
    }
    auto [out, outIdx] = allocMinMaxOutputs(x, dim, outType, alloc);
    minMaxNanAlongDim<T>(x, dim,
                         static_cast<T *>(out.rawDataMut()),
                         outIdx.doubleDataMut(),
                         cmp, typeMatch);
    return std::make_tuple(std::move(out), std::move(outIdx));
}

// COMPLEX nan-aware min/max — same |z|+angle comparator, but skips
// elements where either real or imag part is NaN.
template <bool IsMax>
std::tuple<Value, Value>
reduceMinMaxComplexNanAll(const Value &x, Allocator *alloc, const char *fn)
{
    if (x.numel() == 0)
        throw Error(std::string(fn) + " of empty array is not supported",
                     0, 0, fn, "", std::string("m:") + fn + ":empty");
    const Complex *data = x.complexData();
    auto isNan = [](Complex c) { return std::isnan(c.real()) || std::isnan(c.imag()); };
    // For all-real check, only consider non-NaN elements.
    bool allReal = true;
    for (size_t i = 0; i < x.numel(); ++i) {
        if (isNan(data[i])) continue;
        if (data[i].imag() != 0.0) { allReal = false; break; }
    }
    auto findBest = [&](size_t baseOff, size_t stride, size_t n,
                        Complex &best, size_t &bi) {
        size_t firstValid = SIZE_MAX;
        for (size_t k = 0; k < n; ++k) {
            const Complex v = data[baseOff + k * stride];
            if (isNan(v)) continue;
            if (firstValid == SIZE_MAX) {
                best = v; bi = k; firstValid = k;
            } else if (complexBetter<IsMax>(v, best, allReal)) {
                best = v; bi = k; firstValid = k;
            }
        }
        return firstValid != SIZE_MAX;
    };

    if (x.isScalar() || x.dims().isVector()) {
        Complex best;
        size_t bi = 0;
        if (!findBest(0, 1, x.numel(), best, bi))
            return std::make_tuple(Value::complexScalar(Complex(std::nan(""), 0.0), alloc),
                                   Value::scalar(1.0, alloc));
        return std::make_tuple(Value::complexScalar(best, alloc),
                               Value::scalar(static_cast<double>(bi + 1), alloc));
    }
    const int redDim = detail::firstNonSingletonDim(x);
    auto [out, outIdx] = allocComplexMinMaxOutputs(x, redDim, alloc);
    Complex *dst = out.complexDataMut();
    double *dstI = outIdx.doubleDataMut();

    const auto &d = x.dims();
    const int redAxis = redDim - 1;
    const size_t sliceLen = d.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < d.ndim(); ++i) O *= d.dim(i);

    auto runSlice = [&](size_t outOff, size_t baseOff, size_t stride) {
        Complex best;
        size_t bi = 0;
        if (!findBest(baseOff, stride, sliceLen, best, bi)) {
            dst[outOff] = Complex(std::nan(""), 0.0);
            dstI[outOff] = 1.0;
        } else {
            dst[outOff] = best;
            dstI[outOff] = static_cast<double>(bi + 1);
        }
    };
    if (B == 1) {
        for (size_t o = 0; o < O; ++o) runSlice(o, o * sliceLen, 1);
    } else {
        for (size_t o = 0; o < O; ++o)
            for (size_t b = 0; b < B; ++b)
                runSlice(o * B + b, o * sliceLen * B + b, B);
    }
    return std::make_tuple(std::move(out), std::move(outIdx));
}

template <bool IsMax>
std::tuple<Value, Value>
reduceMinMaxComplexNanAlongDim(const Value &x, int dim, Allocator *alloc, const char *fn)
{
    if (x.isScalar() || x.dims().isVector()) {
        if (dim != detail::firstNonSingletonDim(x))
            return reduceMinMaxComplexAlongDim<IsMax>(x, dim, alloc, fn);
        return reduceMinMaxComplexNanAll<IsMax>(x, alloc, fn);
    }
    const Complex *data = x.complexData();
    auto isNan = [](Complex c) { return std::isnan(c.real()) || std::isnan(c.imag()); };
    bool allReal = true;
    for (size_t i = 0; i < x.numel(); ++i) {
        if (isNan(data[i])) continue;
        if (data[i].imag() != 0.0) { allReal = false; break; }
    }
    auto [out, outIdx] = allocComplexMinMaxOutputs(x, dim, alloc);
    Complex *dst = out.complexDataMut();
    double *dstI = outIdx.doubleDataMut();

    const auto &d = x.dims();
    const int redAxis = dim - 1;
    const size_t sliceLen = d.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < d.ndim(); ++i) O *= d.dim(i);

    auto runSlice = [&](size_t outOff, size_t baseOff, size_t stride) {
        size_t firstValid = SIZE_MAX;
        Complex best;
        size_t bi = 0;
        for (size_t k = 0; k < sliceLen; ++k) {
            const Complex v = data[baseOff + k * stride];
            if (isNan(v)) continue;
            if (firstValid == SIZE_MAX) { best = v; bi = k; firstValid = k; }
            else if (complexBetter<IsMax>(v, best, allReal)) { best = v; bi = k; firstValid = k; }
        }
        if (firstValid == SIZE_MAX) {
            dst[outOff] = Complex(std::nan(""), 0.0);
            dstI[outOff] = 1.0;
        } else {
            dst[outOff] = best;
            dstI[outOff] = static_cast<double>(bi + 1);
        }
    };
    if (B == 1) {
        for (size_t o = 0; o < O; ++o) runSlice(o, o * sliceLen, 1);
    } else {
        for (size_t o = 0; o < O; ++o)
            for (size_t b = 0; b < B; ++b)
                runSlice(o * B + b, o * sliceLen * B + b, B);
    }
    return std::make_tuple(std::move(out), std::move(outIdx));
}

template <bool IsMax, typename Cmp>
std::tuple<Value, Value>
dispatchMinMaxNanAll(const Value &x, Cmp cmp, Allocator *alloc, const char *fn)
{
    switch (x.type()) {
    case ValueType::DOUBLE:  return reduceMinMaxNanAllT<double>(x, cmp, ValueType::DOUBLE, alloc);
    case ValueType::SINGLE:  return reduceMinMaxNanAllT<float >(x, cmp, ValueType::SINGLE, alloc);
    case ValueType::COMPLEX: return reduceMinMaxComplexNanAll<IsMax>(x, alloc, fn);
    // Integer/logical/char have no NaN — fall through to the regular path.
    default: return dispatchMinMaxAll<IsMax>(x, cmp, alloc, fn);
    }
}

template <bool IsMax, typename Cmp>
std::tuple<Value, Value>
dispatchMinMaxNanAlongDim(const Value &x, int dim, Cmp cmp,
                          Allocator *alloc, const char *fn)
{
    switch (x.type()) {
    case ValueType::DOUBLE:  return reduceMinMaxNanAlongDimT<double>(x, dim, cmp, ValueType::DOUBLE, alloc);
    case ValueType::SINGLE:  return reduceMinMaxNanAlongDimT<float >(x, dim, cmp, ValueType::SINGLE, alloc);
    case ValueType::COMPLEX: return reduceMinMaxComplexNanAlongDim<IsMax>(x, dim, alloc, fn);
    default: return dispatchMinMaxAlongDim<IsMax>(x, dim, cmp, alloc, fn);
    }
}

// Dispatch on x.type(), instantiate reducer with right T/outType pair.
// LOGICAL maps to T=uint8_t (storage type). CHAR maps to T=char.
// COMPLEX uses the |z|-then-angle comparator (MATLAB rule).
template <bool IsMax, typename Cmp>
std::tuple<Value, Value>
dispatchMinMaxAll(const Value &x, Cmp cmp, Allocator *alloc, const char *fn)
{
    switch (x.type()) {
    case ValueType::DOUBLE:  return reduceMinMaxAllT<double  >(x, cmp, ValueType::DOUBLE,  alloc);
    case ValueType::SINGLE:  return reduceMinMaxAllT<float   >(x, cmp, ValueType::SINGLE,  alloc);
    case ValueType::INT8:    return reduceMinMaxAllT<int8_t  >(x, cmp, ValueType::INT8,    alloc);
    case ValueType::INT16:   return reduceMinMaxAllT<int16_t >(x, cmp, ValueType::INT16,   alloc);
    case ValueType::INT32:   return reduceMinMaxAllT<int32_t >(x, cmp, ValueType::INT32,   alloc);
    case ValueType::INT64:   return reduceMinMaxAllT<int64_t >(x, cmp, ValueType::INT64,   alloc);
    case ValueType::UINT8:   return reduceMinMaxAllT<uint8_t >(x, cmp, ValueType::UINT8,   alloc);
    case ValueType::UINT16:  return reduceMinMaxAllT<uint16_t>(x, cmp, ValueType::UINT16,  alloc);
    case ValueType::UINT32:  return reduceMinMaxAllT<uint32_t>(x, cmp, ValueType::UINT32,  alloc);
    case ValueType::UINT64:  return reduceMinMaxAllT<uint64_t>(x, cmp, ValueType::UINT64,  alloc);
    case ValueType::LOGICAL: return reduceMinMaxAllT<uint8_t >(x, cmp, ValueType::LOGICAL, alloc);
    case ValueType::CHAR:    return reduceMinMaxAllT<char    >(x, cmp, ValueType::CHAR,    alloc);
    case ValueType::COMPLEX: return reduceMinMaxComplexAll<IsMax>(x, alloc, fn);
    default:
        throw Error(std::string(fn) + ": unsupported input type",
                     0, 0, fn, "", std::string("m:") + fn + ":type");
    }
}

template <bool IsMax, typename Cmp>
std::tuple<Value, Value>
dispatchMinMaxAlongDim(const Value &x, int dim, Cmp cmp, Allocator *alloc, const char *fn)
{
    switch (x.type()) {
    case ValueType::DOUBLE:  return reduceMinMaxAlongDimT<double  >(x, dim, cmp, ValueType::DOUBLE,  alloc);
    case ValueType::SINGLE:  return reduceMinMaxAlongDimT<float   >(x, dim, cmp, ValueType::SINGLE,  alloc);
    case ValueType::INT8:    return reduceMinMaxAlongDimT<int8_t  >(x, dim, cmp, ValueType::INT8,    alloc);
    case ValueType::INT16:   return reduceMinMaxAlongDimT<int16_t >(x, dim, cmp, ValueType::INT16,   alloc);
    case ValueType::INT32:   return reduceMinMaxAlongDimT<int32_t >(x, dim, cmp, ValueType::INT32,   alloc);
    case ValueType::INT64:   return reduceMinMaxAlongDimT<int64_t >(x, dim, cmp, ValueType::INT64,   alloc);
    case ValueType::UINT8:   return reduceMinMaxAlongDimT<uint8_t >(x, dim, cmp, ValueType::UINT8,   alloc);
    case ValueType::UINT16:  return reduceMinMaxAlongDimT<uint16_t>(x, dim, cmp, ValueType::UINT16,  alloc);
    case ValueType::UINT32:  return reduceMinMaxAlongDimT<uint32_t>(x, dim, cmp, ValueType::UINT32,  alloc);
    case ValueType::UINT64:  return reduceMinMaxAlongDimT<uint64_t>(x, dim, cmp, ValueType::UINT64,  alloc);
    case ValueType::LOGICAL: return reduceMinMaxAlongDimT<uint8_t >(x, dim, cmp, ValueType::LOGICAL, alloc);
    case ValueType::CHAR:    return reduceMinMaxAlongDimT<char    >(x, dim, cmp, ValueType::CHAR,    alloc);
    case ValueType::COMPLEX: return reduceMinMaxComplexAlongDim<IsMax>(x, dim, alloc, fn);
    default:
        throw Error(std::string(fn) + ": unsupported input type",
                     0, 0, fn, "", std::string("m:") + fn + ":type");
    }
}

} // anonymous namespace

std::tuple<Value, Value> max(Allocator &alloc, const Value &x)
{
    return dispatchMinMaxAll<true>(x, [](auto v, auto best) { return v > best; }, &alloc, "max");
}

std::tuple<Value, Value> min(Allocator &alloc, const Value &x)
{
    return dispatchMinMaxAll<false>(x, [](auto v, auto best) { return v < best; }, &alloc, "min");
}

std::tuple<Value, Value> max(Allocator &alloc, const Value &x, int dim)
{
    if (dim <= 0) return max(alloc, x);
    const int d = detail::resolveDim(x, dim, "max");
    return dispatchMinMaxAlongDim<true>(x, d, [](auto v, auto best) { return v > best; }, &alloc, "max");
}

std::tuple<Value, Value> maxOmitNan(Allocator &alloc, const Value &x, int dim)
{
    if (dim <= 0)
        return dispatchMinMaxNanAll<true>(x, [](auto v, auto best) { return v > best; }, &alloc, "max");
    const int d = detail::resolveDim(x, dim, "max");
    return dispatchMinMaxNanAlongDim<true>(x, d, [](auto v, auto best) { return v > best; }, &alloc, "max");
}

std::tuple<Value, Value> minOmitNan(Allocator &alloc, const Value &x, int dim)
{
    if (dim <= 0)
        return dispatchMinMaxNanAll<false>(x, [](auto v, auto best) { return v < best; }, &alloc, "min");
    const int d = detail::resolveDim(x, dim, "min");
    return dispatchMinMaxNanAlongDim<false>(x, d, [](auto v, auto best) { return v < best; }, &alloc, "min");
}

std::tuple<Value, Value> min(Allocator &alloc, const Value &x, int dim)
{
    if (dim <= 0) return min(alloc, x);
    const int d = detail::resolveDim(x, dim, "min");
    return dispatchMinMaxAlongDim<false>(x, d, [](auto v, auto best) { return v < best; }, &alloc, "min");
}

Value max(Allocator &alloc, const Value &a, const Value &b)
{
    Allocator *p = &alloc;
    // Integer / single binary form: result follows MATLAB type promotion
    // (integer wins over double; single wins over double; same-class
    // integers stay; mixed-class integers throw).
    {
        auto r = dispatchIntegerBinaryOp(a, b,
            [](auto x, auto y) { return x > y ? x : y; }, p);
        if (!r.isUnset()) return r;
    }
    return elementwiseDouble(a, b, [](double aa, double bb) { return std::max(aa, bb); }, p);
}

Value min(Allocator &alloc, const Value &a, const Value &b)
{
    Allocator *p = &alloc;
    {
        auto r = dispatchIntegerBinaryOp(a, b,
            [](auto x, auto y) { return x < y ? x : y; }, p);
        if (!r.isUnset()) return r;
    }
    return elementwiseDouble(a, b, [](double aa, double bb) { return std::min(aa, bb); }, p);
}

// Binary nan-aware variants. For floating types, NaN propagates as
// "missing": when one arg is NaN, take the other; both NaN → NaN.
// For integer types, NaN can't occur so omitnan is a no-op (same as
// the regular max/min).
Value maxOmitNanBinary(Allocator &alloc, const Value &a, const Value &b)
{
    Allocator *p = &alloc;
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

Value minOmitNanBinary(Allocator &alloc, const Value &a, const Value &b)
{
    Allocator *p = &alloc;
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
Value linspace(Allocator &alloc, double a, double b, size_t n)
{
    auto r = Value::matrix(1, n, ValueType::DOUBLE, &alloc);
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

Value logspace(Allocator &alloc, double a, double b, size_t n)
{
    auto r = Value::matrix(1, n, ValueType::DOUBLE, &alloc);
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

// rand / randn / randND / randnND — moved to math/random/rng.cpp.

// ════════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

// Helper to reduce boilerplate — unary adapter that calls Fn(alloc, args[0]).
#define NK_UNARY_ADAPTER(name, fn)                                              \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "m:" #name ":nargin");           \
        outs[0] = fn(ctx.engine->allocator(), args[0]);                         \
    }

// Same as NK_UNARY_ADAPTER but passes &outs[0] through as an
// output-buffer reuse hint. The callee MAY overwrite *outs[0]'s
// existing buffer instead of allocating fresh, when the hint is
// a uniquely-owned heap double of matching shape. The VM CALL
// handler pre-fills outs[0] with R[I.a] (the destination register)
// when none of the args alias the destination — so `z = sin(x)`
// in a loop reuses z's buffer iteration after iteration.
#define NK_UNARY_ADAPTER_HINT(name, fn)                                         \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "m:" #name ":nargin");           \
        outs[0] = fn(ctx.engine->allocator(), args[0], &outs[0]);               \
    }

// SIMD-backed unaries — abs lives in backends/MStdAbs_*.cpp,
// sin/cos/exp/log in backends/MStdTranscendental_*.cpp. We host their
// engine adapters here because no other TU does.
NK_UNARY_ADAPTER_HINT(abs,     abs)
NK_UNARY_ADAPTER_HINT(sin,     sin)
NK_UNARY_ADAPTER_HINT(cos,     cos)
NK_UNARY_ADAPTER_HINT(exp,     exp)
NK_UNARY_ADAPTER_HINT(log,     log)

// All other unary adapters (sqrt / tan / asin / acos / atan / log2 /
// log10 / floor / ceil / round / fix / sign / deg2rad / rad2deg /
// expm1 / log1p / gamma / erf / ...) live next to their public APIs
// under math/elementary/{trigonometry,exponents,rounding,misc,special}.cpp.

#undef NK_UNARY_ADAPTER

// sum/prod/mean — MATLAB signatures:
//   sum(X)                  — scalar reduce, default output type
//   sum(X, dim)             — reduce along dim, default output type
//   sum(X, outtype)         — scalar reduce with explicit output type
//   sum(X, dim, outtype)    — reduce along dim with explicit output type
// outtype ∈ {'default', 'double', 'native'}:
//   'default' — DOUBLE (current numkit-m behaviour for all input types)
//   'double'  — same as default
//   'native'  — preserve input element class (integer types saturate;
//               LOGICAL/CHAR/COMPLEX rejected — matches MATLAB).
// 'all' as a dim placeholder is not yet supported.

namespace {

enum class OutTypeMode { Default, Double, Native };

inline bool isStringArg(const Value &v)
{
    return v.type() == ValueType::CHAR;
}

OutTypeMode parseOutTypeMode(const Value &arg, const char *fn)
{
    std::string s = arg.toString();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (s == "default") return OutTypeMode::Default;
    if (s == "double")  return OutTypeMode::Double;
    if (s == "native")  return OutTypeMode::Native;
    throw Error(std::string(fn) + ": unknown output type '" + s + "'",
                 0, 0, fn, "", std::string("m:") + fn + ":outtype");
}

ValueType resolveNativeOutType(ValueType inType, const char *fn)
{
    switch (inType) {
    case ValueType::DOUBLE: case ValueType::SINGLE:
    case ValueType::INT8:   case ValueType::INT16:  case ValueType::INT32:  case ValueType::INT64:
    case ValueType::UINT8:  case ValueType::UINT16: case ValueType::UINT32: case ValueType::UINT64:
        return inType;
    case ValueType::LOGICAL:
    case ValueType::CHAR:
        // MATLAB: 'native' is only defined for numeric (non-logical) inputs.
        throw Error(std::string(fn) + ": 'native' is not defined for logical/char inputs",
                     0, 0, fn, "", std::string("m:") + fn + ":nativeType");
    case ValueType::COMPLEX:
        // COMPLEX is handled before this call (dispatchReductionAdapter
        // routes it to the complex path), so reaching this branch is a
        // bug in the dispatcher.
        return ValueType::COMPLEX;
    default:
        throw Error(std::string(fn) + ": unsupported input type for 'native'",
                     0, 0, fn, "", std::string("m:") + fn + ":type");
    }
}

template <typename T>
inline T toOutT(double v)
{
    if constexpr (std::is_floating_point_v<T>) {
        return static_cast<T>(v);
    } else {
        if (std::isnan(v)) return T{};
        v = std::round(v);
        constexpr double lo = static_cast<double>(std::numeric_limits<T>::min());
        constexpr double hi = static_cast<double>(std::numeric_limits<T>::max());
        if (v < lo) return std::numeric_limits<T>::min();
        if (v > hi) return std::numeric_limits<T>::max();
        return static_cast<T>(v);
    }
}

template <typename T>
inline Value makeNativeScalar(T v, ValueType outType, Allocator *alloc)
{
    if (outType == ValueType::DOUBLE)
        return Value::scalar(static_cast<double>(v), alloc);
    auto r = Value::matrix(1, 1, outType, alloc);
    static_cast<T *>(r.rawDataMut())[0] = v;
    return r;
}

// Slice-walker that produces typed output. Walks 2D/3D/ND uniformly via
// stride math (mirrors round-3 minMaxAlongDim). `accumOp` accumulates a
// double over the slice; `finalize` converts (sum, sliceLen) to T.
template <typename T, typename Init, typename AccumOp, typename Finalize>
void typedReduceAlongDim(const Value &x, int redDim, T *dst,
                         Init init, AccumOp accumOp, Finalize finalize)
{
    const auto &d = x.dims();
    const int redAxis = redDim - 1;
    const size_t sliceLen = d.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < d.ndim(); ++i) O *= d.dim(i);

    auto reduceSlice = [&](size_t baseOff, size_t stride) -> T {
        double acc = init;
        for (size_t k = 0; k < sliceLen; ++k)
            acc = accumOp(acc, x.elemAsDouble(baseOff + k * stride));
        return finalize(acc, sliceLen);
    };

    if (B == 1) {
        for (size_t o = 0; o < O; ++o) dst[o] = reduceSlice(o * sliceLen, 1);
        return;
    }
    for (size_t o = 0; o < O; ++o) {
        for (size_t b = 0; b < B; ++b) {
            const size_t base = o * sliceLen * B + b;
            const size_t outIdx = o * B + b;
            dst[outIdx] = reduceSlice(base, B);
        }
    }
}

inline Value allocReduceOutput(const Value &x, int redDim, ValueType outType, Allocator *alloc)
{
    if (x.dims().ndim() >= 4 && redDim >= 1 && redDim <= x.dims().ndim()) {
        auto shape = detail::outShapeForDimND(x, redDim);
        return Value::matrixND(shape.data(), (int) shape.size(), outType, alloc);
    }
    auto outShape = detail::outShapeForDim(x, redDim);
    return createMatrix(outShape, outType, alloc);
}

template <typename T, typename Init, typename AccumOp, typename Finalize>
Value reduceTypedAll(const Value &x, ValueType outType, Allocator *alloc,
                      Init init, AccumOp accumOp, Finalize finalize)
{
    if (x.isEmpty()) {
        // Empty input: scalar reduce → identity (sum=0, prod=1, mean=NaN).
        const T v = finalize(static_cast<double>(init), 0);
        return makeNativeScalar<T>(v, outType, alloc);
    }
    if (x.isScalar() || x.dims().isVector()) {
        double acc = init;
        for (size_t i = 0; i < x.numel(); ++i)
            acc = accumOp(acc, x.elemAsDouble(i));
        return makeNativeScalar<T>(finalize(acc, x.numel()), outType, alloc);
    }
    const int redDim = detail::firstNonSingletonDim(x);
    Value out = allocReduceOutput(x, redDim, outType, alloc);
    typedReduceAlongDim<T>(x, redDim, static_cast<T *>(out.rawDataMut()),
                           init, accumOp, finalize);
    return out;
}

template <typename T, typename Init, typename AccumOp, typename Finalize>
Value reduceTypedAlongDim(const Value &x, int dim, ValueType outType, Allocator *alloc,
                           Init init, AccumOp accumOp, Finalize finalize)
{
    if (x.isEmpty() && x.dims().ndim() < 4) {
        return Value::matrix(0, 0, outType, alloc);
    }
    if (x.isScalar() || x.dims().isVector()) {
        if (dim != detail::firstNonSingletonDim(x)) {
            // Identity: copy x cast to T.
            const size_t n = x.numel();
            Value out;
            if (x.dims().isVector())
                out = createMatrix({x.dims().rows(), x.dims().cols(), 0}, outType, alloc);
            else
                return makeNativeScalar<T>(toOutT<T>(x.elemAsDouble(0)), outType, alloc);
            T *dst = static_cast<T *>(out.rawDataMut());
            for (size_t i = 0; i < n; ++i)
                dst[i] = toOutT<T>(x.elemAsDouble(i));
            return out;
        }
        return reduceTypedAll<T>(x, outType, alloc, init, accumOp, finalize);
    }
    Value out = allocReduceOutput(x, dim, outType, alloc);
    typedReduceAlongDim<T>(x, dim, static_cast<T *>(out.rawDataMut()),
                           init, accumOp, finalize);
    return out;
}

// Operation tags so the dispatcher can pick init/op/finalize uniformly
// across sum/prod/mean. Each tag exposes both a real (double-accumulator)
// and a complex (Complex-accumulator) family — the real one feeds the
// typed path (DOUBLE/SINGLE/integer outputs), the complex one feeds the
// COMPLEX path.
struct SumOp {
    static constexpr double init = 0.0;
    static double accum(double a, double b) { return a + b; }
    template<typename T>
    static T finalize(double acc, size_t /*n*/) { return toOutT<T>(acc); }
    static Complex cInit() { return Complex(0.0, 0.0); }
    static Complex cAccum(Complex a, Complex b) { return a + b; }
    static Complex cFinalize(Complex acc, size_t /*n*/) { return acc; }
};
struct ProdOp {
    static constexpr double init = 1.0;
    static double accum(double a, double b) { return a * b; }
    template<typename T>
    static T finalize(double acc, size_t /*n*/) { return toOutT<T>(acc); }
    static Complex cInit() { return Complex(1.0, 0.0); }
    static Complex cAccum(Complex a, Complex b) { return a * b; }
    static Complex cFinalize(Complex acc, size_t /*n*/) { return acc; }
};
struct MeanOp {
    static constexpr double init = 0.0;
    static double accum(double a, double b) { return a + b; }
    template<typename T>
    static T finalize(double acc, size_t n) {
        return toOutT<T>(n == 0 ? std::nan("") : acc / static_cast<double>(n));
    }
    static Complex cInit() { return Complex(0.0, 0.0); }
    static Complex cAccum(Complex a, Complex b) { return a + b; }
    static Complex cFinalize(Complex acc, size_t n) {
        return n == 0 ? Complex(std::nan(""), 0.0) : acc / static_cast<double>(n);
    }
};

// Reduce *every* element of x to a single scalar of type T. Used for
// the 'all' dim placeholder; differs from reduceTypedAll (which does
// "along first non-singleton dim", returning a vector for matrices).
template <typename T, typename Init, typename AccumOp, typename Finalize>
Value reduceAllElementsScalar(const Value &x, ValueType outType, Allocator *alloc,
                               Init init, AccumOp accumOp, Finalize finalize)
{
    double acc = init;
    const size_t n = x.numel();
    for (size_t i = 0; i < n; ++i)
        acc = accumOp(acc, x.elemAsDouble(i));
    return makeNativeScalar<T>(finalize(acc, n), outType, alloc);
}

// ── Complex reductions ──────────────────────────────────────────────
//
// MATLAB: sum/prod/mean preserve COMPLEX class. Non-complex inputs
// upgrade to Complex(real, 0). The accumulator must be Complex (NOT
// double — the round-4 default path used `elemAsDouble` which silently
// dropped imaginary parts and gave wrong results for sum(complex)).

inline Complex readElemAsComplex(const Value &x, size_t i, bool typeMatches)
{
    if (typeMatches) return x.complexData()[i];
    return Complex(x.elemAsDouble(i), 0.0);
}

inline Value allocComplexReduceOutput(const Value &x, int redDim, Allocator *alloc)
{
    if (x.dims().ndim() >= 4 && redDim >= 1 && redDim <= x.dims().ndim()) {
        auto shape = detail::outShapeForDimND(x, redDim);
        return Value::matrixND(shape.data(), (int) shape.size(), ValueType::COMPLEX, alloc);
    }
    auto outShape = detail::outShapeForDim(x, redDim);
    return createMatrix(outShape, ValueType::COMPLEX, alloc);
}

template <typename AccumOp, typename Finalize>
void complexReduceAlongDim(const Value &x, int redDim, Complex *dst,
                           Complex init, AccumOp accumOp, Finalize finalize)
{
    const bool typeMatches = (x.type() == ValueType::COMPLEX);
    const auto &d = x.dims();
    const int redAxis = redDim - 1;
    const size_t sliceLen = d.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < d.ndim(); ++i) O *= d.dim(i);

    auto reduceSlice = [&](size_t baseOff, size_t stride) -> Complex {
        Complex acc = init;
        for (size_t k = 0; k < sliceLen; ++k)
            acc = accumOp(acc, readElemAsComplex(x, baseOff + k * stride, typeMatches));
        return finalize(acc, sliceLen);
    };

    if (B == 1) {
        for (size_t o = 0; o < O; ++o) dst[o] = reduceSlice(o * sliceLen, 1);
        return;
    }
    for (size_t o = 0; o < O; ++o) {
        for (size_t b = 0; b < B; ++b) {
            const size_t base = o * sliceLen * B + b;
            const size_t outIdx = o * B + b;
            dst[outIdx] = reduceSlice(base, B);
        }
    }
}

template <typename AccumOp, typename Finalize>
Value reduceComplexAll(const Value &x, Allocator *alloc,
                        Complex init, AccumOp accumOp, Finalize finalize)
{
    if (x.isEmpty() && x.dims().ndim() < 4)
        return Value::matrix(0, 0, ValueType::COMPLEX, alloc);
    const bool typeMatches = (x.type() == ValueType::COMPLEX);
    if (x.isScalar() || x.dims().isVector()) {
        Complex acc = init;
        for (size_t i = 0; i < x.numel(); ++i)
            acc = accumOp(acc, readElemAsComplex(x, i, typeMatches));
        return Value::complexScalar(finalize(acc, x.numel()), alloc);
    }
    const int redDim = detail::firstNonSingletonDim(x);
    Value out = allocComplexReduceOutput(x, redDim, alloc);
    complexReduceAlongDim(x, redDim, out.complexDataMut(), init, accumOp, finalize);
    return out;
}

template <typename AccumOp, typename Finalize>
Value reduceComplexAlongDim(const Value &x, int dim, Allocator *alloc,
                             Complex init, AccumOp accumOp, Finalize finalize)
{
    if (x.isEmpty() && x.dims().ndim() < 4)
        return Value::matrix(0, 0, ValueType::COMPLEX, alloc);
    const bool typeMatches = (x.type() == ValueType::COMPLEX);
    if (x.isScalar() || x.dims().isVector()) {
        if (dim != detail::firstNonSingletonDim(x)) {
            // Identity reduction.
            const size_t n = x.numel();
            if (!x.dims().isVector())
                return Value::complexScalar(readElemAsComplex(x, 0, typeMatches), alloc);
            Value out = createMatrix({x.dims().rows(), x.dims().cols(), 0},
                                      ValueType::COMPLEX, alloc);
            Complex *dst = out.complexDataMut();
            for (size_t i = 0; i < n; ++i)
                dst[i] = readElemAsComplex(x, i, typeMatches);
            return out;
        }
        return reduceComplexAll(x, alloc, init, accumOp, finalize);
    }
    Value out = allocComplexReduceOutput(x, dim, alloc);
    complexReduceAlongDim(x, dim, out.complexDataMut(), init, accumOp, finalize);
    return out;
}

template <typename AccumOp, typename Finalize>
Value reduceComplexAllElementsScalar(const Value &x, Allocator *alloc,
                                      Complex init, AccumOp accumOp, Finalize finalize)
{
    const bool typeMatches = (x.type() == ValueType::COMPLEX);
    Complex acc = init;
    const size_t n = x.numel();
    for (size_t i = 0; i < n; ++i)
        acc = accumOp(acc, readElemAsComplex(x, i, typeMatches));
    return Value::complexScalar(finalize(acc, n), alloc);
}

template <typename Op>
Value runComplexReduction(const Value &x, int dim, Allocator *alloc, bool isAll = false)
{
    if (isAll)
        return reduceComplexAllElementsScalar(x, alloc, Op::cInit(), Op::cAccum, Op::cFinalize);
    return (dim > 0)
        ? reduceComplexAlongDim(x, dim, alloc, Op::cInit(), Op::cAccum, Op::cFinalize)
        : reduceComplexAll(x, alloc, Op::cInit(), Op::cAccum, Op::cFinalize);
}

template <typename Op>
Value runNativeReduction(const Value &x, int dim, ValueType outType, Allocator *alloc,
                          bool isAll = false)
{
    auto run = [&](auto tag) {
        using T = decltype(tag);
        if (isAll)
            return reduceAllElementsScalar<T>(x, outType, alloc,
                                              Op::init, Op::accum, Op::template finalize<T>);
        return (dim > 0)
            ? reduceTypedAlongDim<T>(x, dim, outType, alloc,
                                     Op::init, Op::accum, Op::template finalize<T>)
            : reduceTypedAll<T>(x, outType, alloc,
                                Op::init, Op::accum, Op::template finalize<T>);
    };
    switch (outType) {
    case ValueType::DOUBLE: return run(double  {});
    case ValueType::SINGLE: return run(float   {});
    case ValueType::INT8:   return run(int8_t  {});
    case ValueType::INT16:  return run(int16_t {});
    case ValueType::INT32:  return run(int32_t {});
    case ValueType::INT64:  return run(int64_t {});
    case ValueType::UINT8:  return run(uint8_t {});
    case ValueType::UINT16: return run(uint16_t{});
    case ValueType::UINT32: return run(uint32_t{});
    case ValueType::UINT64: return run(uint64_t{});
    default:
        throw Error("internal: unsupported native output type",
                     0, 0, "", "", "m:nativeReduce:type");
    }
}

inline std::string lowercaseStr(const Value &v)
{
    std::string s = v.toString();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

inline bool isAllString(const Value &v)
{
    return isStringArg(v) && lowercaseStr(v) == "all";
}

enum class StringFlag { Unknown, All, OutType, NanFlag };

inline StringFlag classifyStringFlag(const Value &v)
{
    if (!isStringArg(v)) return StringFlag::Unknown;
    const std::string s = lowercaseStr(v);
    if (s == "all") return StringFlag::All;
    if (s == "default" || s == "double" || s == "native") return StringFlag::OutType;
    if (s == "omitnan" || s == "includenan") return StringFlag::NanFlag;
    return StringFlag::Unknown;
}

// ── NaN-aware reductions (omitnan flag) ──────────────────────────────
//
// MATLAB modern API: sum/mean/prod/var/std/min/max accept 'omitnan' as
// a flag that skips NaN inputs. This is independent of the existing
// nansum/nanmean entry points (those stay for backward compatibility).
//
// For non-floating types (integer/logical/char) NaN can't occur, so
// 'omitnan' is identical to 'includenan' (the default).
//
// For COMPLEX, an element is NaN if either its real or imag part is NaN.

template <typename Op>
inline void nanAccumDouble(double &acc, size_t &count, double v)
{
    if (std::isnan(v)) return;
    acc = Op::accum(acc, v);
    ++count;
}

template <typename T, typename Op>
inline T nanFinalize(double acc, size_t nonNanCount)
{
    if constexpr (std::is_same_v<Op, MeanOp>)
        return toOutT<T>(nonNanCount == 0 ? std::nan("")
                                          : acc / static_cast<double>(nonNanCount));
    else
        // sum: empty → 0 (matches MATLAB nansum). prod: empty → 1.
        return toOutT<T>(acc);
}

template <typename T, typename Op>
void nanReduceAlongDim(const Value &x, int redDim, T *dst)
{
    const auto &d = x.dims();
    const int redAxis = redDim - 1;
    const size_t sliceLen = d.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < d.ndim(); ++i) O *= d.dim(i);

    auto reduceSlice = [&](size_t baseOff, size_t stride) -> T {
        double acc = Op::init;
        size_t count = 0;
        for (size_t k = 0; k < sliceLen; ++k)
            nanAccumDouble<Op>(acc, count, x.elemAsDouble(baseOff + k * stride));
        return nanFinalize<T, Op>(acc, count);
    };
    if (B == 1) {
        for (size_t o = 0; o < O; ++o) dst[o] = reduceSlice(o * sliceLen, 1);
        return;
    }
    for (size_t o = 0; o < O; ++o)
        for (size_t b = 0; b < B; ++b)
            dst[o * B + b] = reduceSlice(o * sliceLen * B + b, B);
}

template <typename T, typename Op>
Value nanReduceAll(const Value &x, ValueType outType, Allocator *alloc)
{
    if (x.isEmpty() && x.dims().ndim() < 4)
        return Value::matrix(0, 0, outType, alloc);
    if (x.isScalar() || x.dims().isVector()) {
        double acc = Op::init;
        size_t count = 0;
        for (size_t i = 0; i < x.numel(); ++i)
            nanAccumDouble<Op>(acc, count, x.elemAsDouble(i));
        return makeNativeScalar<T>(nanFinalize<T, Op>(acc, count), outType, alloc);
    }
    const int redDim = detail::firstNonSingletonDim(x);
    Value out = allocReduceOutput(x, redDim, outType, alloc);
    nanReduceAlongDim<T, Op>(x, redDim, static_cast<T *>(out.rawDataMut()));
    return out;
}

template <typename T, typename Op>
Value nanReduceAlongDimImpl(const Value &x, int dim, ValueType outType, Allocator *alloc)
{
    if (x.isEmpty() && x.dims().ndim() < 4)
        return Value::matrix(0, 0, outType, alloc);
    if (x.isScalar() || x.dims().isVector()) {
        if (dim != detail::firstNonSingletonDim(x)) {
            // Identity: copy x as outType (cast where needed).
            const size_t n = x.numel();
            if (!x.dims().isVector())
                return makeNativeScalar<T>(toOutT<T>(x.elemAsDouble(0)), outType, alloc);
            Value out = createMatrix({x.dims().rows(), x.dims().cols(), 0}, outType, alloc);
            T *dst = static_cast<T *>(out.rawDataMut());
            for (size_t i = 0; i < n; ++i) dst[i] = toOutT<T>(x.elemAsDouble(i));
            return out;
        }
        return nanReduceAll<T, Op>(x, outType, alloc);
    }
    Value out = allocReduceOutput(x, dim, outType, alloc);
    nanReduceAlongDim<T, Op>(x, dim, static_cast<T *>(out.rawDataMut()));
    return out;
}

template <typename T, typename Op>
Value nanReduceAllElementsScalar(const Value &x, ValueType outType, Allocator *alloc)
{
    double acc = Op::init;
    size_t count = 0;
    for (size_t i = 0; i < x.numel(); ++i)
        nanAccumDouble<Op>(acc, count, x.elemAsDouble(i));
    return makeNativeScalar<T>(nanFinalize<T, Op>(acc, count), outType, alloc);
}

template <typename Op>
Value runNanReduction(const Value &x, int dim, ValueType outType, Allocator *alloc,
                       bool isAll = false)
{
    auto run = [&](auto tag) {
        using T = decltype(tag);
        if (isAll)
            return nanReduceAllElementsScalar<T, Op>(x, outType, alloc);
        return (dim > 0)
            ? nanReduceAlongDimImpl<T, Op>(x, dim, outType, alloc)
            : nanReduceAll<T, Op>(x, outType, alloc);
    };
    switch (outType) {
    case ValueType::DOUBLE: return run(double  {});
    case ValueType::SINGLE: return run(float   {});
    case ValueType::INT8:   return run(int8_t  {});
    case ValueType::INT16:  return run(int16_t {});
    case ValueType::INT32:  return run(int32_t {});
    case ValueType::INT64:  return run(int64_t {});
    case ValueType::UINT8:  return run(uint8_t {});
    case ValueType::UINT16: return run(uint16_t{});
    case ValueType::UINT32: return run(uint32_t{});
    case ValueType::UINT64: return run(uint64_t{});
    default:
        throw Error("internal: unsupported nan-aware output type",
                     0, 0, "", "", "m:nanReduce:type");
    }
}

// COMPLEX nan-aware reduction: an element is NaN if either its real
// or imag part is NaN.
inline bool isComplexNaN(Complex c)
{
    return std::isnan(c.real()) || std::isnan(c.imag());
}

template <typename Op>
Value runComplexNanReduction(const Value &x, int dim, Allocator *alloc,
                              bool isAll = false)
{
    const bool typeMatches = (x.type() == ValueType::COMPLEX);
    auto readC = [&](size_t i) { return readElemAsComplex(x, i, typeMatches); };

    auto finalizeOne = [](Complex acc, size_t count) -> Complex {
        if constexpr (std::is_same_v<Op, MeanOp>)
            return count == 0 ? Complex(std::nan(""), 0.0)
                              : acc / static_cast<double>(count);
        else
            return acc;
    };
    auto reduceRange = [&](size_t baseOff, size_t stride, size_t n) -> Complex {
        Complex acc = Op::cInit();
        size_t count = 0;
        for (size_t k = 0; k < n; ++k) {
            const Complex v = readC(baseOff + k * stride);
            if (isComplexNaN(v)) continue;
            acc = Op::cAccum(acc, v);
            ++count;
        }
        return finalizeOne(acc, count);
    };

    if (isAll || x.isScalar() || x.dims().isVector()) {
        return Value::complexScalar(reduceRange(0, 1, x.numel()), alloc);
    }
    const int d = (dim > 0) ? dim : detail::firstNonSingletonDim(x);
    Value out = allocComplexReduceOutput(x, d, alloc);
    Complex *dst = out.complexDataMut();

    const auto &dd = x.dims();
    const int redAxis = d - 1;
    const size_t sliceLen = dd.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= dd.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < dd.ndim(); ++i) O *= dd.dim(i);

    if (B == 1) {
        for (size_t o = 0; o < O; ++o)
            dst[o] = reduceRange(o * sliceLen, 1, sliceLen);
    } else {
        for (size_t o = 0; o < O; ++o)
            for (size_t b = 0; b < B; ++b)
                dst[o * B + b] = reduceRange(o * sliceLen * B + b, B, sliceLen);
    }
    return out;
}

struct ReductionFlags {
    int          dim     = 0;
    bool         isAll   = false;
    OutTypeMode  outMode = OutTypeMode::Default;
    bool         omitNan = false;
};

// Parse args[1..] and populate the reduction-flag struct. Each kind of
// flag (dim/all, outtype, nanflag) appears at most once. Strings are
// classified by their content, so order is flexible.
ReductionFlags parseReductionFlags(Span<const Value> args, const char *fn)
{
    ReductionFlags r;
    bool haveDim = false;     // either explicit dim or 'all'
    bool haveOutType = false;
    bool haveNanFlag = false;
    for (size_t i = 1; i < args.size(); ++i) {
        const Value &a = args[i];
        if (a.isEmpty()) continue;
        if (isStringArg(a)) {
            switch (classifyStringFlag(a)) {
            case StringFlag::All:
                if (haveDim)
                    throw Error(std::string(fn) + ": dim specified twice",
                                 0, 0, fn, "", std::string("m:") + fn + ":dupDim");
                r.isAll = true; haveDim = true; break;
            case StringFlag::OutType:
                if (haveOutType)
                    throw Error(std::string(fn) + ": output type specified twice",
                                 0, 0, fn, "", std::string("m:") + fn + ":outtypeDup");
                r.outMode = parseOutTypeMode(a, fn); haveOutType = true; break;
            case StringFlag::NanFlag:
                if (haveNanFlag)
                    throw Error(std::string(fn) + ": nan flag specified twice",
                                 0, 0, fn, "", std::string("m:") + fn + ":nanFlagDup");
                r.omitNan = (lowercaseStr(a) == "omitnan");
                haveNanFlag = true; break;
            default:
                throw Error(std::string(fn) + ": unknown flag '" + lowercaseStr(a) + "'",
                             0, 0, fn, "", std::string("m:") + fn + ":badFlag");
            }
        } else {
            if (haveDim)
                throw Error(std::string(fn) + ": dim specified twice",
                             0, 0, fn, "", std::string("m:") + fn + ":dupDim");
            r.dim = static_cast<int>(a.toScalar());
            haveDim = true;
        }
    }
    return r;
}

// Parse args and pick the right reduction path.
//   defaultFn — DOUBLE output (existing fast path)
//   nativeFn  — typed pipeline (covers 'native' and SINGLE-preserving default)
//   complexFn — COMPLEX accumulator path
//   nanFn     — NaN-aware typed pipeline (for omitnan)
//   nanCFn    — NaN-aware complex path (for omitnan + complex)
//
// MATLAB syntax handled:
//   fn(X)                                — default reduce
//   fn(X, dim)                           — along dim
//   fn(X, 'all')                         — scalar reduce
//   fn(X, ..., outtype)                  — outtype ∈ {default, double, native}
//   fn(X, ..., 'omitnan' | 'includenan') — modern nan flag
//
// Output-type rules:
//   Default: SINGLE→SINGLE, COMPLEX→COMPLEX, others→DOUBLE
//   Double:  COMPLEX→COMPLEX, others→DOUBLE
//   Native:  preserve class; LOGICAL/CHAR rejected.
template <typename DefaultFn, typename NativeFn, typename ComplexFn,
          typename NanFn, typename NanComplexFn>
Value dispatchReductionAdapter(Span<const Value> args, const char *fn,
                                DefaultFn defaultFn, NativeFn nativeFn,
                                ComplexFn complexFn, NanFn nanFn,
                                NanComplexFn nanCFn)
{
    const ReductionFlags f = parseReductionFlags(args, fn);
    const ValueType inT = args[0].type();

    // COMPLEX input always preserves complex type. Branch on omitnan.
    if (inT == ValueType::COMPLEX)
        return f.omitNan ? nanCFn(args[0], f.dim, f.isAll)
                         : complexFn(args[0], f.dim, f.isAll);

    // Resolve the output type for typed/nan pipelines.
    auto resolveOut = [&]() -> ValueType {
        if (f.outMode == OutTypeMode::Native) return resolveNativeOutType(inT, fn);
        if (f.outMode == OutTypeMode::Default && inT == ValueType::SINGLE) return ValueType::SINGLE;
        return ValueType::DOUBLE;
    };

    if (f.omitNan) {
        const ValueType outT = resolveOut();
        return nanFn(args[0], f.dim, outT, f.isAll);
    }

    if (f.outMode == OutTypeMode::Native) {
        const ValueType outT = resolveNativeOutType(inT, fn);
        return nativeFn(args[0], f.dim, outT, f.isAll);
    }
    if (f.outMode == OutTypeMode::Default && inT == ValueType::SINGLE)
        return nativeFn(args[0], f.dim, ValueType::SINGLE, f.isAll);
    return defaultFn(args[0], f.dim, f.isAll);
}

} // namespace

#define NK_REDUCTION_ADAPTER(name, fn, op)                                       \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                 \
                    Span<Value> outs, CallContext &ctx)                         \
    {                                                                             \
        if (args.empty())                                                         \
            throw Error(#name ": requires at least 1 argument",                  \
                         0, 0, #name, "", "m:" #name ":nargin");                  \
        outs[0] = dispatchReductionAdapter(args, #name,                           \
            [&](const Value &x, int dim, bool isAll) {                           \
                if (isAll)                                                        \
                    return runNativeReduction<op>(x, 0, ValueType::DOUBLE,            \
                                                  &ctx.engine->allocator(), true);\
                return (dim > 0) ? fn(ctx.engine->allocator(), x, dim)            \
                                 : fn(ctx.engine->allocator(), x);                \
            },                                                                    \
            [&](const Value &x, int dim, ValueType outT, bool isAll) {               \
                return runNativeReduction<op>(x, dim, outT,                       \
                                              &ctx.engine->allocator(), isAll);   \
            },                                                                    \
            [&](const Value &x, int dim, bool isAll) {                           \
                return runComplexReduction<op>(x, dim,                            \
                                               &ctx.engine->allocator(), isAll);  \
            },                                                                    \
            [&](const Value &x, int dim, ValueType outT, bool isAll) {               \
                return runNanReduction<op>(x, dim, outT,                          \
                                           &ctx.engine->allocator(), isAll);      \
            },                                                                    \
            [&](const Value &x, int dim, bool isAll) {                           \
                return runComplexNanReduction<op>(x, dim,                         \
                                                  &ctx.engine->allocator(), isAll);\
            });                                                                   \
    }

NK_REDUCTION_ADAPTER(sum,  sum,  SumOp)
NK_REDUCTION_ADAPTER(prod, prod, ProdOp)
NK_REDUCTION_ADAPTER(mean, mean, MeanOp)

#undef NK_REDUCTION_ADAPTER

// hypot / nthroot / expm1 / log1p adapters → math/elementary/{misc,exponents}.cpp
// gamma / gammaln / erf / erfc / erfinv adapters → math/elementary/special.cpp
// atan2 adapter → math/elementary/trigonometry.cpp
// mod / rem adapters → math/elementary/misc.cpp

// max/min: MATLAB forms:
//   max(X)                       — reduction along first non-singleton dim, (value, idx)
//   max(A, B)                    — elementwise (with broadcasting), single return
//   max(X, [], dim)              — reduction along explicit dim, (value, idx)
//   max(X, [], dim, 'omitnan')   — same as above, ignoring NaN
//   max(X, [], 'omitnan')        — reduction with default dim, ignoring NaN
// Trailing 'omitnan' / 'includenan' string is recognised in the reduction
// form. Binary form max(A, B) currently doesn't accept omitnan (deferred).
namespace {

// Detect optional trailing 'omitnan'/'includenan' string in the reduction
// form, returning the effective arg count (excluding the flag if present)
// and the omit flag.
size_t stripTrailingNanFlag(Span<const Value> args, bool &omitNan)
{
    omitNan = false;
    size_t n = args.size();
    if (n == 0) return 0;
    const Value &last = args[n - 1];
    if (last.type() != ValueType::CHAR) return n;
    std::string s = last.toString();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (s == "omitnan") { omitNan = true; return n - 1; }
    if (s == "includenan")               return n - 1;
    return n;
}

} // namespace

void max_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("max: requires at least 1 argument",
                     0, 0, "max", "", "m:max:nargin");
    bool omitNan = false;
    const size_t n = stripTrailingNanFlag(args, omitNan);
    if (n >= 2 && !args[1].isEmpty()) {
        // Elementwise max(A, B) — single-return form. NaN-aware variant
        // when 'omitnan' was passed.
        outs[0] = omitNan
            ? maxOmitNanBinary(ctx.engine->allocator(), args[0], args[1])
            : max(ctx.engine->allocator(), args[0], args[1]);
        return;
    }
    // Reduction: optional dim as args[2].
    int dim = 0;
    if (n >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    auto [val, idx] = omitNan
        ? maxOmitNan(ctx.engine->allocator(), args[0], dim)
        : max(ctx.engine->allocator(), args[0], dim);
    outs[0] = std::move(val);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

void min_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("min: requires at least 1 argument",
                     0, 0, "min", "", "m:min:nargin");
    bool omitNan = false;
    const size_t n = stripTrailingNanFlag(args, omitNan);
    if (n >= 2 && !args[1].isEmpty()) {
        outs[0] = omitNan
            ? minOmitNanBinary(ctx.engine->allocator(), args[0], args[1])
            : min(ctx.engine->allocator(), args[0], args[1]);
        return;
    }
    int dim = 0;
    if (n >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    auto [val, idx] = omitNan
        ? minOmitNan(ctx.engine->allocator(), args[0], dim)
        : min(ctx.engine->allocator(), args[0], dim);
    outs[0] = std::move(val);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

// Generators
void linspace_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("linspace: requires at least 2 arguments",
                     0, 0, "linspace", "", "m:linspace:nargin");
    const double a = args[0].toScalar();
    const double b = args[1].toScalar();
    const size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 100u;
    outs[0] = linspace(ctx.engine->allocator(), a, b, n);
}

void logspace_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("logspace: requires at least 2 arguments",
                     0, 0, "logspace", "", "m:logspace:nargin");
    const double a = args[0].toScalar();
    const double b = args[1].toScalar();
    const size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 50u;
    outs[0] = logspace(ctx.engine->allocator(), a, b, n);
}

// rand_reg / randn_reg moved to rng.cpp — they share a single
// process-static engine with randi / randperm so MATLAB-style
// rng(seed) controls all of them. The C++ public APIs rand(alloc,
// rng, …) / randn(alloc, rng, …) above stay here unchanged.

} // namespace detail

} // namespace numkit::builtin
