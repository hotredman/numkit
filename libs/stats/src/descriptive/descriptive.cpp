// libs/stats/src/descriptive/descriptive.cpp
//
// Descriptive statistics: var, std, median, quantile, prctile, mode,
// cov, corrcoef. All take an explicit `dim` argument (1-based, or 0
// for "first non-singleton"). Implementations route through
// applyAlongDim from libs/builtin/src/reduction_helpers.hpp.
//
// Moved from libs/builtin/src/data_analysis/descriptive_statistics/stats.cpp
// in Phase 7b — these are Statistics Toolbox content per MATLAB
// taxonomy, not core MATLAB. Registration as stats.descriptive.* +
// compat.* lives in libs/stats/src/library.cpp.

#include <numkit/stats/descriptive/descriptive.hpp>

#include <numkit/stats/nan_aware/nan_aware.hpp>  // var_reg / std_reg / median_reg dispatch into stats:: when 'omitnan' is given

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "reduction_helpers.hpp"
#include "math/arithmetic/var_reduction.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <string>
#include <type_traits>
#include <utility>

namespace numkit::stats {

// `using namespace` aliases for helpers physically rooted in libs/builtin
// (Phase 7b moved this file out, but the helpers stay there). Reg/impl
// bodies can call applyAlongDim, resolveDim, compactNonNan,
// firstNonSingletonDim, outShapeForDim, outShapeForDimND, sliceLenForDim,
// varianceTwoPass, … by short name as before the move.
using namespace ::numkit::builtin::detail;
using ::numkit::builtin::varianceTwoPass;

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

// Complex variance along dim: walks slices via stride math, gathers
// each slice into a Complex scratch buffer, computes per-slice complex
// variance, writes DOUBLE output.
void complexVarianceAlongDim(std::pmr::memory_resource *mr,
                             const Value &x, int redDim, double *dst, int normFlag,
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
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const Complex *src = x.complexData();
    if (x.isScalar() || x.dims().isVector()) {
        double v = complexVarianceFromSlice(src, x.numel(), normFlag, omitNan);
        if (sqrtIt && !std::isnan(v)) v = std::sqrt(v);
        return Value::scalar(v, mr);
    }
    const int d = (dim > 0) ? dim : firstNonSingletonDim(x);
    Value out = allocVarianceOutput(x, d, mr);
    ScratchArena scratch(mr);
    complexVarianceAlongDim(&scratch, x, d,
                            out.doubleDataMut(), normFlag, omitNan);
    if (sqrtIt) {
        double *p = out.doubleDataMut();
        const size_t n = out.numel();
        for (size_t i = 0; i < n; ++i)
            if (!std::isnan(p[i])) p[i] = std::sqrt(p[i]);
    }
    return out;
}

} // namespace

Value var(std::pmr::memory_resource *mr, const Value &x, int normFlag, int dim)
{
    validateNormFlag(normFlag, "var");
    if (x.type() == ValueType::COMPLEX)
        return varianceComplex(x, normFlag, dim, mr, /*sqrtIt=*/false);
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(varianceTwoPass(x.doubleData(), x.numel(), normFlag), mr);

    const int d = resolveDim(x, dim, "var");
    Value r = applyAlongDim(x, d,
        [normFlag](size_t, double *slice, size_t n) {
            return varianceTwoPass(slice, n, normFlag);
        }, mr);
    if (x.type() == ValueType::SINGLE)
        r = narrowToSingle(std::move(r), mr);
    return r;
}

Value stdev(std::pmr::memory_resource *mr, const Value &x, int normFlag, int dim)
{
    validateNormFlag(normFlag, "std");
    if (x.type() == ValueType::COMPLEX)
        return varianceComplex(x, normFlag, dim, mr, /*sqrtIt=*/true);
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(std::sqrt(varianceTwoPass(x.doubleData(), x.numel(), normFlag)), mr);

    const int d = resolveDim(x, dim, "std");
    Value r = applyAlongDim(x, d,
        [normFlag](size_t, double *slice, size_t n) {
            return std::sqrt(varianceTwoPass(slice, n, normFlag));
        }, mr);
    if (x.type() == ValueType::SINGLE)
        r = narrowToSingle(std::move(r), mr);
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

Value median(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    if (x.type() == ValueType::COMPLEX)
        throw Error("median: complex inputs are not supported (no defined ordering)",
                     0, 0, "median", "", "m:median:complex");
    const int d = resolveDim(x, dim, "median");
    Value r = applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            return medianFromSlice(slice, n);
        }, mr);
    if (x.type() == ValueType::SINGLE)
        r = narrowToSingle(std::move(r), mr);
    return r;
}

// ────────────────────────────────────────────────────────────────────
// quantile / prctile
// ────────────────────────────────────────────────────────────────────
//
// Three interpolation methods are supported, matching MATLAB R2025b
// (`help quantile` → method ∈ {midpoint, inclusive, exclusive,
// approximate}):
//
//   * Midpoint  (default; R2007a algorithm, Type-5 in Hyndman/Fan)
//                positions: (k-0.5)/N for k = 1..N
//                inverse:   q = p*N + 0.5, clamp [1, N], linear interp
//
//   * Inclusive (Type-7; numkit's old default)
//                positions: (k-1)/(N-1)
//                inverse:   q = p*(N-1) + 1, clamp [1, N], linear interp
//
//   * Exclusive (Type-6; Weibull)
//                positions: k/(N+1)
//                inverse:   q = p*(N+1), clamp [1, N], linear interp
//
//   * Approximate — t-digest, currently falls back to Midpoint
//     (no functional gap, just signature compatibility).
namespace {

enum class QMethod { Midpoint, Inclusive, Exclusive, Approximate };

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
Value quantileImpl(std::pmr::memory_resource *mr, const Value &x, const Value &p,
                    int dim, double pScale, QMethod method, const char *fn)
{
    if (p.numel() == 0)
        throw Error(std::string(fn) + ": p must be non-empty",
                     0, 0, fn, "", std::string("m:") + fn + ":emptyP");

    ScratchArena scratch(mr);

    // Normalize probabilities into a flat vector (so prctile sees /100)
    // then validate the post-scaling value lies in [0,1].
    auto probs = ScratchVec<double>(p.numel(), &scratch);
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

Value quantile(std::pmr::memory_resource *mr, const Value &x, const Value &p, int dim)
{
    return quantileImpl(mr, x, p, dim, 1.0, QMethod::Midpoint, "quantile");
}

Value prctile(std::pmr::memory_resource *mr, const Value &x, const Value &p, int dim)
{
    return quantileImpl(mr, x, p, dim, 0.01, QMethod::Midpoint, "prctile");
}

// Internal entry point used by registration adapters which parsed
// 'all' / vecdim / Method themselves. `flatten = true` collapses x to
// a flat row vector before reduction; `method` selects the algorithm.
Value quantileWithOpts(std::pmr::memory_resource *mr, const Value &x,
                       const Value &p, int dim, bool flatten,
                       QMethod method, double pScale, const char *fn)
{
    if (flatten) {
        // Reshape into a 1×N row, then reduce along dim 2 (the only
        // non-singleton dim — gives the canonical "all" semantics).
        Value flat = Value::matrix(1, x.numel(), ValueType::DOUBLE, mr);
        if (x.numel() > 0) {
            const double *src = x.doubleData();
            std::copy(src, src + x.numel(), flat.doubleDataMut());
        }
        return quantileImpl(mr, flat, p, 2, pScale, method, fn);
    }
    return quantileImpl(mr, x, p, dim, pScale, method, fn);
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
void modeAlongDim(std::pmr::memory_resource *mr,
                  const Value &x, int redDim, T *dst, double *dstC,
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
    modeAlongDim<T>(&scratch, x, redDim,
                    static_cast<T *>(out.rawDataMut()),
                    outC.doubleDataMut(),
                    typeMatch);
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
    modeAlongDim<T>(&scratch, x, dim,
                    static_cast<T *>(out.rawDataMut()),
                    outC.doubleDataMut(),
                    typeMatch);
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
                     0, 0, fn, "", std::string("m:") + fn + ":complex");
    default:
        throw Error(std::string(fn) + ": unsupported input type",
                     0, 0, fn, "", std::string("m:") + fn + ":type");
    }
}

} // namespace

std::tuple<Value, Value>
mode(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    const int d = resolveDim(x, dim, "mode");
    return dispatchMode(x, d, mr, "mode");
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
Value covMatrixFromCentered(std::pmr::memory_resource *mr, const double *X,
                             std::size_t n, std::size_t p, double divisor)
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

Value cov(std::pmr::memory_resource *mr, const Value &x, int normFlag)
{
    validateNormFlagCov(normFlag, "cov");
    validateCovInputs(x, "cov");

    ScratchArena scratch(mr);
    ScratchVec<double> data(&scratch);
    std::size_t n, p;
    readMatrix(x, data, n, p);
    if (n == 0) {
        // MATLAB: cov of empty → NaN (or empty p×p depending on shape).
        if (p == 1) return Value::scalar(std::nan(""), mr);
        return Value::matrix(p, p, ValueType::DOUBLE, mr);
    }
    centerColumns(data.data(), n, p);

    const double divisor = (normFlag == 0)
        ? std::max(1.0, static_cast<double>(n) - 1.0)
        : static_cast<double>(n);

    if (p == 1) {
        // Vector input → return scalar variance.
        double s = 0.0;
        for (std::size_t i = 0; i < n; ++i) s += data[i] * data[i];
        return Value::scalar(s / divisor, mr);
    }
    return covMatrixFromCentered(mr, data.data(), n, p, divisor);
}

Value cov(std::pmr::memory_resource *mr, const Value &x, const Value &y, int normFlag)
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
        return Value::matrix(2, 2, ValueType::DOUBLE, mr);
    ScratchArena scratch(mr);
    auto data = ScratchVec<double>(n * 2, &scratch);
    for (std::size_t i = 0; i < n; ++i) {
        data[i] = x.elemAsDouble(i);          // column 0 (= x)
        data[n + i] = y.elemAsDouble(i);      // column 1 (= y)
    }
    centerColumns(data.data(), n, 2);
    const double divisor = (normFlag == 0)
        ? std::max(1.0, static_cast<double>(n) - 1.0)
        : static_cast<double>(n);
    return covMatrixFromCentered(mr, data.data(), n, 2, divisor);
}

namespace {

Value corrcoefFromCov(std::pmr::memory_resource *mr, const Value &C)
{
    if (C.dims().rows() != C.dims().cols())
        throw Error("corrcoef: covariance matrix must be square",
                     0, 0, "corrcoef", "", "m:corrcoef:internal");
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

} // namespace

Value corrcoef(std::pmr::memory_resource *mr, const Value &x)
{
    // Special case: vector input → 1×1 matrix [1] (variable correlated
    // with itself). Matches MATLAB's `corrcoef(rand(5,1))` behaviour.
    if (x.dims().isVector() || x.isScalar()) {
        auto R = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        R.doubleDataMut()[0] = 1.0;
        return R;
    }
    auto C = cov(mr, x);
    return corrcoefFromCov(mr, C);
}

Value corrcoef(std::pmr::memory_resource *mr, const Value &x, const Value &y)
{
    auto C = cov(mr, x, y);
    return corrcoefFromCov(mr, C);
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


// Phase P2: route nansum / nanmean through the single-pass scan kernels
// in backends/MStdNanReductions_{simd,portable}.cpp. These read the input
// once with an IsNaN mask and skip the compactNonNan scratch copy that
// the older scalar lambda needed. Vector input bypasses applyAlongDim
// entirely (no per-call scratch mr); matrix dim slices still go
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

// Helper: weighted variance / std on a flat 1-D array.
//   normFlag = 0  → divide by Σw - 0 (default for scalar w=0; sample)
//   normFlag = 1  → divide by Σw     (population; ML estimate)
//
// MATLAB's documented denominator for weighted variance is Σw (the
// "default" for vector weights — equivalent to normFlag=1 semantics).
// var(A, W, ...) with vector W therefore implies normFlag=1.
double weightedVarFlat(const double *x, const double *w, size_t n,
                       bool sqrtIt, bool omitNan)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    double sw = 0.0, sxw = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double xi = x[i], wi = w[i];
        if (omitNan && std::isnan(xi)) continue;
        if (wi < 0.0)
            throw Error("var/std: weights must be non-negative",
                        0, 0, "var/std", "", "m:varstd:negWeight");
        sw  += wi;
        sxw += wi * xi;
    }
    if (sw == 0.0) return std::numeric_limits<double>::quiet_NaN();
    const double mean = sxw / sw;
    double ss = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double xi = x[i], wi = w[i];
        if (omitNan && std::isnan(xi)) continue;
        const double d = xi - mean;
        ss += wi * d * d;
    }
    const double v = ss / sw;
    return sqrtIt ? std::sqrt(v) : v;
}

// Helper: scalar-normFlag variance/std on a flat 1-D array.
double scalarVarFlat(const double *x, size_t n, int normFlag,
                     bool sqrtIt, bool omitNan)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    double s = 0.0;
    size_t cnt = 0;
    for (size_t i = 0; i < n; ++i) {
        if (omitNan && std::isnan(x[i])) continue;
        s += x[i]; ++cnt;
    }
    if (cnt == 0) return std::numeric_limits<double>::quiet_NaN();
    if (cnt == 1) return (normFlag == 1) ? 0.0
                                         : std::numeric_limits<double>::quiet_NaN();
    const double mean = s / static_cast<double>(cnt);
    double ss = 0.0;
    for (size_t i = 0; i < n; ++i) {
        if (omitNan && std::isnan(x[i])) continue;
        const double d = x[i] - mean;
        ss += d * d;
    }
    const double denom = (normFlag == 1) ? static_cast<double>(cnt)
                                         : static_cast<double>(cnt - 1);
    const double v = ss / denom;
    return sqrtIt ? std::sqrt(v) : v;
}

// Convert a Value to a flat row (used for 'all' / vecdim full-flatten /
// weight-vector inputs). Skips no elements.
std::vector<double> flatten(const Value &x)
{
    std::vector<double> out(x.numel());
    for (size_t i = 0; i < x.numel(); ++i) out[i] = x.elemAsDouble(i);
    return out;
}

// Common var/std driver — handles every variant (scalar w + scalar dim,
// 'all', vecdim full-flatten, weight vector).
//
// `sqrtIt = true` makes this `std`.
Value varStdDispatch(std::pmr::memory_resource *mr, Span<const Value> args,
                     bool sqrtIt, const char *fn)
{
    bool omitNan = false;
    size_t n = stripNanFlag(args, omitNan, fn);

    const Value &x = args[0];

    // Parse args[1] (w) and args[2] (dim/'all'/vecdim) into a normalised
    // shape: scalar normFlag (0 or 1), or weight vector + flatten flag.
    int normFlag = 0;
    bool isWeightVec = false;
    const Value *wVec = nullptr;

    if (n >= 2 && !args[1].isEmpty()) {
        if (args[1].numel() == 1 && args[1].isNumeric()) {
            normFlag = static_cast<int>(args[1].toScalar());
            if (normFlag != 0 && normFlag != 1)
                throw Error(std::string(fn) + ": w must be 0 or 1, or a "
                            "weight vector",
                            0, 0, fn, "", std::string("m:") + fn + ":w");
        } else {
            isWeightVec = true;
            wVec = &args[1];
        }
    }

    int dim = 0;
    bool flattenAll = false;
    if (n >= 3 && !args[2].isEmpty()) {
        const Value &a = args[2];
        if (a.isChar() || a.isString()) {
            std::string s = a.toString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (s == "all") flattenAll = true;
            else throw Error(std::string(fn) + ": unknown dim flag '" + s + "'",
                              0, 0, fn, "", std::string("m:") + fn + ":dim");
        } else if (a.numel() == 1) {
            dim = static_cast<int>(a.toScalar());
        } else {
            // vecdim: only full-flatten coverage supported
            std::vector<int> dims;
            for (size_t i = 0; i < a.numel(); ++i)
                dims.push_back(static_cast<int>(a.elemAsDouble(i)));
            const int rank = x.dims().is3D() ? 3
                              : (x.dims().isVector() || x.isScalar() ? 1 : 2);
            std::vector<bool> seen(rank + 1, false);
            for (int d : dims) {
                if (d < 1 || d > rank)
                    throw Error(std::string(fn) + ": vecdim entries out of range",
                                0, 0, fn, "", std::string("m:") + fn + ":vecdim");
                seen[d] = true;
            }
            bool allCovered = true;
            for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
            if (!allCovered)
                throw Error(std::string(fn) + ": partial vecdim reduction is "
                            "not yet supported (only full-flatten vecdim)",
                            0, 0, fn, "", std::string("m:") + fn + ":vecdim");
            flattenAll = true;
        }
    }

    // ── Weighted-vector path ──────────────────────────────────────────
    if (isWeightVec) {
        if (flattenAll || dim == 0) {
            // Vector input or 'all': flatten + run weightedVarFlat.
            auto xv = flatten(x);
            auto wv = flatten(*wVec);
            if (xv.size() != wv.size())
                throw Error(std::string(fn) + ": weight vector length must "
                            "match number of elements",
                            0, 0, fn, "", std::string("m:") + fn + ":wlen");
            const double v = weightedVarFlat(xv.data(), wv.data(),
                                             xv.size(), sqrtIt, omitNan);
            return Value::scalar(v, mr);
        }
        // For matrix + weight + dim: defer (out of scope this cycle).
        throw Error(std::string(fn) + ": weight vector with non-flat dim "
                    "not yet supported",
                    0, 0, fn, "", std::string("m:") + fn + ":wDim");
    }

    // ── Flatten 'all' / vecdim path ───────────────────────────────────
    if (flattenAll) {
        auto xv = flatten(x);
        const double v = scalarVarFlat(xv.data(), xv.size(), normFlag,
                                       sqrtIt, omitNan);
        return Value::scalar(v, mr);
    }

    // ── Standard scalar-dim path ──────────────────────────────────────
    if (omitNan) {
        if (x.type() == ValueType::COMPLEX) {
            return varianceComplex(x, normFlag, dim, mr, sqrtIt, true);
        }
        Value r = sqrtIt
                    ? ::numkit::stats::nanstdev(mr, x, normFlag, dim)
                    : ::numkit::stats::nanvar  (mr, x, normFlag, dim);
        if (x.type() == ValueType::SINGLE)
            r = narrowToSingle(std::move(r), mr);
        return r;
    }
    return sqrtIt ? stdev(mr, x, normFlag, dim) : var(mr, x, normFlag, dim);
}

void var_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw Error("var: requires at least 1 argument",
                     0, 0, "var", "", "m:var:nargin");
    outs[0] = varStdDispatch(ctx.engine->resource(), args,
                             /*sqrtIt=*/false, "var");
}

void std_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw Error("std: requires at least 1 argument",
                     0, 0, "std", "", "m:std:nargin");
    outs[0] = varStdDispatch(ctx.engine->resource(), args,
                             /*sqrtIt=*/true, "std");
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
        ScratchArena scratch(ctx.engine->resource());
        auto buf = ScratchVec<double>(total, &scratch);
        const bool fastDouble = (args[0].type() == ValueType::DOUBLE);
        if (fastDouble)
            std::copy(args[0].doubleData(), args[0].doubleData() + total, buf.data());
        else
            for (size_t i = 0; i < total; ++i) buf[i] = args[0].elemAsDouble(i);
        size_t k = total;
        if (omitNan) k = compactNonNan(buf.data(), total);
        double v = medianFromSlice(buf.data(), k);
        Value r = Value::scalar(v, ctx.engine->resource());
        if (args[0].type() == ValueType::SINGLE)
            r = narrowToSingle(std::move(r), ctx.engine->resource());
        outs[0] = std::move(r);
        return;
    }
    if (omitNan) {
        rejectComplexOmitNan(args[0], "median");
        Value r = ::numkit::stats::nanmedian(ctx.engine->resource(), args[0], dim);
        if (args[0].type() == ValueType::SINGLE)
            r = narrowToSingle(std::move(r), ctx.engine->resource());
        outs[0] = std::move(r);
        return;
    }
    outs[0] = median(ctx.engine->resource(), args[0], dim);
}

// Common parser for the trailing args of quantile/prctile:
//   fn(X, p[, dim] [, Method=method])
//
// Handles:
//   * 'all' (string) — flatten input
//   * scalar dim
//   * vecdim — only [1 2] / [1 2 3] / etc. covering ALL dims is supported
//     as a synonym for 'all'; partial reductions throw a documented error.
//   * Method N-V pair: 'midpoint' (default) | 'inclusive' | 'exclusive'
//                       | 'approximate' (falls back to midpoint).
struct QArgs {
    int dim = 0;
    bool flatten = false;
    QMethod method = QMethod::Midpoint;
};

QArgs parseQArgs(Span<const Value> args, size_t start, const Value &x,
                 const char *fn)
{
    QArgs q;
    size_t i = start;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()
        && !args[i].isEmpty()) {
        if (args[i].numel() == 1) {
            q.dim = static_cast<int>(args[i].toScalar());
        } else {
            // Vecdim: covers-all-dims → flatten; otherwise error.
            std::vector<int> dims;
            for (size_t j = 0; j < args[i].numel(); ++j)
                dims.push_back(static_cast<int>(args[i].elemAsDouble(j)));
            const int rank = x.dims().is3D() ? 3
                              : (x.dims().isVector() || x.isScalar() ? 1 : 2);
            std::vector<bool> seen(rank + 1, false);
            for (int d : dims) {
                if (d < 1 || d > rank)
                    throw Error(std::string(fn) + ": vecdim entries out of range",
                                0, 0, fn, "", std::string("m:") + fn + ":vecdim");
                seen[d] = true;
            }
            bool allCovered = true;
            for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
            if (!allCovered)
                throw Error(std::string(fn) + ": partial vecdim reduction is "
                            "not yet supported in numkit (only full-flatten "
                            "vecdim like [1 2] or 'all')",
                            0, 0, fn, "", std::string("m:") + fn + ":vecdim");
            q.flatten = true;
        }
        ++i;
    } else if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        std::string s = args[i].toString();
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (s == "all") {
            q.flatten = true;
            ++i;
        }
        // else: leave for the Name-Value loop below ("Method=...")
    }

    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error(std::string(fn) + ": expected Name-Value pair",
                        0, 0, fn, "", std::string("m:") + fn + ":nv");
        std::string name = args[i].toString();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (name == "method") {
            if (!args[i + 1].isChar() && !args[i + 1].isString())
                throw Error(std::string(fn) + ": Method must be a string",
                            0, 0, fn, "", std::string("m:") + fn + ":method");
            std::string m = args[i + 1].toString();
            std::transform(m.begin(), m.end(), m.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if      (m == "midpoint")    q.method = QMethod::Midpoint;
            else if (m == "inclusive")   q.method = QMethod::Inclusive;
            else if (m == "exclusive")   q.method = QMethod::Exclusive;
            else if (m == "approximate") q.method = QMethod::Approximate;
            else
                throw Error(std::string(fn) + ": Method must be one of "
                            "{midpoint, inclusive, exclusive, approximate}",
                            0, 0, fn, "", std::string("m:") + fn + ":method");
        } else {
            throw Error(std::string(fn) + ": unknown Name-Value '" + name + "'",
                        0, 0, fn, "", std::string("m:") + fn + ":nv");
        }
        i += 2;
    }
    return q;
}

void quantile_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("quantile: requires (X, p[, dim] [, Method=method])",
                     0, 0, "quantile", "", "m:quantile:nargin");
    auto q = parseQArgs(args, 2, args[0], "quantile");
    outs[0] = quantileWithOpts(ctx.engine->resource(), args[0], args[1],
                                q.dim, q.flatten, q.method, 1.0, "quantile");
}

void prctile_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("prctile: requires (X, p[, dim] [, Method=method])",
                     0, 0, "prctile", "", "m:prctile:nargin");
    auto q = parseQArgs(args, 2, args[0], "prctile");
    outs[0] = quantileWithOpts(ctx.engine->resource(), args[0], args[1],
                                q.dim, q.flatten, q.method, 0.01, "prctile");
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
    auto [v, c] = mode(ctx.engine->resource(), args[0], dim);
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
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = cov(mr, args[0]);
        return;
    }
    // 2-arg form is ambiguous: cov(x, normFlag) vs cov(x, y).
    // Disambiguate exactly the way MATLAB does: if the second arg is a
    // scalar (0 or 1), it's normFlag; otherwise it's y.
    if (args.size() == 2) {
        if (args[1].isScalar()) {
            const double v = args[1].toScalar();
            if (v == 0.0 || v == 1.0) {
                outs[0] = cov(mr, args[0], static_cast<int>(v));
                return;
            }
        }
        outs[0] = cov(mr, args[0], args[1]);
        return;
    }
    // 3-arg form: cov(x, y, normFlag).
    const int w = static_cast<int>(args[2].toScalar());
    outs[0] = cov(mr, args[0], args[1], w);
}

void corrcoef_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("corrcoef: requires at least 1 argument",
                     0, 0, "corrcoef", "", "m:corrcoef:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = corrcoef(mr, args[0]);
        return;
    }
    outs[0] = corrcoef(mr, args[0], args[1]);
}

// nan*_reg adapters all moved to libs/stats/src/nan_aware/nan_aware.cpp.

} // namespace detail

} // namespace numkit::stats
