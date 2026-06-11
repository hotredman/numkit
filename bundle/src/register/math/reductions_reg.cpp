// toolboxes/signal/src/math/arithmetic/reductions_reg.cpp
//
// CallContext register half of math/arithmetic/reductions.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/lang/arrays/matrix.hpp>       // reshape (for 'all')
#include <numkit/core/engine.hpp>
#include <numkit/math/arithmetic/reductions.hpp>
#include <numkit/math/arithmetic/rounding.hpp>       // abs adapter
#include <numkit/math/exp_log/exponents.hpp>      // exp / log adapters
#include <numkit/math/trig/trigonometry.hpp>   // sin / cos adapters
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "_unary_hint.hpp"  // 3-arg sin/cos/exp/log/abs hint overloads
#include <numkit/ops/helpers.hpp>
#include "arithmetic/var_reduction.hpp"  // for sumScan + addInto
#include <numkit/ops/reductions.hpp>
#include "arithmetic/reductions_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::builtin {
using namespace numkit::lang;  // C4c cross-area (reshape/poly_of_matrix)
using namespace numkit::math;  // C4c localized (umbrella removed)

namespace detail {

// Helper to reduce boilerplate — unary adapter that calls Fn(mr, args[0]).
#define NK_UNARY_ADAPTER(name, fn)                                              \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "numkit:" #name ":nargin");           \
        outs[0] = fn(args[0], ctx.engine->resource());                         \
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
                         0, 0, #name, "", "numkit:" #name ":nargin");           \
        outs[0] = fn(args[0], &outs[0], ctx.engine->resource());               \
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
                 0, 0, fn, "", std::string("numkit:") + fn + ":outtype");
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
                     0, 0, fn, "", std::string("numkit:") + fn + ":nativeType");
    case ValueType::COMPLEX:
        // COMPLEX is handled before this call (dispatchReductionAdapter
        // routes it to the complex path), so reaching this branch is a
        // bug in the dispatcher.
        return ValueType::COMPLEX;
    default:
        throw Error(std::string(fn) + ": unsupported input type for 'native'",
                     0, 0, fn, "", std::string("numkit:") + fn + ":type");
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
inline Value makeNativeScalar(T v, ValueType outType, std::pmr::memory_resource *mr)
{
    if (outType == ValueType::DOUBLE)
        return Value::scalar(static_cast<double>(v), mr);
    auto r = Value::matrix(1, 1, outType, mr);
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

inline Value allocReduceOutput(const Value &x, int redDim, ValueType outType, std::pmr::memory_resource *mr)
{
    if (x.dims().ndim() >= 4 && redDim >= 1 && redDim <= x.dims().ndim()) {
        ScratchArena scratch(mr);
        auto shape = numkit::ops::outShapeForDimND(&scratch, x, redDim);
        return Value::matrixND(shape.data(), (int) shape.size(), outType, mr);
    }
    auto outShape = numkit::ops::outShapeForDim(x, redDim);
    return createMatrix(outShape, outType, mr);
}

template <typename T, typename Init, typename AccumOp, typename Finalize>
Value reduceTypedAll(const Value &x, ValueType outType, std::pmr::memory_resource *mr,
                      Init init, AccumOp accumOp, Finalize finalize)
{
    if (x.isEmpty()) {
        // Empty input: scalar reduce → identity (sum=0, prod=1, mean=NaN).
        const T v = finalize(static_cast<double>(init), 0);
        return makeNativeScalar<T>(v, outType, mr);
    }
    if (x.isScalar() || x.dims().isVector()) {
        double acc = init;
        for (size_t i = 0; i < x.numel(); ++i)
            acc = accumOp(acc, x.elemAsDouble(i));
        return makeNativeScalar<T>(finalize(acc, x.numel()), outType, mr);
    }
    const int redDim = numkit::ops::firstNonSingletonDim(x);
    Value out = allocReduceOutput(x, redDim, outType, mr);
    typedReduceAlongDim<T>(x, redDim, static_cast<T *>(out.rawDataMut()),
                           init, accumOp, finalize);
    return out;
}

template <typename T, typename Init, typename AccumOp, typename Finalize>
Value reduceTypedAlongDim(const Value &x, int dim, ValueType outType, std::pmr::memory_resource *mr,
                           Init init, AccumOp accumOp, Finalize finalize)
{
    if (x.isEmpty() && x.dims().ndim() < 4) {
        return Value::matrix(0, 0, outType, mr);
    }
    if (x.isScalar() || x.dims().isVector()) {
        if (dim != numkit::ops::firstNonSingletonDim(x)) {
            // Identity: copy x cast to T.
            const size_t n = x.numel();
            Value out;
            if (x.dims().isVector())
                out = createMatrix({x.dims().rows(), x.dims().cols(), 0}, outType, mr);
            else
                return makeNativeScalar<T>(toOutT<T>(x.elemAsDouble(0)), outType, mr);
            T *dst = static_cast<T *>(out.rawDataMut());
            for (size_t i = 0; i < n; ++i)
                dst[i] = toOutT<T>(x.elemAsDouble(i));
            return out;
        }
        return reduceTypedAll<T>(x, outType, mr, init, accumOp, finalize);
    }
    Value out = allocReduceOutput(x, dim, outType, mr);
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
Value reduceAllElementsScalar(const Value &x, ValueType outType, std::pmr::memory_resource *mr,
                               Init init, AccumOp accumOp, Finalize finalize)
{
    double acc = init;
    const size_t n = x.numel();
    for (size_t i = 0; i < n; ++i)
        acc = accumOp(acc, x.elemAsDouble(i));
    return makeNativeScalar<T>(finalize(acc, n), outType, mr);
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

inline Value allocComplexReduceOutput(const Value &x, int redDim, std::pmr::memory_resource *mr)
{
    if (x.dims().ndim() >= 4 && redDim >= 1 && redDim <= x.dims().ndim()) {
        ScratchArena scratch(mr);
        auto shape = numkit::ops::outShapeForDimND(&scratch, x, redDim);
        return Value::matrixND(shape.data(), (int) shape.size(), ValueType::COMPLEX, mr);
    }
    auto outShape = numkit::ops::outShapeForDim(x, redDim);
    return createMatrix(outShape, ValueType::COMPLEX, mr);
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
Value reduceComplexAll(const Value &x, std::pmr::memory_resource *mr,
                        Complex init, AccumOp accumOp, Finalize finalize)
{
    if (x.isEmpty() && x.dims().ndim() < 4)
        return Value::matrix(0, 0, ValueType::COMPLEX, mr);
    const bool typeMatches = (x.type() == ValueType::COMPLEX);
    if (x.isScalar() || x.dims().isVector()) {
        Complex acc = init;
        for (size_t i = 0; i < x.numel(); ++i)
            acc = accumOp(acc, readElemAsComplex(x, i, typeMatches));
        return Value::complexScalar(finalize(acc, x.numel()), mr);
    }
    const int redDim = numkit::ops::firstNonSingletonDim(x);
    Value out = allocComplexReduceOutput(x, redDim, mr);
    complexReduceAlongDim(x, redDim, out.complexDataMut(), init, accumOp, finalize);
    return out;
}

template <typename AccumOp, typename Finalize>
Value reduceComplexAlongDim(const Value &x, int dim, std::pmr::memory_resource *mr,
                             Complex init, AccumOp accumOp, Finalize finalize)
{
    if (x.isEmpty() && x.dims().ndim() < 4)
        return Value::matrix(0, 0, ValueType::COMPLEX, mr);
    const bool typeMatches = (x.type() == ValueType::COMPLEX);
    if (x.isScalar() || x.dims().isVector()) {
        if (dim != numkit::ops::firstNonSingletonDim(x)) {
            // Identity reduction.
            const size_t n = x.numel();
            if (!x.dims().isVector())
                return Value::complexScalar(readElemAsComplex(x, 0, typeMatches), mr);
            Value out = createMatrix({x.dims().rows(), x.dims().cols(), 0},
                                      ValueType::COMPLEX, mr);
            Complex *dst = out.complexDataMut();
            for (size_t i = 0; i < n; ++i)
                dst[i] = readElemAsComplex(x, i, typeMatches);
            return out;
        }
        return reduceComplexAll(x, mr, init, accumOp, finalize);
    }
    Value out = allocComplexReduceOutput(x, dim, mr);
    complexReduceAlongDim(x, dim, out.complexDataMut(), init, accumOp, finalize);
    return out;
}

template <typename AccumOp, typename Finalize>
Value reduceComplexAllElementsScalar(const Value &x, std::pmr::memory_resource *mr,
                                      Complex init, AccumOp accumOp, Finalize finalize)
{
    const bool typeMatches = (x.type() == ValueType::COMPLEX);
    Complex acc = init;
    const size_t n = x.numel();
    for (size_t i = 0; i < n; ++i)
        acc = accumOp(acc, readElemAsComplex(x, i, typeMatches));
    return Value::complexScalar(finalize(acc, n), mr);
}

template <typename Op>
Value runComplexReduction(const Value &x, int dim, std::pmr::memory_resource *mr, bool isAll = false)
{
    if (isAll)
        return reduceComplexAllElementsScalar(x, mr, Op::cInit(), Op::cAccum, Op::cFinalize);
    return (dim > 0)
        ? reduceComplexAlongDim(x, dim, mr, Op::cInit(), Op::cAccum, Op::cFinalize)
        : reduceComplexAll(x, mr, Op::cInit(), Op::cAccum, Op::cFinalize);
}

template <typename Op>
Value runNativeReduction(const Value &x, int dim, ValueType outType, std::pmr::memory_resource *mr,
                          bool isAll = false)
{
    auto run = [&](auto tag) {
        using T = decltype(tag);
        if (isAll)
            return reduceAllElementsScalar<T>(x, outType, mr,
                                              Op::init, Op::accum, Op::template finalize<T>);
        return (dim > 0)
            ? reduceTypedAlongDim<T>(x, dim, outType, mr,
                                     Op::init, Op::accum, Op::template finalize<T>)
            : reduceTypedAll<T>(x, outType, mr,
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
                     0, 0, "", "", "numkit:nativeReduce:type");
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
Value nanReduceAll(const Value &x, ValueType outType, std::pmr::memory_resource *mr)
{
    if (x.isEmpty() && x.dims().ndim() < 4)
        return Value::matrix(0, 0, outType, mr);
    if (x.isScalar() || x.dims().isVector()) {
        double acc = Op::init;
        size_t count = 0;
        for (size_t i = 0; i < x.numel(); ++i)
            nanAccumDouble<Op>(acc, count, x.elemAsDouble(i));
        return makeNativeScalar<T>(nanFinalize<T, Op>(acc, count), outType, mr);
    }
    const int redDim = numkit::ops::firstNonSingletonDim(x);
    Value out = allocReduceOutput(x, redDim, outType, mr);
    nanReduceAlongDim<T, Op>(x, redDim, static_cast<T *>(out.rawDataMut()));
    return out;
}

template <typename T, typename Op>
Value nanReduceAlongDimImpl(const Value &x, int dim, ValueType outType, std::pmr::memory_resource *mr)
{
    if (x.isEmpty() && x.dims().ndim() < 4)
        return Value::matrix(0, 0, outType, mr);
    if (x.isScalar() || x.dims().isVector()) {
        if (dim != numkit::ops::firstNonSingletonDim(x)) {
            // Identity: copy x as outType (cast where needed).
            const size_t n = x.numel();
            if (!x.dims().isVector())
                return makeNativeScalar<T>(toOutT<T>(x.elemAsDouble(0)), outType, mr);
            Value out = createMatrix({x.dims().rows(), x.dims().cols(), 0}, outType, mr);
            T *dst = static_cast<T *>(out.rawDataMut());
            for (size_t i = 0; i < n; ++i) dst[i] = toOutT<T>(x.elemAsDouble(i));
            return out;
        }
        return nanReduceAll<T, Op>(x, outType, mr);
    }
    Value out = allocReduceOutput(x, dim, outType, mr);
    nanReduceAlongDim<T, Op>(x, dim, static_cast<T *>(out.rawDataMut()));
    return out;
}

template <typename T, typename Op>
Value nanReduceAllElementsScalar(const Value &x, ValueType outType, std::pmr::memory_resource *mr)
{
    double acc = Op::init;
    size_t count = 0;
    for (size_t i = 0; i < x.numel(); ++i)
        nanAccumDouble<Op>(acc, count, x.elemAsDouble(i));
    return makeNativeScalar<T>(nanFinalize<T, Op>(acc, count), outType, mr);
}

template <typename Op>
Value runNanReduction(const Value &x, int dim, ValueType outType, std::pmr::memory_resource *mr,
                       bool isAll = false)
{
    auto run = [&](auto tag) {
        using T = decltype(tag);
        if (isAll)
            return nanReduceAllElementsScalar<T, Op>(x, outType, mr);
        return (dim > 0)
            ? nanReduceAlongDimImpl<T, Op>(x, dim, outType, mr)
            : nanReduceAll<T, Op>(x, outType, mr);
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
                     0, 0, "", "", "numkit:nanReduce:type");
    }
}

// COMPLEX nan-aware reduction: an element is NaN if either its real
// or imag part is NaN.
inline bool isComplexNaN(Complex c)
{
    return std::isnan(c.real()) || std::isnan(c.imag());
}

template <typename Op>
Value runComplexNanReduction(const Value &x, int dim, std::pmr::memory_resource *mr,
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
        return Value::complexScalar(reduceRange(0, 1, x.numel()), mr);
    }
    const int d = (dim > 0) ? dim : numkit::ops::firstNonSingletonDim(x);
    Value out = allocComplexReduceOutput(x, d, mr);
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
                                 0, 0, fn, "", std::string("numkit:") + fn + ":dupDim");
                r.isAll = true; haveDim = true; break;
            case StringFlag::OutType:
                if (haveOutType)
                    throw Error(std::string(fn) + ": output type specified twice",
                                 0, 0, fn, "", std::string("numkit:") + fn + ":outtypeDup");
                r.outMode = parseOutTypeMode(a, fn); haveOutType = true; break;
            case StringFlag::NanFlag:
                if (haveNanFlag)
                    throw Error(std::string(fn) + ": nan flag specified twice",
                                 0, 0, fn, "", std::string("numkit:") + fn + ":nanFlagDup");
                r.omitNan = (lowercaseStr(a) == "omitnan");
                haveNanFlag = true; break;
            default:
                throw Error(std::string(fn) + ": unknown flag '" + lowercaseStr(a) + "'",
                             0, 0, fn, "", std::string("numkit:") + fn + ":badFlag");
            }
        } else {
            if (haveDim)
                throw Error(std::string(fn) + ": dim specified twice",
                             0, 0, fn, "", std::string("numkit:") + fn + ":dupDim");
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
                         0, 0, #name, "", "numkit:" #name ":nargin");                  \
        outs[0] = dispatchReductionAdapter(args, #name,                           \
            [&](const Value &x, int dim, bool isAll) {                           \
                if (isAll)                                                        \
                    return runNativeReduction<op>(x, 0, ValueType::DOUBLE,            \
                                                  ctx.engine->resource(), true);\
                return (dim > 0) ? fn(x, dim, ctx.engine->resource())            \
                                 : fn(x, ctx.engine->resource());                \
            },                                                                    \
            [&](const Value &x, int dim, ValueType outT, bool isAll) {               \
                return runNativeReduction<op>(x, dim, outT,                       \
                                              ctx.engine->resource(), isAll);   \
            },                                                                    \
            [&](const Value &x, int dim, bool isAll) {                           \
                return runComplexReduction<op>(x, dim,                            \
                                               ctx.engine->resource(), isAll);  \
            },                                                                    \
            [&](const Value &x, int dim, ValueType outT, bool isAll) {               \
                return runNanReduction<op>(x, dim, outT,                          \
                                           ctx.engine->resource(), isAll);      \
            },                                                                    \
            [&](const Value &x, int dim, bool isAll) {                           \
                return runComplexNanReduction<op>(x, dim,                         \
                                                  ctx.engine->resource(), isAll);\
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
//   max(A, B, 'omitnan')         — elementwise NaN-skip (when one operand is NaN
//                                  the result equals the other; both NaN → NaN)
// Trailing 'omitnan' / 'includenan' string is recognised in both forms.
namespace {

// Detect optional trailing 'omitnan'/'includenan' string in the reduction
// form, returning the effective arg count (excluding the flag if present)
// and the omit flag.
size_t stripTrailingNanFlag(Span<const Value> args, bool &omitNan)
{
    // MATLAB's DEFAULT for max/min is 'omitnan' (NaN is ignored unless every
    // element is NaN). Only an explicit 'includenan' turns omission off.
    // (sum/mean/etc. default to 'includenan'; max/min are the exception.)
    omitNan = true;
    size_t n = args.size();
    if (n == 0) return 0;
    const Value &last = args[n - 1];
    if (last.type() != ValueType::CHAR) return n;
    std::string s = last.toString();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (s == "omitnan")    { omitNan = true;  return n - 1; }
    if (s == "includenan") { omitNan = false; return n - 1; }
    return n;
}

} // namespace

void max_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("max: requires at least 1 argument",
                     0, 0, "max", "", "numkit:max:nargin");
    bool omitNan = false;
    const size_t n = stripTrailingNanFlag(args, omitNan);
    auto *mr = ctx.engine->resource();
    // MATLAB returns DOUBLE (the code points) for max/min of a CHAR array — the
    // char class is NOT preserved (unlike sort/unique/cummax; note mode DOES
    // keep char). Promote char operands to double up front so every form
    // (reduction / 'all' / dim / binary / 2nd-output index) yields a double
    // result. bugs/builtin/maxmin-char-double.md.
    const Value a0 = args[0].isChar() ? toDoubleValue(args[0], mr) : args[0];
    if (n >= 2 && !args[1].isEmpty()) {
        // Elementwise max(A, B) — single-return form. NaN-aware variant
        // when 'omitnan' was passed.
        const Value a1 = args[1].isChar() ? toDoubleValue(args[1], mr) : args[1];
        outs[0] = omitNan ? maxOmitNanBinary(a0, a1, mr) : max(a0, a1, mr);
        return;
    }
    // max(A, [], 'all'[, 'linear']) — reduce over EVERY element. Flatten to
    // a column and reduce; the column position IS the linear index (matching
    // MATLAB's 'all' 2nd output, which is always linear).
    for (size_t i = 1; i < n; ++i)
        if (isStringArg(args[i]) && lowercaseStr(args[i]) == "all") {
            Value flat = reshape(a0, a0.numel(), 1, 0, mr);
            auto [v, ix] = omitNan ? maxOmitNan(flat, 0, mr) : max(flat, 0, mr);
            outs[0] = std::move(v);
            if (nargout > 1) outs[1] = std::move(ix);
            return;
        }
    // Reduction: optional dim as args[2].
    int dim = 0;
    if (n >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    auto [val, idx] = omitNan ? maxOmitNan(a0, dim, mr) : max(a0, dim, mr);
    outs[0] = std::move(val);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

void min_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("min: requires at least 1 argument",
                     0, 0, "min", "", "numkit:min:nargin");
    bool omitNan = false;
    const size_t n = stripTrailingNanFlag(args, omitNan);
    auto *mr = ctx.engine->resource();
    // MATLAB returns DOUBLE (code points) for min of a CHAR array — see max_reg
    // + bugs/builtin/maxmin-char-double.md.
    const Value a0 = args[0].isChar() ? toDoubleValue(args[0], mr) : args[0];
    if (n >= 2 && !args[1].isEmpty()) {
        const Value a1 = args[1].isChar() ? toDoubleValue(args[1], mr) : args[1];
        outs[0] = omitNan ? minOmitNanBinary(a0, a1, mr) : min(a0, a1, mr);
        return;
    }
    // min(A, [], 'all'[, 'linear']) — reduce over every element (see max_reg).
    for (size_t i = 1; i < n; ++i)
        if (isStringArg(args[i]) && lowercaseStr(args[i]) == "all") {
            Value flat = reshape(a0, a0.numel(), 1, 0, mr);
            auto [v, ix] = omitNan ? minOmitNan(flat, 0, mr) : min(flat, 0, mr);
            outs[0] = std::move(v);
            if (nargout > 1) outs[1] = std::move(ix);
            return;
        }
    int dim = 0;
    if (n >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    auto [val, idx] = omitNan ? minOmitNan(a0, dim, mr) : min(a0, dim, mr);
    outs[0] = std::move(val);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

// Generators
void linspace_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("linspace: requires at least 2 arguments",
                     0, 0, "linspace", "", "numkit:linspace:nargin");
    const double a = args[0].toScalar();
    const double b = args[1].toScalar();
    const size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 100u;
    outs[0] = linspace(a, b, n, ctx.engine->resource());
}

void logspace_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("logspace: requires at least 2 arguments",
                     0, 0, "logspace", "", "numkit:logspace:nargin");
    const double a = args[0].toScalar();
    const double b = args[1].toScalar();
    const size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 50u;
    outs[0] = logspace(a, b, n, ctx.engine->resource());
}

// rand_reg / randn_reg moved to rng.cpp — they share a single
// process-static engine with randi / randperm so MATLAB-style
// rng(seed) controls all of them. The C++ public APIs rand(mr,
// rng, …) / randn(mr, rng, …) above stay here unchanged.

} // namespace detail

} // namespace numkit::builtin
