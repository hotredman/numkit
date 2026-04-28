// libs/builtin/src/MStdAccum.cpp
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

#include <numkit/m/builtin/MStdAccum.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace numkit::m::builtin {

namespace {

// 1-based subscript validation: must be a positive integer ≤ outDim.
size_t toSubIndex(double v, size_t maxAllowed, const char *fn)
{
    if (!std::isfinite(v) || v < 1.0)
        throw MError(std::string(fn) + ": subscripts must be positive integers",
                     0, 0, fn, "", std::string("m:") + fn + ":subRange");
    const double rounded = std::round(v);
    if (std::abs(v - rounded) > 1e-9)
        throw MError(std::string(fn) + ": subscripts must be integer-valued",
                     0, 0, fn, "", std::string("m:") + fn + ":subInt");
    const size_t idx = static_cast<size_t>(rounded);
    if (maxAllowed > 0 && idx > maxAllowed)
        throw MError(std::string(fn) + ": subscript exceeds output dimension",
                     0, 0, fn, "", std::string("m:") + fn + ":subOOB");
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

// Build the output shape. `userShape` (passed-in) wins if non-empty,
// else derive from per-column max(subs). subs is N×D.
std::vector<size_t> resolveOutShape(const MValue &subs,
                                    const std::vector<size_t> &userShape,
                                    const char *fn)
{
    const auto &d = subs.dims();
    const size_t N = d.rows();
    const size_t D = (d.ndim() <= 1) ? 1 : d.cols();
    if (!userShape.empty()) {
        if (userShape.size() < D)
            throw MError(std::string(fn) + ": sz length must be at least size(subs, 2)",
                         0, 0, fn, "", std::string("m:") + fn + ":sizeRank");
        return userShape;
    }
    // Auto-derive from max per column. For 1D subs, 1D output.
    std::vector<size_t> shape(D, 0);
    if (N == 0) {
        // Empty subs + no sz → 0×0 (matches MATLAB).
        return std::vector<size_t>(std::max<size_t>(D, 1), 0);
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
size_t linearIndexFromSubs(const MValue &subs, size_t r, size_t N, size_t D,
                           const std::vector<size_t> &shape, const char *fn)
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

// Allocate the output MValue for the given shape.
MValue allocOutput(Allocator &alloc, const std::vector<size_t> &shape)
{
    if (shape.size() == 1)
        return MValue::matrix(shape[0], 1, MType::DOUBLE, &alloc);
    if (shape.size() == 2)
        return MValue::matrix(shape[0], shape[1], MType::DOUBLE, &alloc);
    return MValue::matrixND(shape.data(), static_cast<int>(shape.size()),
                            MType::DOUBLE, &alloc);
}

inline double readVal(const MValue &vals, size_t i, bool valIsScalar)
{
    return valIsScalar ? vals.toScalar() : vals.doubleData()[i];
}

} // namespace

MValue accumarray(Allocator &alloc,
                  const MValue &subs,
                  const MValue &vals,
                  const std::vector<size_t> &outShape,
                  AccumReducer op,
                  double fillVal)
{
    const char *fn = "accumarray";

    if (subs.type() != MType::DOUBLE)
        throw MError("accumarray: subs must be DOUBLE",
                     0, 0, fn, "", "m:accumarray:subType");
    if (vals.type() != MType::DOUBLE)
        throw MError("accumarray: vals must be DOUBLE",
                     0, 0, fn, "", "m:accumarray:valType");

    const auto &sd = subs.dims();
    if (sd.ndim() > 2)
        throw MError("accumarray: subs must be a 2D matrix",
                     0, 0, fn, "", "m:accumarray:subND");

    const size_t N = sd.rows();
    const size_t D = (sd.ndim() <= 1 || sd.cols() == 0) ? 1 : sd.cols();

    const bool valIsScalar = vals.isScalar();
    if (!valIsScalar && vals.numel() != N)
        throw MError("accumarray: vals must be a scalar or a length-N vector",
                     0, 0, fn, "", "m:accumarray:valSize");

    auto shape = resolveOutShape(subs, outShape, fn);
    if (shape.size() < D) shape.resize(D, 1);

    MValue out = allocOutput(alloc, shape);
    const size_t total = out.numel();
    double *dst = out.doubleDataMut();

    // Special case: empty output (any dim is 0) — MATLAB returns the
    // zero-shaped output without iterating subs. Validate subs anyway?
    // MATLAB doesn't — it just returns the empty result. Match that.
    if (total == 0)
        return out;

    // Track which cells have received contributions; uninitialized cells
    // get fillVal at the end. For `mean`, also accumulate counts.
    std::vector<uint8_t> touched(total, 0);
    const bool needCount = (op == AccumReducer::Mean);
    std::vector<size_t> count;
    if (needCount) count.assign(total, 0);

    const double init = identityFor(op);

    for (size_t r = 0; r < N; ++r) {
        const size_t lin = linearIndexFromSubs(subs, r, N, D, shape, fn);
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

AccumReducer parseReducerFromHandle(const MValue &h)
{
    if (!h.isFuncHandle())
        throw MError("accumarray: fn argument must be a function handle",
                     0, 0, "accumarray", "", "m:accumarray:fnType");
    std::string s = h.funcHandleName();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (s == "sum")  return AccumReducer::Sum;
    if (s == "max")  return AccumReducer::Max;
    if (s == "min")  return AccumReducer::Min;
    if (s == "prod") return AccumReducer::Prod;
    if (s == "mean") return AccumReducer::Mean;
    if (s == "any")  return AccumReducer::Any;
    if (s == "all")  return AccumReducer::All;
    throw MError("accumarray: unsupported function handle '@" + s
                 + "' (built-in reducers: @sum/@max/@min/@prod/@mean/@any/@all)",
                 0, 0, "accumarray", "", "m:accumarray:fnUnsupported");
}

std::vector<size_t> parseSizeArg(const MValue &sz)
{
    if (sz.isEmpty()) return {};
    if (sz.type() != MType::DOUBLE || !sz.dims().isVector())
        throw MError("accumarray: sz must be a numeric row vector",
                     0, 0, "accumarray", "", "m:accumarray:sizeType");
    const size_t k = sz.numel();
    std::vector<size_t> shape(k);
    const double *p = sz.doubleData();
    for (size_t i = 0; i < k; ++i) {
        const double v = p[i];
        if (!std::isfinite(v) || v < 0)
            throw MError("accumarray: sz entries must be non-negative integers",
                         0, 0, "accumarray", "", "m:accumarray:sizeRange");
        shape[i] = static_cast<size_t>(std::round(v));
    }
    return shape;
}

} // namespace

void accumarray_reg(Span<const MValue> args, size_t /*nargout*/,
                    Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("accumarray: requires at least 2 arguments (subs, vals)",
                     0, 0, "accumarray", "", "m:accumarray:nargin");
    if (args.size() > 6)
        throw MError("accumarray: too many arguments",
                     0, 0, "accumarray", "", "m:accumarray:nargin");

    std::vector<size_t> shape;
    if (args.size() >= 3 && !args[2].isEmpty())
        shape = parseSizeArg(args[2]);

    AccumReducer op = AccumReducer::Sum;
    if (args.size() >= 4 && !args[3].isEmpty())
        op = parseReducerFromHandle(args[3]);

    double fillVal = 0.0;
    if (args.size() >= 5 && !args[4].isEmpty()) {
        if (!args[4].isScalar())
            throw MError("accumarray: fillval must be a scalar",
                         0, 0, "accumarray", "", "m:accumarray:fillType");
        fillVal = args[4].toScalar();
    }
    if (args.size() >= 6 && !args[5].isEmpty()) {
        // sparse output flag — we don't support sparse storage. If the
        // user explicitly asks for it, fail loudly rather than silently
        // returning a dense result.
        if (args[5].toScalar() != 0.0)
            throw MError("accumarray: sparse output (issparse=1) is not supported",
                         0, 0, "accumarray", "", "m:accumarray:sparse");
    }

    outs[0] = accumarray(ctx.engine->allocator(),
                         args[0], args[1], shape, op, fillVal);
}

} // namespace detail

} // namespace numkit::m::builtin
