// src/builtin/src/datafun/accum.cpp
//
// accumarray — group-by reduction algorithm.

#include <numkit/builtin/datafun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/value_type.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <string>

namespace numkit::builtin {

namespace {

// 1-based subscript validation: must be a positive integer <= outDim.
size_t toSubIndex(double v, size_t maxAllowed, const char *fn)
{
    if (!std::isfinite(v) || v < 1.0)
        throw Error(std::string(fn) + ": subscripts must be positive integers",
                     0, 0, fn, "", std::string("numkit:") + fn + ":subRange");
    const double rounded = std::round(v);
    if (std::abs(v - rounded) > 1e-9)
        throw Error(std::string(fn) + ": subscripts must be integer-valued",
                     0, 0, fn, "", std::string("numkit:") + fn + ":subInt");
    const size_t idx = static_cast<size_t>(rounded);
    if (maxAllowed > 0 && idx > maxAllowed)
        throw Error(std::string(fn) + ": subscript exceeds output dimension",
                     0, 0, fn, "", std::string("numkit:") + fn + ":subOOB");
    return idx;
}

double identityFor(AccumReducer op)
{
    switch (op) {
    case AccumReducer::Sum:  return 0.0;
    case AccumReducer::Mean: return 0.0;
    case AccumReducer::Prod: return 1.0;
    case AccumReducer::Max:  return -std::numeric_limits<double>::infinity();
    case AccumReducer::Min:  return  std::numeric_limits<double>::infinity();
    case AccumReducer::Any:  return 0.0;
    case AccumReducer::All:  return 1.0;
    }
    return 0.0;
}

inline double applyReducer(AccumReducer op, double acc, double v)
{
    switch (op) {
    case AccumReducer::Sum:
    case AccumReducer::Mean: return acc + v;
    case AccumReducer::Prod: return acc * v;
    case AccumReducer::Max:  return (v > acc || std::isnan(v)) ? v : acc;
    case AccumReducer::Min:  return (v < acc || std::isnan(v)) ? v : acc;
    case AccumReducer::Any:  return (acc != 0.0 || v != 0.0) ? 1.0 : 0.0;
    case AccumReducer::All:  return (acc != 0.0 && v != 0.0) ? 1.0 : 0.0;
    }
    return acc;
}

// Build the output shape. userShape (passed-in) wins if non-zero
// length, else derive from per-column max(subs). subs is N x D.
ScratchVec<size_t> resolveOutShape(const Value &subs, const size_t *userShape,
                                   std::size_t nUserShape, const char *fn,
                                   std::pmr::memory_resource *mr)
{
    const auto &d = subs.dims();
    const size_t N = d.rows();
    const size_t D = (d.ndim() <= 1) ? 1 : d.cols();
    if (nUserShape > 0) {
        if (nUserShape < D)
            throw Error(std::string(fn) + ": sz length must be at least size(subs, 2)",
                         0, 0, fn, "", std::string("numkit:") + fn + ":sizeRank");
        ScratchVec<size_t> out(userShape, userShape + nUserShape, mr);
        return out;
    }
    // Auto-derive from max per column. For 1D subs, 1D output.
    ScratchVec<size_t> shape(D, mr);
    if (N == 0) {
        // Empty subs + no sz -> 0x0 (matches MATLAB).
        return ScratchVec<size_t>(std::max<size_t>(D, 1), 0, mr);
    }
    const double *p = subs.doubleData();
    for (size_t c = 0; c < D; ++c) {
        double mx = 0.0;
        for (size_t r = 0; r < N; ++r) {
            const double v = p[c * N + r];
            if (v > mx) mx = v;
        }
        shape[c] = toSubIndex(mx, 0, fn);
    }
    return shape;
}

// Column-major linear index from the row-of-subs r of the N x D matrix.
size_t linearIndexFromSubs(const Value &subs, size_t r, size_t N, size_t D,
                           const size_t *shape, const char *fn)
{
    const double *p = subs.doubleData();
    size_t idx = 0;
    size_t stride = 1;
    for (size_t c = 0; c < D; ++c) {
        const size_t s = toSubIndex(p[c * N + r], shape[c], fn);
        idx += (s - 1) * stride;
        stride *= shape[c];
    }
    return idx;
}

// Allocate the output Value for the given shape.
Value allocOutput(const size_t *shape, std::size_t nShape, std::pmr::memory_resource *mr)
{
    if (nShape == 1)
        return Value::matrix(shape[0], 1, ValueType::DOUBLE, mr);
    if (nShape == 2)
        return Value::matrix(shape[0], shape[1], ValueType::DOUBLE, mr);
    return Value::matrixND(shape, static_cast<int>(nShape),
                            ValueType::DOUBLE, mr);
}

inline double readVal(const Value &vals, size_t i, bool valIsScalar)
{
    return valIsScalar ? vals.toScalar() : vals.doubleData()[i];
}

} // namespace

Value accumarray(const Value &subs, const Value &vals, Span<const size_t> outShape,
                 AccumReducer op, double fillVal, std::pmr::memory_resource *mr)
{
    const char *fn = "accumarray";

    if (subs.type() != ValueType::DOUBLE)
        throw Error("accumarray: subs must be DOUBLE",
                     0, 0, fn, "", "numkit:accumarray:subType");
    if (vals.type() != ValueType::DOUBLE)
        throw Error("accumarray: vals must be DOUBLE",
                     0, 0, fn, "", "numkit:accumarray:valType");

    const auto &sd = subs.dims();
    if (sd.ndim() > 2)
        throw Error("accumarray: subs must be a 2D matrix",
                     0, 0, fn, "", "numkit:accumarray:subND");

    const size_t N = sd.rows();
    const size_t D = (sd.ndim() <= 1 || sd.cols() == 0) ? 1 : sd.cols();

    const bool valIsScalar = vals.isScalar();
    if (!valIsScalar && vals.numel() != N)
        throw Error("accumarray: vals must be a scalar or a length-N vector",
                     0, 0, fn, "", "numkit:accumarray:valSize");

    ScratchArena scratch(mr);
    auto shape = resolveOutShape(subs, outShape.data(), outShape.size(), fn, &scratch);
    if (shape.size() < D) shape.resize(D, 1);

    Value out = allocOutput(shape.data(), shape.size(), mr);
    const size_t total = out.numel();
    double *dst = out.doubleDataMut();

    if (total == 0)
        return out;

    auto touched = ScratchVec<uint8_t>(total, &scratch);
    const bool needCount = (op == AccumReducer::Mean);
    auto count = ScratchVec<size_t>(&scratch);
    if (needCount) count.assign(total, 0);

    const double init = identityFor(op);

    for (size_t r = 0; r < N; ++r) {
        const size_t lin = linearIndexFromSubs(subs, r, N, D, shape.data(), fn);
        const double v = readVal(vals, r, valIsScalar);
        if (!touched[lin]) {
            dst[lin] = applyReducer(op, init, v);
            touched[lin] = 1;
        } else {
            dst[lin] = applyReducer(op, dst[lin], v);
        }
        if (needCount) ++count[lin];
    }

    if (needCount) {
        for (size_t i = 0; i < total; ++i) {
            if (!touched[i])       dst[i] = fillVal;
            else if (count[i] > 0) dst[i] /= static_cast<double>(count[i]);
        }
    } else if (fillVal != 0.0 ||
               op == AccumReducer::Prod || op == AccumReducer::Max ||
               op == AccumReducer::Min  || op == AccumReducer::All) {
        for (size_t i = 0; i < total; ++i)
            if (!touched[i]) dst[i] = fillVal;
    }

    return out;
}

} // namespace numkit::builtin
