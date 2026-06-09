// toolboxes/builtin/src/lang/arrays/accum.cpp
//
// accumarray — group-by reduction. See public header for the user-
// facing contract. Implementation notes:
//   * Subscript matrix `subs` is N×D (1-based per MATLAB). N = number
//     of contributions, D = output dimensionality. D=1 → 1D output;
//     D=2 → 2D output; D≥3 routed through matrixND.
//   * `vals` may be a scalar (broadcast) or a length-N vector.
//   * `fillVal` populates cells that received no contribution. Cells
//     with at least one contribution start from the reducer's identity
//     (0 for sum, 1 for prod, ±inf for min/max, etc.) and get
//     replaced — never touch fillVal.
//   * For `mean`, we keep a parallel count array and divide at the end.

#include <numkit/builtin/language/arrays/accum.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value_type.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <string>

namespace numkit::builtin {

namespace {

// 1-based subscript validation: must be a positive integer ≤ outDim.
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

// Build the output shape. `userShape` (passed-in) wins if non-zero
// length, else derive from per-column max(subs). subs is N×D. Result
// vector backed by `mr`.
ScratchVec<size_t> resolveOutShape(const Value &subs, const size_t *userShape, std::size_t nUserShape, const char *fn, std::pmr::memory_resource *mr)
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
        // Empty subs + no sz → 0×0 (matches MATLAB).
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

// Column-major linear index from the row-of-subs `r` of the N×D matrix.
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

Value accumarray(const Value &subs, const Value &vals, Span<const size_t> outShape, AccumReducer op, double fillVal, std::pmr::memory_resource *mr)
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

    // Special case: empty output (any dim is 0) — MATLAB returns the
    // zero-shaped output without iterating subs. Validate subs anyway?
    // MATLAB doesn't — it just returns the empty result. Match that.
    if (total == 0)
        return out;

    // Track which cells have received contributions; uninitialized cells
    // get fillVal at the end. For `mean`, also accumulate counts.
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

    // Fill un-touched cells, finalize mean.
    if (needCount) {
        for (size_t i = 0; i < total; ++i) {
            if (!touched[i])      dst[i] = fillVal;
            else if (count[i] > 0) dst[i] /= static_cast<double>(count[i]);
        }
    } else if (fillVal != 0.0 ||
               op == AccumReducer::Prod || op == AccumReducer::Max ||
               op == AccumReducer::Min  || op == AccumReducer::All) {
        // Sum default already starts the buffer at 0, so we can skip
        // when fillVal == 0 AND op is Sum / Mean / Any (all of which
        // have identity 0 too — consistent with leaving allocator
        // zero-init in place).
        for (size_t i = 0; i < total; ++i)
            if (!touched[i]) dst[i] = fillVal;
    }

    return out;
}

namespace detail {

namespace {

// Returns true and sets `op` for a built-in reducer handle (fast streaming
// path); returns false for any other named/anonymous handle, which routes to
// accumarrayGeneral (group + call the handle per cell).
bool tryParseReducer(const Value &h, AccumReducer &op)
{
    if (!h.isFuncHandle())
        throw Error("accumarray: fn argument must be a function handle",
                     0, 0, "accumarray", "", "numkit:accumarray:fnType");
    std::string s = h.funcHandleName();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (s == "sum")  { op = AccumReducer::Sum;  return true; }
    if (s == "max")  { op = AccumReducer::Max;  return true; }
    if (s == "min")  { op = AccumReducer::Min;  return true; }
    if (s == "prod") { op = AccumReducer::Prod; return true; }
    if (s == "mean") { op = AccumReducer::Mean; return true; }
    if (s == "any")  { op = AccumReducer::Any;  return true; }
    if (s == "all")  { op = AccumReducer::All;  return true; }
    return false;
}

// General accumarray: apply an arbitrary function handle (named or anonymous)
// to the COLUMN vector of values gathered in each output cell. Empty cells get
// fillVal. MATLAB requires the handle to return a scalar per cell.
Value accumarrayGeneral(const Value &subs, const Value &vals,
                        Span<const size_t> outShape, const Value &handle,
                        double fillVal, CallContext &ctx,
                        std::pmr::memory_resource *mr)
{
    const char *fn = "accumarray";
    if (subs.type() != ValueType::DOUBLE)
        throw Error("accumarray: subs must be DOUBLE", 0, 0, fn, "", "numkit:accumarray:subType");
    if (vals.type() != ValueType::DOUBLE)
        throw Error("accumarray: vals must be DOUBLE", 0, 0, fn, "", "numkit:accumarray:valType");
    const auto &sd = subs.dims();
    if (sd.ndim() > 2)
        throw Error("accumarray: subs must be a 2D matrix", 0, 0, fn, "", "numkit:accumarray:subND");
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
    if (total == 0) return out;

    // Group the contributions per output cell, CSR-style (O(N)). Values keep
    // their original row order within a cell.
    auto lins = ScratchVec<size_t>(N, &scratch);
    auto cnt  = ScratchVec<size_t>(total, 0, &scratch);
    for (size_t r = 0; r < N; ++r) {
        lins[r] = linearIndexFromSubs(subs, r, N, D, shape.data(), fn);
        ++cnt[lins[r]];
    }
    auto off = ScratchVec<size_t>(total + 1, 0, &scratch);
    for (size_t i = 0; i < total; ++i) off[i + 1] = off[i] + cnt[i];
    auto ordered = ScratchVec<double>(N, &scratch);
    auto cur = ScratchVec<size_t>(total, 0, &scratch);
    for (size_t r = 0; r < N; ++r) {
        const size_t c = lins[r];
        ordered[off[c] + cur[c]++] = readVal(vals, r, valIsScalar);
    }

    for (size_t i = 0; i < total; ++i) {
        if (cnt[i] == 0) { dst[i] = fillVal; continue; }
        const size_t len = cnt[i];
        Value g = Value::matrix(len, 1, ValueType::DOUBLE, mr);
        std::copy(ordered.begin() + off[i], ordered.begin() + off[i] + len,
                  g.doubleDataMut());
        Value callArgs[1] = { std::move(g) };
        Value res = ctx.engine->callFunctionHandle(handle,
                                                   Span<const Value>(callArgs, 1));
        dst[i] = res.toScalar();
    }
    return out;
}

ScratchVec<size_t> parseSizeArg(const Value &sz, std::pmr::memory_resource *mr)
{
    ScratchVec<size_t> shape(mr);
    if (sz.isEmpty()) return shape;
    if (sz.type() != ValueType::DOUBLE || !sz.dims().isVector())
        throw Error("accumarray: sz must be a numeric row vector",
                     0, 0, "accumarray", "", "numkit:accumarray:sizeType");
    const size_t k = sz.numel();
    shape.resize(k);
    const double *p = sz.doubleData();
    for (size_t i = 0; i < k; ++i) {
        const double v = p[i];
        if (!std::isfinite(v) || v < 0)
            throw Error("accumarray: sz entries must be non-negative integers",
                         0, 0, "accumarray", "", "numkit:accumarray:sizeRange");
        shape[i] = static_cast<size_t>(std::round(v));
    }
    return shape;
}

} // namespace

void accumarray_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("accumarray: requires at least 2 arguments (subs, vals)",
                     0, 0, "accumarray", "", "numkit:accumarray:nargin");
    if (args.size() > 6)
        throw Error("accumarray: too many arguments",
                     0, 0, "accumarray", "", "numkit:accumarray:nargin");

    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto shape = ScratchVec<size_t>(&scratch);
    if (args.size() >= 3 && !args[2].isEmpty())
        shape = parseSizeArg(args[2], &scratch);

    AccumReducer op = AccumReducer::Sum;
    const Value *customFn = nullptr;
    if (args.size() >= 4 && !args[3].isEmpty()) {
        if (!tryParseReducer(args[3], op)) customFn = &args[3];
    }

    double fillVal = 0.0;
    if (args.size() >= 5 && !args[4].isEmpty()) {
        if (!args[4].isScalar())
            throw Error("accumarray: fillval must be a scalar",
                         0, 0, "accumarray", "", "numkit:accumarray:fillType");
        fillVal = args[4].toScalar();
    }
    if (args.size() >= 6 && !args[5].isEmpty()) {
        // sparse output flag — we don't support sparse storage. If the
        // user explicitly asks for it, fail loudly rather than silently
        // returning a dense result.
        if (args[5].toScalar() != 0.0)
            throw Error("accumarray: sparse output (issparse=1) is not supported",
                         0, 0, "accumarray", "", "numkit:accumarray:sparse");
    }

    // MATLAB accumarray accepts integer/logical vals. The output class follows
    // the reducer: sum/prod/mean -> double, but max/min PRESERVE the integer
    // class of vals (like the reductions). Promote to double for the
    // double-only inner loops, then narrow max/min back to the int class.
    const Value &valsArg = args[1];
    const bool valsInt = !valsArg.isComplex() && isIntegerType(valsArg.type());
    const bool valsProm = valsInt || valsArg.isLogical();
    Value valsHold;
    if (valsProm) valsHold = toDoubleValue(valsArg, mr);
    const Value &vals = valsProm ? valsHold : valsArg;

    Span<const size_t> shapeSpan(shape.data(), shape.size());
    if (customFn) {
        // Custom function handle: numkit applies the handle to a double group,
        // so the result is double. MATLAB passes the original-class group and
        // follows the handle's output class (e.g. @(x)x(1) on int8 -> int8) —
        // numkit stays lenient (double) on that niche; @max/@min/@sum/... are
        // recognised as built-in reducers above and take the typed path.
        outs[0] = accumarrayGeneral(args[0], vals, shapeSpan, *customFn, fillVal, ctx, mr);
    } else {
        Value res = accumarray(args[0], vals, shapeSpan, op, fillVal, mr);
        if (valsInt && (op == AccumReducer::Max || op == AccumReducer::Min))
            res = doubleToIntegerExact(res, valsArg.type(), mr);
        outs[0] = std::move(res);
    }
}

} // namespace detail

} // namespace numkit::builtin
