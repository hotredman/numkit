// libs/builtin/src/data_analysis/descriptive_statistics/stats.cpp
//
// Phase-1 stats: var, std, median, quantile, prctile, mode.
// All take an explicit `dim` argument (1-based, or 0 for "first
// non-singleton"). Implementations route through applyAlongDim from
// reduction_helpers.hpp, which handles vector/2D/3D layouts uniformly.

#include <numkit/builtin/data_analysis/descriptive_statistics/stats.hpp>

#include <numkit/stats/nan_aware/nan_aware.hpp>  // var_reg / std_reg / median_reg dispatch into stats:: when 'omitnan' is given

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "reduction_helpers.hpp"
#include "backends/nan_reductions.hpp"
#include "backends/var_reduction.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace numkit::builtin {

using detail::applyAlongDim;
using detail::resolveDim;

// ────────────────────────────────────────────────────────────────────
// var / std
// ────────────────────────────────────────────────────────────────────
//
// Phase P5 + P2-followup: var / std / nanvar / nanstd all route through
// the SIMD two-pass kernels in backends/MStdVarReduction_{simd,portable}.cpp
// and MStdNanReductions_{simd,portable}.cpp. Welford's recurrence
// (numerically pristine but fully serial) is no longer used here.
namespace {

void validateNormFlag(int w, const char *fn)
{
    if (w != 0 && w != 1)
        throw Error(std::string(fn) + ": normalization flag must be 0 or 1",
                     0, 0, fn, "", std::string("m:") + fn + ":badFlag");
}

// Cast a DOUBLE result to SINGLE in place. Used to preserve SINGLE
// input type without writing parallel single-precision kernels —
// arithmetic happens at double precision (more precise than MATLAB)
// then narrows at the end.
Value narrowToSingle(Value d, Allocator *alloc)
{
    if (d.type() != ValueType::DOUBLE) return d;
    Value r = createForDims(d.dims(), ValueType::SINGLE, alloc);
    const double *src = d.doubleData();
    float *dst = r.singleDataMut();
    for (size_t i = 0; i < d.numel(); ++i)
        dst[i] = static_cast<float>(src[i]);
    return r;
}

// Complex variance: E[|x - mean|²]. Returns real-valued DOUBLE per
// MATLAB convention. With normFlag == 0 (default) divides by n-1;
// normFlag == 1 divides by n. When omitNan is true, NaN-complex
// elements (real or imag is NaN) are skipped before mean/variance.
inline bool isComplexNaNStats(Complex c)
{
    return std::isnan(c.real()) || std::isnan(c.imag());
}

double complexVarianceFromSlice(const Complex *data, size_t n, int normFlag,
                                bool omitNan = false)
{
    Complex mean(0.0, 0.0);
    size_t k = 0;
    for (size_t i = 0; i < n; ++i) {
        if (omitNan && isComplexNaNStats(data[i])) continue;
        mean += data[i];
        ++k;
    }
    if (k == 0) return std::nan("");
    if (k == 1) return (normFlag == 0) ? std::nan("") : 0.0;
    mean /= static_cast<double>(k);
    double acc = 0.0;
    for (size_t i = 0; i < n; ++i) {
        if (omitNan && isComplexNaNStats(data[i])) continue;
        const Complex d = data[i] - mean;
        acc += d.real() * d.real() + d.imag() * d.imag();
    }
    const double divisor = static_cast<double>(k - (normFlag == 0 ? 1 : 0));
    return acc / divisor;
}

inline Value allocVarianceOutput(const Value &x, int redDim, Allocator *alloc)
{
    if (x.dims().ndim() >= 4 && redDim >= 1 && redDim <= x.dims().ndim()) {
        auto shape = detail::outShapeForDimND(x, redDim);
        return Value::matrixND(shape.data(), (int) shape.size(), ValueType::DOUBLE, alloc);
    }
    auto outShape = detail::outShapeForDim(x, redDim);
    return createMatrix(outShape, ValueType::DOUBLE, alloc);
}

// Complex variance along dim: walks slices via stride math, gathers
// each slice into a Complex scratch buffer, computes per-slice complex
// variance, writes DOUBLE output.
void complexVarianceAlongDim(const Value &x, int redDim, double *dst, int normFlag,
                             bool omitNan = false)
{
    const auto &d = x.dims();
    const int redAxis = redDim - 1;
    const size_t sliceLen = d.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < d.ndim(); ++i) O *= d.dim(i);

    const Complex *src = x.complexData();
    std::vector<Complex> scratch(sliceLen);
    auto reduceSlice = [&](size_t outOff, size_t baseOff, size_t stride) {
        for (size_t k = 0; k < sliceLen; ++k)
            scratch[k] = src[baseOff + k * stride];
        dst[outOff] = complexVarianceFromSlice(scratch.data(), sliceLen, normFlag, omitNan);
    };
    if (B == 1) {
        for (size_t o = 0; o < O; ++o) reduceSlice(o, o * sliceLen, 1);
        return;
    }
    for (size_t o = 0; o < O; ++o)
        for (size_t b = 0; b < B; ++b)
            reduceSlice(o * B + b, o * sliceLen * B + b, B);
}

Value varianceComplex(const Value &x, int normFlag, int dim,
                       Allocator *alloc, bool sqrtIt, bool omitNan = false)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, alloc);
    const Complex *src = x.complexData();
    if (x.isScalar() || x.dims().isVector()) {
        double v = complexVarianceFromSlice(src, x.numel(), normFlag, omitNan);
        if (sqrtIt && !std::isnan(v)) v = std::sqrt(v);
        return Value::scalar(v, alloc);
    }
    const int d = (dim > 0) ? dim : detail::firstNonSingletonDim(x);
    Value out = allocVarianceOutput(x, d, alloc);
    complexVarianceAlongDim(x, d, out.doubleDataMut(), normFlag, omitNan);
    if (sqrtIt) {
        double *p = out.doubleDataMut();
        const size_t n = out.numel();
        for (size_t i = 0; i < n; ++i)
            if (!std::isnan(p[i])) p[i] = std::sqrt(p[i]);
    }
    return out;
}

} // namespace

Value var(Allocator &alloc, const Value &x, int normFlag, int dim)
{
    validateNormFlag(normFlag, "var");
    if (x.type() == ValueType::COMPLEX)
        return varianceComplex(x, normFlag, dim, &alloc, /*sqrtIt=*/false);
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(varianceTwoPass(x.doubleData(), x.numel(), normFlag), &alloc);

    const int d = resolveDim(x, dim, "var");
    Value r = applyAlongDim(x, d,
        [normFlag](size_t, double *slice, size_t n) {
            return varianceTwoPass(slice, n, normFlag);
        }, &alloc);
    if (x.type() == ValueType::SINGLE)
        r = narrowToSingle(std::move(r), &alloc);
    return r;
}

Value stdev(Allocator &alloc, const Value &x, int normFlag, int dim)
{
    validateNormFlag(normFlag, "std");
    if (x.type() == ValueType::COMPLEX)
        return varianceComplex(x, normFlag, dim, &alloc, /*sqrtIt=*/true);
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(std::sqrt(varianceTwoPass(x.doubleData(), x.numel(), normFlag)), &alloc);

    const int d = resolveDim(x, dim, "std");
    Value r = applyAlongDim(x, d,
        [normFlag](size_t, double *slice, size_t n) {
            return std::sqrt(varianceTwoPass(slice, n, normFlag));
        }, &alloc);
    if (x.type() == ValueType::SINGLE)
        r = narrowToSingle(std::move(r), &alloc);
    return r;
}

// ────────────────────────────────────────────────────────────────────
// median
// ────────────────────────────────────────────────────────────────────
//
// nth_element gives O(n) average instead of O(n log n) full sort.
// The slice is mutated in place — that's fine because the scratch
// buffer is owned by forEachSlice and reused per output index.
namespace {

double medianFromSlice(double *data, size_t n)
{
    if (n == 0) return std::nan("");
    if (n == 1) return data[0];
    const size_t mid = n / 2;
    std::nth_element(data, data + mid, data + n);
    if (n % 2 == 1)
        return data[mid];
    // Even count: average of the two middles. The "lower middle" is
    // max(data[0..mid-1]) after partial sort: nth_element guarantees
    // data[0..mid-1] all <= data[mid]; we need the largest of those.
    const double upper = data[mid];
    const double lower = *std::max_element(data, data + mid);
    return 0.5 * (lower + upper);
}

} // namespace

Value median(Allocator &alloc, const Value &x, int dim)
{
    if (x.type() == ValueType::COMPLEX)
        throw Error("median: complex inputs are not supported (no defined ordering)",
                     0, 0, "median", "", "m:median:complex");
    const int d = resolveDim(x, dim, "median");
    Value r = applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            return medianFromSlice(slice, n);
        }, &alloc);
    if (x.type() == ValueType::SINGLE)
        r = narrowToSingle(std::move(r), &alloc);
    return r;
}

// ────────────────────────────────────────────────────────────────────
// quantile / prctile
// ────────────────────────────────────────────────────────────────────
//
// Default linear-interpolation method (MATLAB's default for quantile
// and prctile). For probability p in [0,1] and a slice of length n,
// position h = p*(n-1) (0-based). Lower index = floor(h), upper =
// ceil(h), result = lower_val + (h - floor(h)) * (upper_val - lower_val).
//
// Implementation note: when p is a vector, the slice is fully sorted
// once and then queried at each p. When p is a scalar, nth_element on
// floor(h) and ceil(h) is faster, but the difference is small for the
// typical use case.
namespace {

double quantileFromSortedSlice(const double *sorted, size_t n, double p)
{
    if (n == 0) return std::nan("");
    if (n == 1) return sorted[0];
    if (!std::isfinite(p)) return std::nan("");
    if (p <= 0.0) return sorted[0];
    if (p >= 1.0) return sorted[n - 1];
    const double h = p * static_cast<double>(n - 1);
    const size_t lo = static_cast<size_t>(std::floor(h));
    const size_t hi = static_cast<size_t>(std::ceil(h));
    const double frac = h - static_cast<double>(lo);
    return sorted[lo] + frac * (sorted[hi] - sorted[lo]);
}

// quantile/prctile share the bulk of the logic. `pScale` converts the
// raw user input to a [0,1] probability (1.0 for quantile, 1/100 for
// prctile).
Value quantileImpl(Allocator &alloc, const Value &x, const Value &p,
                    int dim, double pScale, const char *fn)
{
    if (p.numel() == 0)
        throw Error(std::string(fn) + ": p must be non-empty",
                     0, 0, fn, "", std::string("m:") + fn + ":emptyP");

    // Normalize probabilities into a flat vector (so prctile sees /100)
    // then validate the post-scaling value lies in [0,1].
    std::vector<double> probs(p.numel());
    for (size_t i = 0; i < p.numel(); ++i) {
        probs[i] = p.doubleData()[i] * pScale;
        if (!(probs[i] >= 0.0 && probs[i] <= 1.0))
            throw Error(std::string(fn) + ": probabilities out of range",
                         0, 0, fn, "", std::string("m:") + fn + ":badProb");
    }

    const size_t k = probs.size();
    const int d = resolveDim(x, dim, fn);

    // Scalar p → standard one-output-per-slice path.
    if (k == 1) {
        const double pp = probs[0];
        return applyAlongDim(x, d,
            [pp](size_t, double *slice, size_t n) {
                std::sort(slice, slice + n);
                return quantileFromSortedSlice(slice, n, pp);
            }, &alloc);
    }

    // Vector p: each slice produces k outputs along the reduced dim.
    // We need to allocate the output explicitly because applyAlongDim
    // assumes 1 output per slice.
    if (x.isEmpty()) {
        return Value::matrix(0, 0, ValueType::DOUBLE, &alloc);
    }
    if (x.dims().isVector() || x.isScalar()) {
        // Output is 1×k row vector.
        std::vector<double> sorted(x.numel());
        std::copy(x.doubleData(), x.doubleData() + x.numel(), sorted.data());
        std::sort(sorted.begin(), sorted.end());
        auto out = Value::matrix(1, k, ValueType::DOUBLE, &alloc);
        for (size_t i = 0; i < k; ++i)
            out.doubleDataMut()[i] = quantileFromSortedSlice(sorted.data(),
                                                              sorted.size(),
                                                              probs[i]);
        return out;
    }

    // Build output shape: same as input but the reduced dim has size k.
    const auto &dd = x.dims();
    DimsArg outShape{dd.rows(), dd.cols(),
                     dd.is3D() ? dd.pages() : 0};
    switch (d) {
        case 1: outShape.rows  = k; break;
        case 2: outShape.cols  = k; break;
        case 3: outShape.pages = k; break;
        default: break;
    }
    auto out = createMatrix(outShape, ValueType::DOUBLE, &alloc);
    double *dst = out.doubleDataMut();

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    const size_t outR = outShape.rows, outC = outShape.cols;
    const size_t outP = outShape.pages == 0 ? 1 : outShape.pages;
    const size_t N = detail::sliceLenForDim(x, d);
    std::vector<double> sorted(N);
    const double *src = x.doubleData();

    auto writeOut = [&](size_t rr, size_t cc, size_t pp, double v) {
        dst[pp * outR * outC + cc * outR + rr] = v;
    };

    if (d == 1) {
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                const double *base = src + pp * R * C + c * R;
                std::copy(base, base + N, sorted.data());
                std::sort(sorted.begin(), sorted.end());
                for (size_t i = 0; i < k; ++i)
                    writeOut(i, c, pp,
                             quantileFromSortedSlice(sorted.data(), N, probs[i]));
            }
    } else if (d == 2) {
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t r = 0; r < R; ++r) {
                for (size_t c = 0; c < C; ++c)
                    sorted[c] = src[pp * R * C + c * R + r];
                std::sort(sorted.begin(), sorted.end());
                for (size_t i = 0; i < k; ++i)
                    writeOut(r, i, pp,
                             quantileFromSortedSlice(sorted.data(), N, probs[i]));
            }
    } else if (d == 3) {
        for (size_t c = 0; c < C; ++c)
            for (size_t r = 0; r < R; ++r) {
                for (size_t pp = 0; pp < P; ++pp)
                    sorted[pp] = src[pp * R * C + c * R + r];
                std::sort(sorted.begin(), sorted.end());
                for (size_t i = 0; i < k; ++i)
                    writeOut(r, c, i,
                             quantileFromSortedSlice(sorted.data(), N, probs[i]));
            }
    }
    return out;
}

} // namespace

Value quantile(Allocator &alloc, const Value &x, const Value &p, int dim)
{
    return quantileImpl(alloc, x, p, dim, 1.0, "quantile");
}

Value prctile(Allocator &alloc, const Value &x, const Value &p, int dim)
{
    return quantileImpl(alloc, x, p, dim, 0.01, "prctile");
}

// ────────────────────────────────────────────────────────────────────
// mode
// ────────────────────────────────────────────────────────────────────
//
// MATLAB rule: mode preserves the input element type. The value array
// has the same type as input (DOUBLE/SINGLE/INT*/UINT*/LOGICAL/CHAR);
// the frequency array is always DOUBLE. NaN values are ignored when
// counting (floating types only — integers have no NaN). Ties resolve
// to the smallest value: we sort ascending, then use strict-greater
// comparison so the first run achieving the max count wins.

namespace {

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

template <typename T>
inline T modeEmptyValue()
{
    if constexpr (std::is_floating_point_v<T>)
        return std::numeric_limits<T>::quiet_NaN();
    else
        return T{};
}

template <typename T>
inline size_t compactNonNanT(T *data, size_t n)
{
    if constexpr (std::is_floating_point_v<T>) {
        size_t w = 0;
        for (size_t i = 0; i < n; ++i)
            if (!std::isnan(data[i])) {
                if (w != i) data[w] = data[i];
                ++w;
            }
        return w;
    } else {
        (void) data;
        return n;
    }
}

template <typename T>
inline void modeFromSliceT(T *data, size_t n, T &outVal, double &outCount)
{
    const size_t k = compactNonNanT<T>(data, n);
    if (k == 0) {
        outVal = modeEmptyValue<T>();
        outCount = 0.0;
        return;
    }
    std::sort(data, data + k);
    T bestVal = data[0];
    size_t bestCount = 1;
    T curVal = data[0];
    size_t curCount = 1;
    for (size_t i = 1; i < k; ++i) {
        if (data[i] == curVal) {
            ++curCount;
        } else {
            if (curCount > bestCount) { bestCount = curCount; bestVal = curVal; }
            curVal = data[i];
            curCount = 1;
        }
    }
    if (curCount > bestCount) { bestCount = curCount; bestVal = curVal; }
    outVal = bestVal;
    outCount = static_cast<double>(bestCount);
}

// Walk every output cell along dim `redDim` (1-based). For each cell,
// gather the slice into `scratch`, run modeFromSliceT, and write the
// (value, count) pair. Handles 2D / 3D / ND uniformly via stride math.
// On empty slices (sliceLen == 0) fills outputs with NaN/0 (or 0/0 for
// integer T) — matches MATLAB's mode-of-empty-slice convention.
template <typename T>
void modeAlongDim(const Value &x, int redDim, T *dst, double *dstC,
                  bool typeMatch)
{
    const auto &d = x.dims();
    const int redAxis = redDim - 1;
    const size_t sliceLen = d.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < d.ndim(); ++i) O *= d.dim(i);

    if (sliceLen == 0) {
        const T defVal = modeEmptyValue<T>();
        const size_t total = B * O;
        for (size_t i = 0; i < total; ++i) { dst[i] = defVal; dstC[i] = 0.0; }
        return;
    }

    std::vector<T> scratch(sliceLen);
    auto runSlice = [&](size_t outIdx, size_t baseOff, size_t stride) {
        for (size_t k = 0; k < sliceLen; ++k)
            scratch[k] = readSrcAsT<T>(x, baseOff + k * stride, typeMatch);
        T v; double c;
        modeFromSliceT<T>(scratch.data(), sliceLen, v, c);
        dst[outIdx] = v;
        dstC[outIdx] = c;
    };

    if (B == 1) {
        for (size_t o = 0; o < O; ++o) runSlice(o, o * sliceLen, 1);
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

inline std::pair<Value, Value>
allocModeOutputs(const Value &x, int redDim, ValueType outType, Allocator *alloc)
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

template <typename T>
std::tuple<Value, Value>
modeAllT(const Value &x, ValueType outType, Allocator *alloc)
{
    const bool typeMatch = (x.type() == outType);
    if (x.isEmpty() && x.dims().ndim() < 4) {
        return std::make_tuple(Value::matrix(0, 0, outType, alloc),
                               Value::matrix(0, 0, ValueType::DOUBLE, alloc));
    }
    if (x.isScalar() || x.dims().isVector()) {
        std::vector<T> scratch(x.numel());
        for (size_t i = 0; i < x.numel(); ++i)
            scratch[i] = readSrcAsT<T>(x, i, typeMatch);
        T v; double c;
        modeFromSliceT<T>(scratch.data(), x.numel(), v, c);
        return std::make_tuple(makeScalarT<T>(v, outType, alloc),
                               Value::scalar(c, alloc));
    }
    const int redDim = detail::firstNonSingletonDim(x);
    auto [out, outC] = allocModeOutputs(x, redDim, outType, alloc);
    modeAlongDim<T>(x, redDim,
                    static_cast<T *>(out.rawDataMut()),
                    outC.doubleDataMut(),
                    typeMatch);
    return std::make_tuple(std::move(out), std::move(outC));
}

template <typename T>
std::tuple<Value, Value>
modeAlongDimT(const Value &x, int dim, ValueType outType, Allocator *alloc)
{
    const bool typeMatch = (x.type() == outType);
    if (x.isEmpty() && x.dims().ndim() < 4) {
        return std::make_tuple(Value::matrix(0, 0, outType, alloc),
                               Value::matrix(0, 0, ValueType::DOUBLE, alloc));
    }
    if (x.isScalar() || x.dims().isVector()) {
        if (dim != detail::firstNonSingletonDim(x)) {
            // Identity reduction: copy x as outType, frequencies = ones.
            const size_t n = x.numel();
            Value out, outC;
            if (x.dims().isVector()) {
                out  = createMatrix({x.dims().rows(), x.dims().cols(), 0}, outType, alloc);
                outC = createMatrix({x.dims().rows(), x.dims().cols(), 0}, ValueType::DOUBLE, alloc);
            } else {
                out  = makeScalarT<T>(readSrcAsT<T>(x, 0, typeMatch), outType, alloc);
                outC = Value::scalar(1.0, alloc);
                return std::make_tuple(std::move(out), std::move(outC));
            }
            T *dst = static_cast<T *>(out.rawDataMut());
            double *dstC = outC.doubleDataMut();
            for (size_t i = 0; i < n; ++i) {
                dst[i]  = readSrcAsT<T>(x, i, typeMatch);
                dstC[i] = 1.0;
            }
            return std::make_tuple(std::move(out), std::move(outC));
        }
        return modeAllT<T>(x, outType, alloc);
    }
    auto [out, outC] = allocModeOutputs(x, dim, outType, alloc);
    modeAlongDim<T>(x, dim,
                    static_cast<T *>(out.rawDataMut()),
                    outC.doubleDataMut(),
                    typeMatch);
    return std::make_tuple(std::move(out), std::move(outC));
}

// Dispatch on x.type(). LOGICAL maps to T=uint8_t (storage type) and
// outType=LOGICAL so the result preserves logical class. CHAR uses
// T=char. COMPLEX has no defined order → throw.
std::tuple<Value, Value>
dispatchMode(const Value &x, int dim, Allocator *alloc, const char *fn)
{
    const bool useDimReducer = (dim > 0);
    auto run = [&](auto tag, ValueType outT) {
        using T = decltype(tag);
        return useDimReducer
            ? modeAlongDimT<T>(x, dim, outT, alloc)
            : modeAllT<T>(x, outT, alloc);
    };
    switch (x.type()) {
    case ValueType::DOUBLE:  return run(double  {}, ValueType::DOUBLE);
    case ValueType::SINGLE:  return run(float   {}, ValueType::SINGLE);
    case ValueType::INT8:    return run(int8_t  {}, ValueType::INT8);
    case ValueType::INT16:   return run(int16_t {}, ValueType::INT16);
    case ValueType::INT32:   return run(int32_t {}, ValueType::INT32);
    case ValueType::INT64:   return run(int64_t {}, ValueType::INT64);
    case ValueType::UINT8:   return run(uint8_t {}, ValueType::UINT8);
    case ValueType::UINT16:  return run(uint16_t{}, ValueType::UINT16);
    case ValueType::UINT32:  return run(uint32_t{}, ValueType::UINT32);
    case ValueType::UINT64:  return run(uint64_t{}, ValueType::UINT64);
    case ValueType::LOGICAL: return run(uint8_t {}, ValueType::LOGICAL);
    case ValueType::CHAR:    return run(char    {}, ValueType::CHAR);
    case ValueType::COMPLEX:
        throw Error(std::string(fn) + ": not defined for complex inputs",
                     0, 0, fn, "", std::string("m:") + fn + ":complex");
    default:
        throw Error(std::string(fn) + ": unsupported input type",
                     0, 0, fn, "", std::string("m:") + fn + ":type");
    }
}

} // namespace

std::tuple<Value, Value>
mode(Allocator &alloc, const Value &x, int dim)
{
    const int d = resolveDim(x, dim, "mode");
    return dispatchMode(x, d, &alloc, "mode");
}

// skewness / kurtosis moved to libs/stats/src/moments/moments.cpp.

// ────────────────────────────────────────────────────────────────────
// cov / corrcoef
// ────────────────────────────────────────────────────────────────────
namespace {

void validateCovInputs(const Value &x, const char *fn)
{
    if (x.type() == ValueType::COMPLEX)
        throw Error(std::string(fn) + ": complex inputs are not supported",
                     0, 0, fn, "", std::string("m:") + fn + ":complex");
    if (x.dims().is3D() || x.dims().ndim() > 2)
        throw Error(std::string(fn) + ": only vector and 2D matrix inputs are supported",
                     0, 0, fn, "", std::string("m:") + fn + ":rank");
}

void validateNormFlagCov(int w, const char *fn)
{
    if (w != 0 && w != 1)
        throw Error(std::string(fn) + ": normalization flag must be 0 or 1",
                     0, 0, fn, "", std::string("m:") + fn + ":badFlag");
}

// Build an n×p column-major DOUBLE buffer from x. Vector input is
// treated as a single column. Returns the raw data, n, and p.
void readMatrix(const Value &x, std::vector<double> &out,
                std::size_t &n, std::size_t &p)
{
    if (x.dims().isVector() || x.isScalar()) {
        n = x.numel();
        p = 1;
    } else {
        n = x.dims().rows();
        p = x.dims().cols();
    }
    out.assign(n * p, 0.0);
    if (x.type() == ValueType::DOUBLE && (x.dims().isVector() || x.isScalar()
                                       || (!x.dims().is3D() && x.dims().ndim() == 2))) {
        // Column-major source; for a single-column vector either
        // orientation already lays the elements contiguously.
        const double *src = x.doubleData();
        std::memcpy(out.data(), src, n * p * sizeof(double));
        return;
    }
    for (std::size_t i = 0; i < n * p; ++i)
        out[i] = x.elemAsDouble(i);
}

// In-place: subtract per-column mean from a column-major n×p buffer.
void centerColumns(double *data, std::size_t n, std::size_t p)
{
    for (std::size_t c = 0; c < p; ++c) {
        double s = 0.0;
        for (std::size_t r = 0; r < n; ++r)
            s += data[c * n + r];
        const double m = s / static_cast<double>(n);
        for (std::size_t r = 0; r < n; ++r)
            data[c * n + r] -= m;
    }
}

// covImpl: take a centered n×p buffer, compute X' * X / divisor → p×p.
Value covMatrixFromCentered(Allocator &alloc, const double *X,
                             std::size_t n, std::size_t p, double divisor)
{
    auto out = Value::matrix(p, p, ValueType::DOUBLE, &alloc);
    double *dst = out.doubleDataMut();
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = 0; j < p; ++j) {
            double s = 0.0;
            for (std::size_t r = 0; r < n; ++r)
                s += X[i * n + r] * X[j * n + r];
            dst[j * p + i] = s / divisor;
        }
    return out;
}

} // namespace

Value cov(Allocator &alloc, const Value &x, int normFlag)
{
    validateNormFlagCov(normFlag, "cov");
    validateCovInputs(x, "cov");

    std::vector<double> data;
    std::size_t n, p;
    readMatrix(x, data, n, p);
    if (n == 0) {
        // MATLAB: cov of empty → NaN (or empty p×p depending on shape).
        if (p == 1) return Value::scalar(std::nan(""), &alloc);
        return Value::matrix(p, p, ValueType::DOUBLE, &alloc);
    }
    centerColumns(data.data(), n, p);

    const double divisor = (normFlag == 0)
        ? std::max(1.0, static_cast<double>(n) - 1.0)
        : static_cast<double>(n);

    if (p == 1) {
        // Vector input → return scalar variance.
        double s = 0.0;
        for (std::size_t i = 0; i < n; ++i) s += data[i] * data[i];
        return Value::scalar(s / divisor, &alloc);
    }
    return covMatrixFromCentered(alloc, data.data(), n, p, divisor);
}

Value cov(Allocator &alloc, const Value &x, const Value &y, int normFlag)
{
    validateNormFlagCov(normFlag, "cov");
    validateCovInputs(x, "cov");
    validateCovInputs(y, "cov");
    if (!x.dims().isVector() || !y.dims().isVector())
        throw Error("cov: two-input form requires vector arguments",
                     0, 0, "cov", "", "m:cov:notVector");
    if (x.numel() != y.numel())
        throw Error("cov: x and y must have the same length",
                     0, 0, "cov", "", "m:cov:lengthMismatch");
    const std::size_t n = x.numel();
    if (n == 0)
        return Value::matrix(2, 2, ValueType::DOUBLE, &alloc);
    std::vector<double> data(n * 2);
    for (std::size_t i = 0; i < n; ++i) {
        data[i] = x.elemAsDouble(i);          // column 0 (= x)
        data[n + i] = y.elemAsDouble(i);      // column 1 (= y)
    }
    centerColumns(data.data(), n, 2);
    const double divisor = (normFlag == 0)
        ? std::max(1.0, static_cast<double>(n) - 1.0)
        : static_cast<double>(n);
    return covMatrixFromCentered(alloc, data.data(), n, 2, divisor);
}

namespace {

Value corrcoefFromCov(Allocator &alloc, const Value &C)
{
    if (C.dims().rows() != C.dims().cols())
        throw Error("corrcoef: covariance matrix must be square",
                     0, 0, "corrcoef", "", "m:corrcoef:internal");
    const std::size_t p = C.dims().rows();
    auto R = Value::matrix(p, p, ValueType::DOUBLE, &alloc);
    if (p == 0) return R;
    const double *cd = C.doubleData();
    double *rd = R.doubleDataMut();
    std::vector<double> diag(p);
    for (std::size_t i = 0; i < p; ++i)
        diag[i] = std::sqrt(cd[i * p + i]);
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = 0; j < p; ++j) {
            const double denom = diag[i] * diag[j];
            rd[j * p + i] = (denom == 0.0) ? std::nan("") : cd[j * p + i] / denom;
        }
    return R;
}

} // namespace

Value corrcoef(Allocator &alloc, const Value &x)
{
    // Special case: vector input → 1×1 matrix [1] (variable correlated
    // with itself). Matches MATLAB's `corrcoef(rand(5,1))` behaviour.
    if (x.dims().isVector() || x.isScalar()) {
        auto R = Value::matrix(1, 1, ValueType::DOUBLE, &alloc);
        R.doubleDataMut()[0] = 1.0;
        return R;
    }
    auto C = cov(alloc, x);
    return corrcoefFromCov(alloc, C);
}

Value corrcoef(Allocator &alloc, const Value &x, const Value &y)
{
    auto C = cov(alloc, x, y);
    return corrcoefFromCov(alloc, C);
}

// ────────────────────────────────────────────────────────────────────
// NaN-aware reductions (Phase 2)
// ────────────────────────────────────────────────────────────────────
//
// All seven functions share the same skeleton:
//   1) compactNonNan(slice) — moves non-NaN to the front, returns count k
//   2) if k == 0, return the function-specific empty-slice sentinel
//      (0 for nansum, NaN for the rest — matches MATLAB semantics)
//   3) otherwise call the underlying kernel on (slice, k)
// Routing is via the same applyAlongDim path used by var/std/median etc.

using detail::compactNonNan;

// Phase P2: route nansum / nanmean through the single-pass scan kernels
// in backends/MStdNanReductions_{simd,portable}.cpp. These read the input
// once with an IsNaN mask and skip the compactNonNan scratch copy that
// the older scalar lambda needed. Vector input bypasses applyAlongDim
// entirely (no per-call scratch alloc); matrix dim slices still go
// through applyAlongDim but the lambda no longer mutates the slice.

// nansum / nanmean / nanmax / nanmin / nanvar / nanstdev / nanmedian
// all moved to libs/stats/src/nan_aware/nan_aware.cpp.

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════
namespace {

// If the last positional arg is a 'omitnan'/'includenan' string,
// strip it from the count and return the omit flag. Throws on unknown
// trailing strings so user errors don't get silently ignored.
size_t stripNanFlag(Span<const Value> args, bool &omitNan, const char *fn)
{
    omitNan = false;
    if (args.empty()) return 0;
    const Value &last = args[args.size() - 1];
    if (last.type() != ValueType::CHAR) return args.size();
    std::string s = last.toString();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (s == "omitnan") { omitNan = true; return args.size() - 1; }
    if (s == "includenan")                 return args.size() - 1;
    // Not a nan flag — leave it to the caller to handle (e.g. 'all').
    return args.size();
}

inline void rejectComplexOmitNan(const Value &x, const char *fn)
{
    if (x.type() == ValueType::COMPLEX)
        throw Error(std::string(fn) + ": 'omitnan' for complex input is not supported",
                     0, 0, fn, "", std::string("m:") + fn + ":complexOmitNan");
}

} // namespace

namespace detail {

void var_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw Error("var: requires at least 1 argument",
                     0, 0, "var", "", "m:var:nargin");
    bool omitNan = false;
    const size_t n = stripNanFlag(args, omitNan, "var");
    int w = 0, dim = 0;
    if (n >= 2 && !args[1].isEmpty()) w = static_cast<int>(args[1].toScalar());
    if (n >= 3 && !args[2].isEmpty()) dim = static_cast<int>(args[2].toScalar());
    if (omitNan) {
        if (args[0].type() == ValueType::COMPLEX) {
            outs[0] = varianceComplex(args[0], w, dim,
                                      &ctx.engine->allocator(),
                                      /*sqrtIt=*/false, /*omitNan=*/true);
            return;
        }
        Value r = stats::nanvar(ctx.engine->allocator(), args[0], w, dim);
        if (args[0].type() == ValueType::SINGLE)
            r = narrowToSingle(std::move(r), &ctx.engine->allocator());
        outs[0] = std::move(r);
        return;
    }
    outs[0] = var(ctx.engine->allocator(), args[0], w, dim);
}

void std_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw Error("std: requires at least 1 argument",
                     0, 0, "std", "", "m:std:nargin");
    bool omitNan = false;
    const size_t n = stripNanFlag(args, omitNan, "std");
    int w = 0, dim = 0;
    if (n >= 2 && !args[1].isEmpty()) w = static_cast<int>(args[1].toScalar());
    if (n >= 3 && !args[2].isEmpty()) dim = static_cast<int>(args[2].toScalar());
    if (omitNan) {
        if (args[0].type() == ValueType::COMPLEX) {
            outs[0] = varianceComplex(args[0], w, dim,
                                      &ctx.engine->allocator(),
                                      /*sqrtIt=*/true, /*omitNan=*/true);
            return;
        }
        Value r = stats::nanstdev(ctx.engine->allocator(), args[0], w, dim);
        if (args[0].type() == ValueType::SINGLE)
            r = narrowToSingle(std::move(r), &ctx.engine->allocator());
        outs[0] = std::move(r);
        return;
    }
    outs[0] = stdev(ctx.engine->allocator(), args[0], w, dim);
}

void median_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("median: requires at least 1 argument",
                     0, 0, "median", "", "m:median:nargin");
    bool omitNan = false;
    size_t n = stripNanFlag(args, omitNan, "median");
    int dim = 0;
    bool isAll = false;
    if (n >= 2 && !args[1].isEmpty()) {
        const Value &a = args[1];
        if (a.type() == ValueType::CHAR) {
            std::string s = a.toString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (s == "all") isAll = true;
            else throw Error("median: unknown flag '" + s + "'",
                              0, 0, "median", "", "m:median:badFlag");
        } else {
            dim = static_cast<int>(a.toScalar());
        }
    }
    if (isAll) {
        // 'all' → flatten + median over all elements (skipping NaN if omitnan).
        if (args[0].type() == ValueType::COMPLEX)
            throw Error("median: complex inputs are not supported",
                         0, 0, "median", "", "m:median:complex");
        const size_t total = args[0].numel();
        std::vector<double> scratch(total);
        const bool fastDouble = (args[0].type() == ValueType::DOUBLE);
        if (fastDouble)
            std::copy(args[0].doubleData(), args[0].doubleData() + total, scratch.data());
        else
            for (size_t i = 0; i < total; ++i) scratch[i] = args[0].elemAsDouble(i);
        size_t k = total;
        if (omitNan) k = compactNonNan(scratch.data(), total);
        double v = medianFromSlice(scratch.data(), k);
        Value r = Value::scalar(v, &ctx.engine->allocator());
        if (args[0].type() == ValueType::SINGLE)
            r = narrowToSingle(std::move(r), &ctx.engine->allocator());
        outs[0] = std::move(r);
        return;
    }
    if (omitNan) {
        rejectComplexOmitNan(args[0], "median");
        Value r = stats::nanmedian(ctx.engine->allocator(), args[0], dim);
        if (args[0].type() == ValueType::SINGLE)
            r = narrowToSingle(std::move(r), &ctx.engine->allocator());
        outs[0] = std::move(r);
        return;
    }
    outs[0] = median(ctx.engine->allocator(), args[0], dim);
}

void quantile_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("quantile: requires (X, p[, dim])",
                     0, 0, "quantile", "", "m:quantile:nargin");
    int dim = 0;
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = quantile(ctx.engine->allocator(), args[0], args[1], dim);
}

void prctile_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("prctile: requires (X, p[, dim])",
                     0, 0, "prctile", "", "m:prctile:nargin");
    int dim = 0;
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = prctile(ctx.engine->allocator(), args[0], args[1], dim);
}

void mode_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("mode: requires at least 1 argument",
                     0, 0, "mode", "", "m:mode:nargin");
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        dim = static_cast<int>(args[1].toScalar());
    auto [v, c] = mode(ctx.engine->allocator(), args[0], dim);
    outs[0] = std::move(v);
    if (nargout > 1)
        outs[1] = std::move(c);
}

// skewness_reg / kurtosis_reg moved to libs/stats/src/moments/moments.cpp

void cov_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw Error("cov: requires at least 1 argument",
                     0, 0, "cov", "", "m:cov:nargin");
    Allocator &alloc = ctx.engine->allocator();
    if (args.size() == 1) {
        outs[0] = cov(alloc, args[0]);
        return;
    }
    // 2-arg form is ambiguous: cov(x, normFlag) vs cov(x, y).
    // Disambiguate exactly the way MATLAB does: if the second arg is a
    // scalar (0 or 1), it's normFlag; otherwise it's y.
    if (args.size() == 2) {
        if (args[1].isScalar()) {
            const double v = args[1].toScalar();
            if (v == 0.0 || v == 1.0) {
                outs[0] = cov(alloc, args[0], static_cast<int>(v));
                return;
            }
        }
        outs[0] = cov(alloc, args[0], args[1]);
        return;
    }
    // 3-arg form: cov(x, y, normFlag).
    const int w = static_cast<int>(args[2].toScalar());
    outs[0] = cov(alloc, args[0], args[1], w);
}

void corrcoef_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("corrcoef: requires at least 1 argument",
                     0, 0, "corrcoef", "", "m:corrcoef:nargin");
    Allocator &alloc = ctx.engine->allocator();
    if (args.size() == 1) {
        outs[0] = corrcoef(alloc, args[0]);
        return;
    }
    outs[0] = corrcoef(alloc, args[0], args[1]);
}

// nan*_reg adapters all moved to libs/stats/src/nan_aware/nan_aware.cpp.

} // namespace detail

} // namespace numkit::builtin
