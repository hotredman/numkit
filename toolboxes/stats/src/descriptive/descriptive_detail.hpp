// toolboxes/.../descriptive_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by descriptive.cpp + descriptive_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include "helpers.hpp"            // createForDims/createMatrix/DimsArg (engine-free)
#include "reduction_helpers.hpp"  // numkit::builtin::detail dim-infra (engine-free, ops re-export)
#include "math/arithmetic/var_reduction.hpp" // varianceTwoPass (engine-free)

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

namespace numkit::stats {

using namespace ::numkit::builtin::detail;
using ::numkit::math::varianceTwoPass;

// Named (external-linkage) so the file-scope worker quantileWithOpts can
// take it across the compute/register TU boundary.
enum class QMethod { Midpoint, Inclusive, Exclusive, Approximate };

namespace {

void validateNormFlag(int w, const char *fn)
{
    if (w != 0 && w != 1)
        throw Error(std::string(fn) + ": normalization flag must be 0 or 1",
                     0, 0, fn, "", std::string("numkit:") + fn + ":badFlag");
}

// Cast a DOUBLE result to SINGLE in place. Used to preserve SINGLE
// input type without writing parallel single-precision kernels —
// arithmetic happens at double precision (more precise than MATLAB)
// then narrows at the end.
Value narrowToSingle(Value d, std::pmr::memory_resource *mr)
{
    if (d.type() != ValueType::DOUBLE) return d;
    Value r = createForDims(d.dims(), ValueType::SINGLE, mr);
    const double *src = d.doubleData();
    float *dst = r.singleDataMut();
    for (size_t i = 0; i < d.numel(); ++i)
        dst[i] = static_cast<float>(src[i]);
    return r;
}

// Cast a DOUBLE result back to an integer class: round half away from zero
// (std::round) and saturate to the type's range. Matches MATLAB R2025b, which
// preserves the integer class for median (median(int32([1 2 3 4]))=3 int32,
// median(int8([-1 -2]))=-2 int8) — the two-middle-element average is rounded
// half-away-from-zero and the class is retained.
Value narrowToInteger(const Value &d, ValueType vt, std::pmr::memory_resource *mr)
{
    if (d.type() != ValueType::DOUBLE) return d;
    Value r = createForDims(d.dims(), vt, mr);
    const double *src = d.doubleData();
    const size_t n = d.numel();
    auto fill = [&](auto *dst) {
        using T = std::remove_pointer_t<std::decay_t<decltype(dst)>>;
        const double lo = static_cast<double>(std::numeric_limits<T>::min());
        const double hi = static_cast<double>(std::numeric_limits<T>::max());
        for (size_t i = 0; i < n; ++i) {
            double v = std::round(src[i]);   // round half away from zero
            if (v < lo) v = lo;
            else if (v > hi) v = hi;          // saturate to class range
            dst[i] = static_cast<T>(v);
        }
    };
    switch (vt) {
    case ValueType::INT8:   fill(r.int8DataMut());   break;
    case ValueType::INT16:  fill(r.int16DataMut());  break;
    case ValueType::INT32:  fill(r.int32DataMut());  break;
    case ValueType::INT64:  fill(r.int64DataMut());  break;
    case ValueType::UINT8:  fill(r.uint8DataMut());  break;
    case ValueType::UINT16: fill(r.uint16DataMut()); break;
    case ValueType::UINT32: fill(r.uint32DataMut()); break;
    case ValueType::UINT64: fill(r.uint64DataMut()); break;
    default: return d;
    }
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

inline Value allocVarianceOutput(const Value &x, int redDim, std::pmr::memory_resource *mr)
{
    if (x.dims().ndim() >= 4 && redDim >= 1 && redDim <= x.dims().ndim()) {
        ScratchArena scratch(mr);
        auto shape = outShapeForDimND(&scratch, x, redDim);
        return Value::matrixND(shape.data(), (int) shape.size(), ValueType::DOUBLE, mr);
    }
    auto outShape = outShapeForDim(x, redDim);
    return createMatrix(outShape, ValueType::DOUBLE, mr);
}

// MATLAB empty-input result for the NaN-returning descriptive reductions
// (var / std / median / mode), mirroring mean's empty rule:
//   * default (no explicit dim) of the 0x0 [] -> scalar NaN
//   * otherwise the operating dim (explicit dim when given, else the first
//     dim whose size != 1 — MATLAB treats a size-0 dim as non-singleton)
//     collapses to 1 and the result is NaN-filled.
// A size-0 sibling dim keeps the result empty, e.g. var(zeros(3,0))=1x0,
// median([],2)=0x1. numkit previously returned a 0x0 empty for all of these.
inline Value emptyStatReductionFill(const Value &x, int dim, double fill,
                                    std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    if (dim < 1 && dd.ndim() == 2 && dd.rows() == 0 && dd.cols() == 0)
        return Value::scalar(fill, mr);
    int opDim = dim;
    if (opDim < 1) {
        opDim = 1;
        const int nd = dd.ndim();
        for (int i = 0; i < nd; ++i)
            if (dd.dim(i) != 1) { opDim = i + 1; break; }
    }
    DimsArg o{dd.rows(), dd.cols(), dd.is3D() ? dd.pages() : 0};
    switch (opDim) {
        case 1: o.rows  = 1; break;
        case 2: o.cols  = 1; break;
        case 3: o.pages = (o.pages == 0) ? 0 : 1; break;
        default: break;
    }
    Value out = createMatrix(o, ValueType::DOUBLE, mr);
    double *p = out.doubleDataMut();
    const size_t n = out.numel();
    for (size_t i = 0; i < n; ++i) p[i] = fill;
    return out;
}

inline Value emptyStatReductionNaN(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    return emptyStatReductionFill(x, dim, std::nan(""), mr);
}

// Complex variance along dim: walks slices via stride math, gathers
// each slice into a Complex scratch buffer, computes per-slice complex
// variance, writes DOUBLE output.
void complexVarianceAlongDim(const Value &x, int redDim, double *dst, int normFlag, bool omitNan, std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    const int redAxis = redDim - 1;
    const size_t sliceLen = d.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < d.ndim(); ++i) O *= d.dim(i);

    const Complex *src = x.complexData();
    ScratchVec<Complex> scratch(sliceLen, mr);
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
                       std::pmr::memory_resource *mr, bool sqrtIt, bool omitNan = false)
{
    if (x.isEmpty())
        return emptyStatReductionNaN(x, dim, mr);
    const Complex *src = x.complexData();
    if (x.isScalar() || x.dims().isVector()) {
        double v = complexVarianceFromSlice(src, x.numel(), normFlag, omitNan);
        if (sqrtIt && !std::isnan(v)) v = std::sqrt(v);
        return Value::scalar(v, mr);
    }
    const int d = (dim > 0) ? dim : firstNonSingletonDim(x);
    Value out = allocVarianceOutput(x, d, mr);
    ScratchArena scratch(mr);
    complexVarianceAlongDim(x, d, out.doubleDataMut(), normFlag, omitNan, &scratch);
    if (sqrtIt) {
        double *p = out.doubleDataMut();
        const size_t n = out.numel();
        for (size_t i = 0; i < n; ++i)
            if (!std::isnan(p[i])) p[i] = std::sqrt(p[i]);
    }
    return out;
}

} // namespace
namespace {

double medianFromSlice(double *data, size_t n)
{
    if (n == 0) return std::nan("");
    if (n == 1) return data[0];
    // MATLAB R2025b default: NaN poisons (median(v) where v has NaN
    // returns NaN; 'omitnan' is opt-in via the explicit nanflag, which
    // routes to nanmedian instead of this kernel).
    for (size_t i = 0; i < n; ++i)
        if (std::isnan(data[i])) return std::nan("");
    const size_t mid = n / 2;
    std::nth_element(data, data + mid, data + n);
    if (n % 2 == 1)
        return data[mid];
    const double upper = data[mid];
    const double lower = *std::max_element(data, data + mid);
    return 0.5 * (lower + upper);
}

} // namespace
namespace {

// Complex ordering: by magnitude, ties broken by phase angle (matches numkit
// sort/max and MATLAB's complex median ordering).
inline bool complexAbsAngleLess(const Complex &a, const Complex &b)
{
    const double aa = std::abs(a), ab = std::abs(b);
    if (aa != ab) return aa < ab;
    return std::arg(a) < std::arg(b);
}

// Median of a complex slice (mutates s by sorting). Propagates NaN (MATLAB
// 'includenan' default). Even n → mean of the two middle values.
Complex complexMedianFromSlice(Complex *s, size_t n)
{
    if (n == 0) return Complex(std::nan(""), std::nan(""));
    for (size_t i = 0; i < n; ++i)
        if (isComplexNaNStats(s[i])) return Complex(std::nan(""), std::nan(""));
    std::sort(s, s + n, complexAbsAngleLess);
    if (n & 1u) return s[n / 2];
    return 0.5 * (s[n / 2 - 1] + s[n / 2]);
}

// Per-dim complex median (mirrors complexVarianceAlongDim; COMPLEX output).
void complexMedianAlongDim(const Value &x, int redDim, Complex *dst,
                           std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    const int redAxis = redDim - 1;
    const size_t sliceLen = d.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < d.ndim(); ++i) O *= d.dim(i);
    const Complex *src = x.complexData();
    ScratchVec<Complex> scratch(sliceLen, mr);
    auto reduceSlice = [&](size_t outOff, size_t baseOff, size_t stride) {
        for (size_t k = 0; k < sliceLen; ++k) scratch[k] = src[baseOff + k * stride];
        dst[outOff] = complexMedianFromSlice(scratch.data(), sliceLen);
    };
    if (B == 1) {
        for (size_t o = 0; o < O; ++o) reduceSlice(o, o * sliceLen, 1);
        return;
    }
    for (size_t o = 0; o < O; ++o)
        for (size_t b = 0; b < B; ++b)
            reduceSlice(o * B + b, o * sliceLen * B + b, B);
}

Value medianComplex(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (x.isEmpty()) return emptyStatReductionNaN(x, dim, mr);
    if (x.isScalar()) return Value::complexScalar(x.complexData()[0], mr);
    const int d = resolveDim(x, dim, "median");
    Value out;
    if (x.dims().ndim() >= 4 && d >= 1 && d <= x.dims().ndim()) {
        ScratchArena sc(mr);
        auto shape = outShapeForDimND(&sc, x, d);
        out = Value::matrixND(shape.data(), static_cast<int>(shape.size()),
                              ValueType::COMPLEX, mr);
    } else {
        auto outShape = outShapeForDim(x, d);
        out = createMatrix(outShape, ValueType::COMPLEX, mr);
    }
    ScratchArena scratch(mr);
    complexMedianAlongDim(x, d, out.complexDataMut(), &scratch);
    return out;
}

} // namespace
namespace {


double quantileFromSortedSlice(const double *sorted, size_t n, double p,
                               QMethod method)
{
    if (n == 0) return std::nan("");
    if (n == 1) return sorted[0];
    if (!std::isfinite(p)) return std::nan("");

    // Compute 1-based real-valued position q according to the method.
    double q;
    switch (method) {
        case QMethod::Midpoint:
        case QMethod::Approximate:                     // fallback
            q = p * static_cast<double>(n) + 0.5;
            break;
        case QMethod::Inclusive:
            q = p * static_cast<double>(n - 1) + 1.0;
            break;
        case QMethod::Exclusive:
            q = p * static_cast<double>(n + 1);
            break;
    }
    if (q <= 1.0) return sorted[0];
    if (q >= static_cast<double>(n)) return sorted[n - 1];
    const size_t lo = static_cast<size_t>(std::floor(q)) - 1; // → 0-based
    const size_t hi = lo + 1;
    const double frac = q - std::floor(q);
    return sorted[lo] + frac * (sorted[hi] - sorted[lo]);
}

// quantile/prctile share the bulk of the logic. `pScale` converts the
// raw user input to a [0,1] probability (1.0 for quantile, 1/100 for
// prctile). `method` selects the interpolation rule (default Midpoint
// = MATLAB R2025b default).
Value quantileImpl(const Value &x, const Value &p, int dim, double pScale, QMethod method, const char *fn, std::pmr::memory_resource *mr)
{
    if (p.numel() == 0)
        throw Error(std::string(fn) + ": p must be non-empty",
                     0, 0, fn, "", std::string("numkit:") + fn + ":emptyP");

    ScratchArena scratch(mr);

    // Normalize probabilities into a flat vector (so prctile sees /100)
    // then validate the post-scaling value lies in [0,1].
    auto probs = ScratchVec<double>(p.numel(), &scratch);
    for (size_t i = 0; i < p.numel(); ++i) {
        probs[i] = p.doubleData()[i] * pScale;
        if (!(probs[i] >= 0.0 && probs[i] <= 1.0))
            throw Error(std::string(fn) + ": probabilities out of range",
                         0, 0, fn, "", std::string("numkit:") + fn + ":badProb");
    }

    const size_t k = probs.size();
    const int d = resolveDim(x, dim, fn);

    // Scalar p → standard one-output-per-slice path.
    if (k == 1) {
        const double pp = probs[0];
        return applyAlongDim(x, d,
            [pp, method](size_t, double *slice, size_t n) {
                std::sort(slice, slice + n);
                return quantileFromSortedSlice(slice, n, pp, method);
            }, mr);
    }

    // Vector p: each slice produces k outputs along the reduced dim.
    // We need to allocate the output explicitly because applyAlongDim
    // assumes 1 output per slice.
    if (x.isEmpty()) {
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    }
    if (x.dims().isVector() || x.isScalar()) {
        // Output is 1×k row vector.
        auto sorted = ScratchVec<double>(x.numel(), &scratch);
        std::copy(x.doubleData(), x.doubleData() + x.numel(), sorted.data());
        std::sort(sorted.begin(), sorted.end());
        auto out = Value::matrix(1, k, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < k; ++i)
            out.doubleDataMut()[i] = quantileFromSortedSlice(sorted.data(),
                                                              sorted.size(),
                                                              probs[i],
                                                              method);
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
    auto out = createMatrix(outShape, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    const size_t outR = outShape.rows, outC = outShape.cols;
    const size_t outP = outShape.pages == 0 ? 1 : outShape.pages;
    const size_t N = sliceLenForDim(x, d);
    auto sorted = ScratchVec<double>(N, &scratch);
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
                             quantileFromSortedSlice(sorted.data(), N, probs[i], method));
            }
    } else if (d == 2) {
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t r = 0; r < R; ++r) {
                for (size_t c = 0; c < C; ++c)
                    sorted[c] = src[pp * R * C + c * R + r];
                std::sort(sorted.begin(), sorted.end());
                for (size_t i = 0; i < k; ++i)
                    writeOut(r, i, pp,
                             quantileFromSortedSlice(sorted.data(), N, probs[i], method));
            }
    } else if (d == 3) {
        for (size_t c = 0; c < C; ++c)
            for (size_t r = 0; r < R; ++r) {
                for (size_t pp = 0; pp < P; ++pp)
                    sorted[pp] = src[pp * R * C + c * R + r];
                std::sort(sorted.begin(), sorted.end());
                for (size_t i = 0; i < k; ++i)
                    writeOut(r, c, i,
                             quantileFromSortedSlice(sorted.data(), N, probs[i], method));
            }
    }
    return out;
}

} // namespace
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
inline Value makeScalarT(T v, ValueType outType, std::pmr::memory_resource *mr)
{
    if (outType == ValueType::DOUBLE)
        return Value::scalar(static_cast<double>(v), mr);
    if (outType == ValueType::LOGICAL)
        return Value::logicalScalar(v != 0, mr);
    auto r = Value::matrix(1, 1, outType, mr);
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
void modeAlongDim(const Value &x, int redDim, T *dst, double *dstC, bool typeMatch, std::pmr::memory_resource *mr)
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

    ScratchVec<T> scratch(sliceLen, mr);
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
allocModeOutputs(const Value &x, int redDim, ValueType outType, std::pmr::memory_resource *mr)
{
    if (x.dims().ndim() >= 4 && redDim >= 1 && redDim <= x.dims().ndim()) {
        ScratchArena scratch(mr);
        auto shape = outShapeForDimND(&scratch, x, redDim);
        return {Value::matrixND(shape.data(), (int) shape.size(), outType, mr),
                Value::matrixND(shape.data(), (int) shape.size(), ValueType::DOUBLE, mr)};
    }
    auto outShape = outShapeForDim(x, redDim);
    return {createMatrix(outShape, outType, mr),
            createMatrix(outShape, ValueType::DOUBLE, mr)};
}

template <typename T>
std::tuple<Value, Value>
modeAllT(const Value &x, ValueType outType, std::pmr::memory_resource *mr)
{
    const bool typeMatch = (x.type() == outType);
    if (x.isEmpty() && x.dims().ndim() < 4) {
        return std::make_tuple(Value::matrix(0, 0, outType, mr),
                               Value::matrix(0, 0, ValueType::DOUBLE, mr));
    }
    ScratchArena scratch(mr);
    if (x.isScalar() || x.dims().isVector()) {
        ScratchVec<T> buf(x.numel(), &scratch);
        for (size_t i = 0; i < x.numel(); ++i)
            buf[i] = readSrcAsT<T>(x, i, typeMatch);
        T v; double c;
        modeFromSliceT<T>(buf.data(), x.numel(), v, c);
        return std::make_tuple(makeScalarT<T>(v, outType, mr),
                               Value::scalar(c, mr));
    }
    const int redDim = firstNonSingletonDim(x);
    auto [out, outC] = allocModeOutputs(x, redDim, outType, mr);
    modeAlongDim<T>(x, redDim, static_cast<T *>(out.rawDataMut()), outC.doubleDataMut(), typeMatch, &scratch);
    return std::make_tuple(std::move(out), std::move(outC));
}

template <typename T>
std::tuple<Value, Value>
modeAlongDimT(const Value &x, int dim, ValueType outType, std::pmr::memory_resource *mr)
{
    const bool typeMatch = (x.type() == outType);
    if (x.isEmpty() && x.dims().ndim() < 4) {
        return std::make_tuple(Value::matrix(0, 0, outType, mr),
                               Value::matrix(0, 0, ValueType::DOUBLE, mr));
    }
    if (x.isScalar() || x.dims().isVector()) {
        if (dim != firstNonSingletonDim(x)) {
            // Identity reduction: copy x as outType, frequencies = ones.
            const size_t n = x.numel();
            Value out, outC;
            if (x.dims().isVector()) {
                out  = createMatrix({x.dims().rows(), x.dims().cols(), 0}, outType, mr);
                outC = createMatrix({x.dims().rows(), x.dims().cols(), 0}, ValueType::DOUBLE, mr);
            } else {
                out  = makeScalarT<T>(readSrcAsT<T>(x, 0, typeMatch), outType, mr);
                outC = Value::scalar(1.0, mr);
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
        return modeAllT<T>(x, outType, mr);
    }
    auto [out, outC] = allocModeOutputs(x, dim, outType, mr);
    ScratchArena scratch(mr);
    modeAlongDim<T>(x, dim, static_cast<T *>(out.rawDataMut()), outC.doubleDataMut(), typeMatch, &scratch);
    return std::make_tuple(std::move(out), std::move(outC));
}

// Dispatch on x.type(). LOGICAL maps to T=uint8_t (storage type) and
// outType=LOGICAL so the result preserves logical class. CHAR uses
// T=char. COMPLEX has no defined order → throw.
std::tuple<Value, Value>
dispatchMode(const Value &x, int dim, std::pmr::memory_resource *mr, const char *fn)
{
    const bool useDimReducer = (dim > 0);
    auto run = [&](auto tag, ValueType outT) {
        using T = decltype(tag);
        return useDimReducer
            ? modeAlongDimT<T>(x, dim, outT, mr)
            : modeAllT<T>(x, outT, mr);
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
                     0, 0, fn, "", std::string("numkit:") + fn + ":complex");
    default:
        throw Error(std::string(fn) + ": unsupported input type",
                     0, 0, fn, "", std::string("numkit:") + fn + ":type");
    }
}

} // namespace
namespace {

void validateCovInputs(const Value &x, const char *fn)
{
    if (x.type() == ValueType::COMPLEX)
        throw Error(std::string(fn) + ": complex inputs are not supported",
                     0, 0, fn, "", std::string("numkit:") + fn + ":complex");
    if (x.dims().is3D() || x.dims().ndim() > 2)
        throw Error(std::string(fn) + ": only vector and 2D matrix inputs are supported",
                     0, 0, fn, "", std::string("numkit:") + fn + ":rank");
}

void validateNormFlagCov(int w, const char *fn)
{
    if (w != 0 && w != 1)
        throw Error(std::string(fn) + ": normalization flag must be 0 or 1",
                     0, 0, fn, "", std::string("numkit:") + fn + ":badFlag");
}

// Build an n×p column-major DOUBLE buffer from x. Vector input is
// treated as a single column. Returns the raw data, n, and p.
void readMatrix(const Value &x, ScratchVec<double> &out,
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
Value covMatrixFromCentered(const double *X, std::size_t n, std::size_t p, double divisor, std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(p, p, ValueType::DOUBLE, mr);
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
namespace {

enum class CovNan { Include, Omitrows, Partialrows };

// Recognize a NaN-policy string ('includenan' | 'omitrows' | 'partialrows').
bool parseCovNanFlag(const Value &v, CovNan &mode)
{
    if (!v.isChar() && !v.isString()) return false;
    std::string s = v.toString();
    for (auto &c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (s == "includenan")  { mode = CovNan::Include;     return true; }
    if (s == "omitrows")    { mode = CovNan::Omitrows;    return true; }
    if (s == "partialrows") { mode = CovNan::Partialrows; return true; }
    return false;
}

// Assemble two equal-length vectors into an n×2 column-major matrix Value
// [x(:) y(:)] for the two-vector NaN-aware path.
Value assembleXY(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    validateCovInputs(x, "cov");
    validateCovInputs(y, "cov");
    if (!x.dims().isVector() || !y.dims().isVector())
        throw Error("cov: two-input form requires vector arguments",
                     0, 0, "cov", "", "numkit:cov:notVector");
    if (x.numel() != y.numel())
        throw Error("cov: x and y must have the same length",
                     0, 0, "cov", "", "numkit:cov:lengthMismatch");
    const std::size_t n = x.numel();
    auto out = Value::matrix(n, 2, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i) {
        o[i] = x.elemAsDouble(i);
        o[n + i] = y.elemAsDouble(i);
    }
    return out;
}

// cov with 'omitrows' (listwise deletion: drop any row containing a NaN) or
// 'partialrows' (pairwise deletion: each cov(i,j) uses rows where both
// columns are non-NaN, with the means taken over exactly those rows). The
// default 'includenan' is the NaN-poisoning behaviour of plain cov() and is
// routed there by the dispatcher.
Value covNanAware(const Value &x, int normFlag, CovNan mode,
                  std::pmr::memory_resource *mr)
{
    validateNormFlagCov(normFlag, "cov");
    validateCovInputs(x, "cov");
    ScratchArena scratch(mr);
    ScratchVec<double> data(&scratch);
    std::size_t n, p;
    readMatrix(x, data, n, p);

    auto divisorOf = [normFlag](std::size_t m) {
        return (normFlag == 0) ? std::max(1.0, static_cast<double>(m) - 1.0)
                               : static_cast<double>(m);
    };

    // Vector input → scalar variance over the non-NaN elements.
    if (p == 1) {
        double s = 0.0;
        std::size_t m = 0;
        for (std::size_t i = 0; i < n; ++i)
            if (!std::isnan(data[i])) { s += data[i]; ++m; }
        if (m == 0) return Value::scalar(std::nan(""), mr);
        const double mean = s / static_cast<double>(m);
        double sq = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            if (!std::isnan(data[i])) { const double d = data[i] - mean; sq += d * d; }
        return Value::scalar(sq / divisorOf(m), mr);
    }

    if (mode == CovNan::Omitrows) {
        auto rowOk = [&](std::size_t r) {
            for (std::size_t c = 0; c < p; ++c)
                if (std::isnan(data[c * n + r])) return false;
            return true;
        };
        std::size_t m = 0;
        for (std::size_t r = 0; r < n; ++r) if (rowOk(r)) ++m;
        if (m == 0) {
            auto out = Value::matrix(p, p, ValueType::DOUBLE, mr);
            double *o = out.doubleDataMut();
            for (std::size_t i = 0; i < p * p; ++i) o[i] = std::nan("");
            return out;
        }
        ScratchVec<double> kept(&scratch);
        kept.assign(m * p, 0.0);
        std::size_t rr = 0;
        for (std::size_t r = 0; r < n; ++r) {
            if (!rowOk(r)) continue;
            for (std::size_t c = 0; c < p; ++c) kept[c * m + rr] = data[c * n + r];
            ++rr;
        }
        centerColumns(kept.data(), m, p);
        return covMatrixFromCentered(kept.data(), m, p, divisorOf(m), mr);
    }

    // Partialrows.
    auto out = Value::matrix(p, p, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = i; j < p; ++j) {
            double si = 0.0, sj = 0.0;
            std::size_t m = 0;
            for (std::size_t r = 0; r < n; ++r) {
                const double a = data[i * n + r], b = data[j * n + r];
                if (!std::isnan(a) && !std::isnan(b)) { si += a; sj += b; ++m; }
            }
            double v;
            if (m == 0) {
                v = std::nan("");
            } else {
                const double mi = si / static_cast<double>(m);
                const double mj = sj / static_cast<double>(m);
                double s = 0.0;
                for (std::size_t r = 0; r < n; ++r) {
                    const double a = data[i * n + r], b = data[j * n + r];
                    if (!std::isnan(a) && !std::isnan(b))
                        s += (a - mi) * (b - mj);
                }
                v = s / divisorOf(m);
            }
            o[j * p + i] = v;
            o[i * p + j] = v;
        }
    return out;
}

// ── corrcoef 'Rows' NaN policy ────────────────────────────────────────
enum class CorrcoefRows { All, Complete, Pairwise };

CorrcoefRows parseCorrcoefRows(Span<const Value> args, std::size_t start)
{
    for (std::size_t i = start; i + 1 < args.size(); i += 2) {
        if (!(args[i].isChar() || args[i].isString())) continue;
        std::string name = args[i].toString();
        for (char &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name == "rows") {
            std::string v = args[i + 1].toString();
            for (char &c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (v == "all")      return CorrcoefRows::All;
            if (v == "complete") return CorrcoefRows::Complete;
            if (v == "pairwise") return CorrcoefRows::Pairwise;
            throw Error("corrcoef: Rows must be 'all', 'complete', or 'pairwise'",
                        0, 0, "corrcoef", "", "numkit:corrcoef:BadRows");
        }
    }
    return CorrcoefRows::All;
}

// corrcoef(...,'Alpha',a): significance level for the RL/RU confidence
// bounds (default 0.05). Must be in (0, 1).
double parseCorrcoefAlpha(Span<const Value> args, std::size_t start)
{
    for (std::size_t i = start; i + 1 < args.size(); i += 2) {
        if (!(args[i].isChar() || args[i].isString())) continue;
        std::string name = args[i].toString();
        for (char &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name == "alpha") {
            const double a = args[i + 1].toScalar();
            if (!(a > 0.0 && a < 1.0))
                throw Error("corrcoef: Alpha must be in the interval (0,1)",
                            0, 0, "corrcoef", "", "numkit:corrcoef:BadAlpha");
            return a;
        }
    }
    return 0.05;
}

std::size_t countCompleteRows(const Value &X)
{
    std::size_t n, p;
    {
        const bool vec = X.dims().isVector() || X.isScalar();
        n = vec ? X.numel() : X.dims().rows();
        p = vec ? 1 : X.dims().cols();
    }
    std::size_t m = 0;
    for (std::size_t r = 0; r < n; ++r) {
        bool ok = true;
        for (std::size_t c = 0; c < p && ok; ++c)
            if (std::isnan(X.elemAsDouble(r + c * n))) ok = false;
        if (ok) ++m;
    }
    return m;
}

// Pairwise Pearson correlation: each entry (i,j) uses the rows where both
// columns are non-NaN, with the means AND the standard deviations taken
// over exactly those common rows. (corrcov(cov_partialrows) is NOT the
// same — that normalizes each column by its own per-column rows.)
Value corrcoefPairwise(const Value &X, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    ScratchVec<double> data(&scratch);
    std::size_t n, p;
    readMatrix(X, data, n, p);
    auto out = Value::matrix(p, p, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = 0; j < p; ++j) {
            double si = 0.0, sj = 0.0;
            std::size_t m = 0;
            for (std::size_t r = 0; r < n; ++r) {
                const double a = data[i * n + r], b = data[j * n + r];
                if (!std::isnan(a) && !std::isnan(b)) { si += a; sj += b; ++m; }
            }
            double rij;
            if (m < 2) {
                rij = std::numeric_limits<double>::quiet_NaN();
            } else {
                const double mi = si / static_cast<double>(m);
                const double mj = sj / static_cast<double>(m);
                double sxy = 0.0, sxx = 0.0, syy = 0.0;
                for (std::size_t r = 0; r < n; ++r) {
                    const double a = data[i * n + r], b = data[j * n + r];
                    if (std::isnan(a) || std::isnan(b)) continue;
                    const double da = a - mi, db = b - mj;
                    sxy += da * db; sxx += da * da; syy += db * db;
                }
                const double den = std::sqrt(sxx * syy);
                rij = (den > 0.0) ? sxy / den
                                  : std::numeric_limits<double>::quiet_NaN();
            }
            o[i + j * p] = rij;
        }
    return out;
}

Value corrcoefScalarOne(std::pmr::memory_resource *mr)
{
    auto R = Value::matrix(1, 1, ValueType::DOUBLE, mr);
    R.doubleDataMut()[0] = 1.0;
    return R;
}

Value corrcoefFromCov(const Value &C, std::pmr::memory_resource *mr)
{
    if (C.dims().rows() != C.dims().cols())
        throw Error("corrcoef: covariance matrix must be square",
                     0, 0, "corrcoef", "", "numkit:corrcoef:internal");
    const std::size_t p = C.dims().rows();
    auto R = Value::matrix(p, p, ValueType::DOUBLE, mr);
    if (p == 0) return R;
    const double *cd = C.doubleData();
    double *rd = R.doubleDataMut();
    ScratchArena scratch(mr);
    auto diag = ScratchVec<double>(p, &scratch);
    for (std::size_t i = 0; i < p; ++i)
        diag[i] = std::sqrt(cd[i * p + i]);
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = 0; j < p; ++j) {
            const double denom = diag[i] * diag[j];
            rd[j * p + i] = (denom == 0.0) ? std::nan("") : cd[j * p + i] / denom;
        }
    return R;
}

// Two-sided p-values for a correlation matrix R computed from n observations.
// Diagonal is 1; off-diagonal P(i,j) = 2*tcdf(-|t|, n-2) with the test
// statistic t = r*sqrt((n-2)/(1-r^2)). With n <= 2 (df <= 0) the off-diagonal
// p-values are NaN (MATLAB).
Value corrcoefPValues(const Value &R, double n, std::pmr::memory_resource *mr)
{
    const std::size_t p = R.dims().rows();
    auto P = Value::matrix(p, p, ValueType::DOUBLE, mr);
    if (p == 0) return P;
    const double df = n - 2.0;
    const double *rd = R.doubleData();
    double *pd = P.doubleDataMut();

    // Build the matrix of -|t| values, then evaluate the t-CDF vectorized.
    auto T = Value::matrix(p, p, ValueType::DOUBLE, mr);
    double *td = T.doubleDataMut();
    for (std::size_t k = 0; k < p * p; ++k) {
        const double r = rd[k];
        const double oneMinus = 1.0 - r * r;
        double t = std::numeric_limits<double>::infinity(); // |r|>=1 → extreme
        if (df > 0.0 && oneMinus > 0.0)
            t = std::fabs(r) * std::sqrt(df / oneMinus);
        td[k] = -t;
    }
    Value cdf = (df > 0.0) ? tcdf(T, df, mr) : Value{};
    const double *cd = (df > 0.0) ? cdf.doubleData() : nullptr;

    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = 0; j < p; ++j) {
            const std::size_t k = j * p + i;
            if (i == j) { pd[k] = 1.0; continue; }
            if (df <= 0.0) { pd[k] = std::nan(""); continue; }
            double pv = 2.0 * cd[k];
            if (pv > 1.0) pv = 1.0;
            pd[k] = pv;
        }
    return P;
}

// Fisher z-transform confidence bounds for corrcoef (the 3rd/4th outputs
// RL/RU). For each correlation r: z = atanh(r), se = 1/sqrt(n-3),
// zc = norminv(1-alpha/2); RL = tanh(z - zc*se), RU = tanh(z + zc*se).
// |r| == 1 (incl. the diagonal) -> RL = RU = r. n <= 3 -> se infinite, so
// the bounds widen to the full [-1, 1]. Matches MATLAB R2025b.
void corrcoefConfBounds(const Value &R, double n, double alpha,
                        Value &RL, Value &RU, std::pmr::memory_resource *mr)
{
    const std::size_t p = R.dims().rows();
    RL = Value::matrix(p, p, ValueType::DOUBLE, mr);
    RU = Value::matrix(p, p, ValueType::DOUBLE, mr);
    if (p == 0) return;
    const double *rd = R.doubleData();
    double *rl = RL.doubleDataMut();
    double *ru = RU.doubleDataMut();
    const double zc =
        norminv(Value::scalar(1.0 - alpha / 2.0, mr), 0.0, 1.0, mr).toScalar();
    const double se = (n > 3.0) ? 1.0 / std::sqrt(n - 3.0)
                                : std::numeric_limits<double>::infinity();
    for (std::size_t k = 0; k < p * p; ++k) {
        const double r = rd[k];
        if (std::fabs(r) >= 1.0) { rl[k] = r; ru[k] = r; continue; }
        const double z = std::atanh(r);
        rl[k] = std::tanh(z - zc * se);
        ru[k] = std::tanh(z + zc * se);
    }
}

} // namespace
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
                     0, 0, fn, "", std::string("numkit:") + fn + ":complexOmitNan");
}

} // namespace

Value quantileWithOpts(const Value &x, const Value &p, int dim, bool flatten,
                       QMethod method, double pScale, const char *fn,
                       std::pmr::memory_resource *mr);

} // namespace numkit::stats
