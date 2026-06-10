// toolboxes/signal/src/language/arrays/matrix_reg.cpp
//
// CallContext register half of language/arrays/matrix.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/language/arrays/manip.hpp>     // flip()
#include <numkit/builtin/language/arrays/matrix.hpp>
#include <numkit/builtin/language/operators/unary_ops.hpp>  // transposeNC()
#include <numkit/builtin/math/poly/polynomials.hpp>
#include <numkit/ops/binary_ops.hpp>
#include <numkit/ops/la_solve.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "helpers.hpp"
#include "math/arithmetic/cumsum.hpp"
#include "matrix_detail.hpp"
#include "reduction_helpers.hpp"
#include "rows_helpers.hpp"
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
using namespace numkit::lang;  // C4c localized (umbrella removed)

namespace detail {

void zeros_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    // Strip trailing class-name (e.g. 'uint8') or 'like' form before
    // parsing dims. Default type is DOUBLE.
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(d);
    // Value::matrix*/matrixND zero-fill the buffer for any type, so
    // createMatrixND with the requested type IS the zeros() output.
    outs[0] = createMatrixND(d.data(), d.size(), t, mr);
}

// Fill `v` with one in its declared type (1 / 1.0 / true).
namespace { inline void fillOnes(Value &v, ValueType t)
{
    const size_t n = v.numel();
    if (n == 0) return;
    switch (t) {
      case ValueType::DOUBLE:  { auto *p = v.doubleDataMut();  std::fill(p, p + n, 1.0); break; }
      case ValueType::SINGLE:  { auto *p = v.singleDataMut();  std::fill(p, p + n, 1.0f); break; }
      case ValueType::LOGICAL: { auto *p = v.logicalDataMut(); std::fill(p, p + n, uint8_t(1)); break; }
      case ValueType::INT8:    { auto *p = v.int8DataMut();    std::fill(p, p + n, int8_t(1)); break; }
      case ValueType::INT16:   { auto *p = v.int16DataMut();   std::fill(p, p + n, int16_t(1)); break; }
      case ValueType::INT32:   { auto *p = v.int32DataMut();   std::fill(p, p + n, int32_t(1)); break; }
      case ValueType::INT64:   { auto *p = v.int64DataMut();   std::fill(p, p + n, int64_t(1)); break; }
      case ValueType::UINT8:   { auto *p = v.uint8DataMut();   std::fill(p, p + n, uint8_t(1)); break; }
      case ValueType::UINT16:  { auto *p = v.uint16DataMut();  std::fill(p, p + n, uint16_t(1)); break; }
      case ValueType::UINT32:  { auto *p = v.uint32DataMut();  std::fill(p, p + n, uint32_t(1)); break; }
      case ValueType::UINT64:  { auto *p = v.uint64DataMut();  std::fill(p, p + n, uint64_t(1)); break; }
      default: throw Error("ones: unsupported type for fill",
                           0, 0, "ones", "", "numkit:ones:badType");
    }
}}

void ones_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(d);
    auto m = createMatrixND(d.data(), d.size(), t, mr);
    fillOnes(m, t);
    outs[0] = std::move(m);
}

// MATLAB's colon function: colon(j, k) = j:k, colon(j, i, k) = j:i:k.
// Useful when the operator form is awkward (function-handle slot, etc.)
// and to be a real callable for parity tests. Type preservation matches
// the operator path (see core/src/tree_walker.cpp:colonOutputType and
// core/src/vm.cpp:OpCode::COLON).
namespace { ValueType colonOutType(const Value *ops, size_t n)
{
    ValueType nonDouble = ValueType::DOUBLE;
    bool found = false;
    for (size_t i = 0; i < n; ++i) {
        ValueType t = ops[i].type();
        if (t == ValueType::DOUBLE) continue;
        if (!found) { nonDouble = t; found = true; }
        else if (t != nonDouble)
            throw Error("colon: operands must be all the same type, "
                        "or mixed with real scalar doubles",
                        0, 0, "colon", "", "numkit:colon:typeMix");
    }
    return nonDouble;
}}

void colon_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        ValueType t = colonOutType(args.data(), 2);
        outs[0] = Value::colonRangeTyped(args[0].toScalar(),
                                          args[1].toScalar(), t, mr);
    } else if (args.size() == 3) {
        ValueType t = colonOutType(args.data(), 3);
        outs[0] = Value::colonRangeTyped(args[0].toScalar(),
                                          args[1].toScalar(),
                                          args[2].toScalar(), t, mr);
    } else {
        throw Error("colon: requires 2 or 3 arguments",
                    0, 0, "colon", "", "numkit:colon:nargin");
    }
}

// MATLAB's sparse() with size args allocates an MxN sparse zero matrix.
// Numkit has no sparse storage class -- this stub returns dense zeros.
// Matches issparse=false (we ship that stub; see types.cpp:issparse).
// KNOWN GAP: numkit returns dense; MATLAB returns sparse storage class.
// All numerical operations match (zeros on both sides), only class()
// differs. Documented in PROGRESS for sparse().
void sparse_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        // sparse(M, N) -- allocate MxN dense zeros.
        const size_t M = static_cast<size_t>(args[0].toScalar());
        const size_t N = static_cast<size_t>(args[1].toScalar());
        outs[0] = Value::matrix(M, N, ValueType::DOUBLE, mr);
        return;
    }
    if (args.size() == 1) {
        // sparse(A) -- "convert" dense to sparse. We just return A as-is
        // (since we have no sparse storage). For most numerical use this
        // is correct; isparse() still returns false (matches numkit
        // semantics).
        outs[0] = args[0];
        return;
    }
    throw Error("sparse: numkit has no sparse storage; supports only "
                "sparse(M, N) → dense zeros and sparse(A) → A passthrough",
                0, 0, "sparse", "", "numkit:sparse:NoSparse");
}

// `nan` / `NaN` / `inf` / `Inf` are MATLAB built-in functions (not
// constants): bare `nan` returns scalar NaN; `nan(M, N, ..., 'type')`
// returns float array filled with NaN (only 'double' or 'single' are
// allowed -- integer types can't represent NaN/Inf, MATLAB throws).
// Same shape parsing as zeros/ones.
namespace { void nanInfFill(Value &v, double fillD, float fillS, ValueType t)
{
    const size_t n = v.numel();
    if (n == 0) return;
    if (t == ValueType::DOUBLE) {
        auto *p = v.doubleDataMut(); std::fill(p, p + n, fillD);
    } else if (t == ValueType::SINGLE) {
        auto *p = v.singleDataMut(); std::fill(p, p + n, fillS);
    }
}}

void nan_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr);
        return;
    }
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    if (t != ValueType::DOUBLE && t != ValueType::SINGLE)
        throw Error("nan: type must be 'double' or 'single' (NaN is float-only)",
                    0, 0, "nan", "", "numkit:nan:badType");
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(d);
    auto m = createMatrixND(d.data(), d.size(), t, mr);
    nanInfFill(m, std::numeric_limits<double>::quiet_NaN(),
                  std::numeric_limits<float>::quiet_NaN(), t);
    outs[0] = std::move(m);
}

void inf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::scalar(std::numeric_limits<double>::infinity(), mr);
        return;
    }
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    if (t != ValueType::DOUBLE && t != ValueType::SINGLE)
        throw Error("inf: type must be 'double' or 'single' (Inf is float-only)",
                    0, 0, "inf", "", "numkit:inf:badType");
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(d);
    auto m = createMatrixND(d.data(), d.size(), t, mr);
    nanInfFill(m, std::numeric_limits<double>::infinity(),
                  std::numeric_limits<float>::infinity(), t);
    outs[0] = std::move(m);
}

// `true` and `false` are MATLAB built-in functions (not constants):
// bare `true` returns a scalar logical 1; `true(M, N, ...)` returns a
// logical array filled with 1 (or 0 for false). Mirrors zeros/ones
// shape parsing. See BUGS.md #30.
void true_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::logicalScalar(true, mr);
        return;
    }
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, args);
    stripTrailingOnes(d);
    auto v = createMatrixND(d.data(), d.size(), ValueType::LOGICAL, mr);
    uint8_t *p = v.logicalDataMut();
    for (size_t i = 0; i < v.numel(); ++i) p[i] = 1;
    outs[0] = std::move(v);
}

void false_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::logicalScalar(false, mr);
        return;
    }
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, args);
    stripTrailingOnes(d);
    // createMatrixND zero-fills LOGICAL by default.
    outs[0] = createMatrixND(d.data(), d.size(), ValueType::LOGICAL, mr);
}

void eye_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    auto d = parseDimsArgs(dimArgs);
    if (t == ValueType::DOUBLE) {
        // Fast path: direct double eye().
        outs[0] = eye(d.rows, d.cols, mr);
        return;
    }
    // Typed eye: zero-fill matrix of `t`, then set diagonal to one.
    auto m = Value::matrix(d.rows, d.cols, t, mr);
    const size_t k = std::min(d.rows, d.cols);
    switch (t) {
      case ValueType::SINGLE: { auto *p = m.singleDataMut(); for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1.0f; break; }
      case ValueType::LOGICAL:{ auto *p = m.logicalDataMut(); for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::INT8:   { auto *p = m.int8DataMut();    for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::INT16:  { auto *p = m.int16DataMut();   for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::INT32:  { auto *p = m.int32DataMut();   for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::INT64:  { auto *p = m.int64DataMut();   for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::UINT8:  { auto *p = m.uint8DataMut();   for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::UINT16: { auto *p = m.uint16DataMut();  for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::UINT32: { auto *p = m.uint32DataMut();  for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::UINT64: { auto *p = m.uint64DataMut();  for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      default: throw Error("eye: unsupported type", 0, 0, "eye", "", "numkit:eye:badType");
    }
    outs[0] = std::move(m);
}

void magic_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("magic: requires exactly 1 argument",
                     0, 0, "magic", "", "numkit:magic:nargin");
    const double nd = args[0].toScalar();
    if (nd < 0.0 || nd != std::floor(nd))
        throw Error("magic: N must be a non-negative integer",
                     0, 0, "magic", "", "numkit:magic:badN");
    outs[0] = magic(static_cast<size_t>(nd), ctx.engine->resource());
}

void toeplitz_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("toeplitz: requires 1 or 2 arguments",
                    0, 0, "toeplitz", "", "numkit:toeplitz:nargin");
    const Value &r = args.size() == 2 ? args[1] : Value::Empty;
    outs[0] = toeplitz(args[0], r, ctx.engine->resource());
}

void hankel_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("hankel: requires 1 or 2 arguments",
                    0, 0, "hankel", "", "numkit:hankel:nargin");
    const Value &r = args.size() == 2 ? args[1] : Value::Empty;
    outs[0] = hankel(args[0], r, ctx.engine->resource());
}

void vander_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("vander: requires exactly 1 argument",
                    0, 0, "vander", "", "numkit:vander:nargin");
    outs[0] = vander(args[0], ctx.engine->resource());
}

void compan_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("compan: requires exactly 1 argument",
                    0, 0, "compan", "", "numkit:compan:nargin");
    outs[0] = compan(args[0], ctx.engine->resource());
}

namespace {

// Common gateway for the "size-from-scalar" test-matrix functions
// (pascal, hilb, invhilb, wilkinson, hadamard).
size_t requireSizeArg(Span<const Value> args, const char *fn)
{
    if (args.size() != 1)
        throw Error(std::string(fn) + ": requires exactly 1 argument",
                    0, 0, fn, "", std::string("numkit:") + fn + ":nargin");
    const double nd = args[0].toScalar();
    if (nd < 0.0 || nd != std::floor(nd))
        throw Error(std::string(fn) + ": N must be a non-negative integer",
                    0, 0, fn, "", std::string("numkit:") + fn + ":badN");
    return static_cast<size_t>(nd);
}

} // anonymous namespace

void pascal_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = pascal(requireSizeArg(args, "pascal"), ctx.engine->resource());
}

void hilb_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = hilb(requireSizeArg(args, "hilb"), ctx.engine->resource());
}

void invhilb_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = invhilb(requireSizeArg(args, "invhilb"), ctx.engine->resource());
}

void wilkinson_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = wilkinson(requireSizeArg(args, "wilkinson"), ctx.engine->resource());
}

void hadamard_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = hadamard(requireSizeArg(args, "hadamard"), ctx.engine->resource());
}

void rosser_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (!args.empty())
        throw Error("rosser: takes no arguments",
                    0, 0, "rosser", "", "numkit:rosser:nargin");
    outs[0] = rosser(ctx.engine->resource());
}

// NOTE: inv_reg migrated to toolboxes/linalg (properties.cpp).

// NOTE: linsolve_reg migrated to toolboxes/linalg (solvers.cpp).
// NOTE: pageinv_reg  migrated to toolboxes/linalg (page_ops.cpp).

// NOTE: trace_reg / det_reg migrated to toolboxes/linalg (properties.cpp).
// NOTE: chol_reg / lu_reg / qr_reg / svd_reg migrated to
//       toolboxes/linalg (decompositions.cpp).
// NOTE: rank_reg / cond_reg / normest_reg migrated to
//       toolboxes/linalg (properties.cpp).
// NOTE: pinv_reg / orth_reg / null_reg migrated to
//       toolboxes/linalg (pseudo_subspace.cpp).
// NOTE: eig_reg / hess_reg / schur_reg / sylvester_reg /
//       expm_reg / logm_reg / sqrtm_reg migrated to toolboxes/linalg
//       (eig.cpp, matrix_functions.cpp). Block below disabled.

void topkrows_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("topkrows: requires (A, k[, col[, direction]])",
                    0, 0, "topkrows", "", "numkit:topkrows:nargin");
    const double kd = args[1].toScalar();
    if (kd < 0.0 || kd != std::floor(kd))
        throw Error("topkrows: k must be a non-negative integer",
                    0, 0, "topkrows", "", "numkit:topkrows:badK");
    auto *mr = ctx.engine->resource();

    // Parse `col` (positive int / vector) and `direction` (string /
    // string vector). 'ComparisonMethod' NV pair (auto/real/abs) is
    // accept-and-ignore — numkit only supports real numeric input here.
    std::vector<std::size_t> cols;
    std::vector<std::uint8_t> desc;
    size_t i = 2;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()) {
        // Treat as col vector.
        const Value &c = args[i];
        cols.reserve(c.numel());
        for (std::size_t kk = 0; kk < c.numel(); ++kk) {
            const double v = c.elemAsDouble(kk);
            if (v < 1.0 || v != std::floor(v))
                throw Error("topkrows: col must be a positive integer "
                            "or vector of positive integers",
                            0, 0, "topkrows", "", "numkit:topkrows:badCol");
            cols.push_back(static_cast<std::size_t>(v) - 1);
        }
        ++i;
    }
    if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        const std::string s = args[i].toString();
        if (s == "ComparisonMethod") {
            // accept-and-ignore; consume value
            i += 2;
        } else {
            if (s == "descend")      desc.push_back(1);
            else if (s == "ascend")  desc.push_back(0);
            else
                throw Error("topkrows: direction must be 'ascend' or "
                            "'descend'",
                            0, 0, "topkrows", "", "numkit:topkrows:badDir");
            ++i;
        }
    }
    // Optional trailing 'ComparisonMethod' NV (after dir).
    while (i + 1 < args.size() && (args[i].isChar() || args[i].isString())) {
        const std::string nm = args[i].toString();
        if (nm == "ComparisonMethod") { i += 2; continue; }
        throw Error("topkrows: unexpected argument '" + nm + "'",
                    0, 0, "topkrows", "", "numkit:topkrows:badArg");
    }

    std::vector<std::size_t> idx_out;
    auto B = topkrows_full(args[0], static_cast<std::size_t>(kd),
                            cols, desc,
                            (nargout >= 2) ? &idx_out : nullptr, mr);
    outs[0] = std::move(B);
    if (nargout >= 2) {
        auto I = Value::matrix(idx_out.size(), 1, ValueType::DOUBLE, mr);
        double *id = I.doubleDataMut();
        for (std::size_t k2 = 0; k2 < idx_out.size(); ++k2)
            id[k2] = double(idx_out[k2] + 1);  // 1-indexed
        outs[1] = std::move(I);
    }
}

void size_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("Not enough input arguments",
                     0, 0, "size", "", "numkit:size:nargin");
    auto *mr = ctx.engine->resource();

    if (args.size() >= 2) {
        outs[0] = size(args[0], static_cast<int>(args[1].toScalar()), mr);
        return;
    }

    if (nargout > 1) {
        const auto &dims = args[0].dims();
        // Multi-output form: [r, c] = size(A) or [r, c, p, ...] = size(A).
        // For ND tensors, dims past nargout-1 are gathered into the last
        // requested output (MATLAB behaviour: extra-dim sizes multiplied
        // into the trailing slot). For dims past actual ndim, return 1.
        for (size_t i = 0; i < nargout && i < outs.size(); ++i) {
            double v;
            if (i + 1 < nargout) {
                v = static_cast<double>(dims.dim(static_cast<int>(i)));
            } else {
                // Last requested output: multiply remaining dims (if any).
                size_t prod = 1;
                for (int j = static_cast<int>(i); j < dims.ndim(); ++j)
                    prod *= dims.dim(j);
                if (dims.ndim() <= static_cast<int>(i)) prod = 1;
                v = static_cast<double>(prod);
            }
            outs[i] = Value::scalar(v, mr);
        }
        return;
    }

    outs[0] = size(args[0], mr);
}

void length_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("length: requires 1 argument",
                     0, 0, "length", "", "numkit:length:nargin");
    outs[0] = length(args[0], ctx.engine->resource());
}

void numel_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("numel: requires 1 argument",
                     0, 0, "numel", "", "numkit:numel:nargin");
    outs[0] = numel(args[0], ctx.engine->resource());
}

void ndims_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ndims: requires 1 argument",
                     0, 0, "ndims", "", "numkit:ndims:nargin");
    outs[0] = ndims(args[0], ctx.engine->resource());
}

void reshape_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("reshape: requires at least 2 arguments",
                     0, 0, "reshape", "", "numkit:reshape:nargin");

    const auto &x = args[0];
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    ScratchVec<size_t> dims(&scratch);

    // Dims-vector form: reshape(A, [m n p ...]). No [] inference here.
    if (args.size() == 2 && !args[1].isScalar() && !args[1].isEmpty()) {
        dims = parseDimsArgsND(&scratch, args.subspan(1));
    } else {
        // Scalar-args form: reshape(A, m, n, ...). One [] allowed for
        // dimension inference from x.numel().
        const size_t dimCount = args.size() - 1;
        dims.assign(dimCount, 1);
        int inferPos = -1;
        size_t knownProd = 1;
        for (size_t i = 0; i < dimCount; ++i) {
            if (args[i + 1].isEmpty()) {
                if (inferPos >= 0)
                    throw Error("reshape: only one dimension may be inferred via []",
                                 0, 0, "reshape", "", "numkit:reshape:tooManyInferred");
                inferPos = static_cast<int>(i);
            } else {
                dims[i] = static_cast<size_t>(args[i + 1].toScalar());
                knownProd *= dims[i];
            }
        }
        if (inferPos >= 0) {
            if (knownProd == 0 || x.numel() % knownProd != 0)
                throw Error("reshape: size of array must be divisible by product of known dims",
                             0, 0, "reshape", "", "numkit:reshape:indivisible");
            dims[inferPos] = x.numel() / knownProd;
        }
    }

    // Strip trailing 1s past the 2nd dim (MATLAB convention).
    stripTrailingOnes(dims);
    outs[0] = reshapeND(x, Span<const size_t>(dims.data(), dims.size()), mr);
}

void transpose_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("transpose: requires 1 argument",
                     0, 0, "transpose", "", "numkit:transpose:nargin");
    outs[0] = transpose(args[0], ctx.engine->resource());
}

void pagetranspose_reg(Span<const Value> args, size_t, Span<Value> outs,
                       CallContext &ctx)
{
    if (args.empty())
        throw Error("pagetranspose: requires 1 argument",
                     0, 0, "pagetranspose", "", "numkit:pagetranspose:nargin");
    outs[0] = pagetranspose(args[0], ctx.engine->resource());
}

void pagectranspose_reg(Span<const Value> args, size_t, Span<Value> outs,
                        CallContext &ctx)
{
    if (args.empty())
        throw Error("pagectranspose: requires 1 argument",
                     0, 0, "pagectranspose", "", "numkit:pagectranspose:nargin");
    outs[0] = pagectranspose(args[0], ctx.engine->resource());
}

void peaks_reg(Span<const Value> args, size_t, Span<Value> outs,
               CallContext &ctx)
{
    size_t n = 49;  // MATLAB default
    if (!args.empty()) {
        const double dn = args[0].toScalar();
        if (dn < 0 || dn > 1.0e9 || std::isnan(dn))
            throw Error("peaks: n must be a non-negative integer",
                         0, 0, "peaks", "", "numkit:peaks:badN");
        n = static_cast<size_t>(dn);
    }
    outs[0] = peaks(n, ctx.engine->resource());
}

void sphere_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    size_t n = 20;  // MATLAB default
    if (!args.empty()) n = static_cast<size_t>(args[0].toScalar());
    auto s = sphere(n, ctx.engine->resource());
    outs[0] = std::move(s.X);
    if (nargout > 1) outs[1] = std::move(s.Y);
    if (nargout > 2) outs[2] = std::move(s.Z);
}

void cylinder_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    size_t n = 20;
    Value R;
    if (args.empty()) {
        // Default profile [1 1].
        R = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        R.doubleDataMut()[0] = 1.0;
        R.doubleDataMut()[1] = 1.0;
    } else {
        if (args[0].isScalar()) {
            // cylinder(n) — single integer arg is `n`, R defaults to [1 1].
            n = static_cast<size_t>(args[0].toScalar());
            R = Value::matrix(1, 2, ValueType::DOUBLE, mr);
            R.doubleDataMut()[0] = 1.0;
            R.doubleDataMut()[1] = 1.0;
        } else {
            R = args[0];
            if (args.size() >= 2)
                n = static_cast<size_t>(args[1].toScalar());
        }
    }
    auto s = cylinder(R, n, mr);
    outs[0] = std::move(s.X);
    if (nargout > 1) outs[1] = std::move(s.Y);
    if (nargout > 2) outs[2] = std::move(s.Z);
}

void ellipsoid_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.size() < 6)
        throw Error("ellipsoid: requires (xc, yc, zc, xr, yr, zr [, n])",
                     0, 0, "ellipsoid", "", "numkit:ellipsoid:nargin");
    const double xc = args[0].toScalar();
    const double yc = args[1].toScalar();
    const double zc = args[2].toScalar();
    const double xr = args[3].toScalar();
    const double yr = args[4].toScalar();
    const double zr = args[5].toScalar();
    size_t n = 20;
    if (args.size() >= 7) n = static_cast<size_t>(args[6].toScalar());
    auto s = ellipsoid(xc, yc, zc, xr, yr, zr, n, ctx.engine->resource());
    outs[0] = std::move(s.X);
    if (nargout > 1) outs[1] = std::move(s.Y);
    if (nargout > 2) outs[2] = std::move(s.Z);
}

void pagemtimes_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto parseFlag = [](const Value &v) -> TranspOp {
        if (!v.isChar() && !v.isString())
            throw Error("pagemtimes: transpose flag must be a string",
                         0, 0, "pagemtimes", "", "numkit:pagemtimes:flagType");
        const std::string s = v.toString();
        if (s == "none")       return TranspOp::None;
        if (s == "transpose")  return TranspOp::Transpose;
        if (s == "ctranspose") return TranspOp::CTranspose;
        throw Error("pagemtimes: invalid transpose flag '" + s
                     + "' (expected 'none', 'transpose', or 'ctranspose')",
                     0, 0, "pagemtimes", "", "numkit:pagemtimes:invalidFlag");
    };
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 2) {
        outs[0] = pagemtimes(args[0], args[1], mr);
        return;
    }
    if (args.size() == 4) {
        outs[0] = pagemtimes(args[0], parseFlag(args[1]), args[2], parseFlag(args[3]), mr);
        return;
    }
    throw Error("pagemtimes: expected 2 or 4 arguments",
                 0, 0, "pagemtimes", "", "numkit:pagemtimes:nargin");
}

void diag_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("diag: requires 1 argument",
                     0, 0, "diag", "", "numkit:diag:nargin");
    long k = 0;
    if (args.size() >= 2)
        k = static_cast<long>(std::llround(args[1].toScalar()));
    outs[0] = diag(args[0], k, ctx.engine->resource());
}

void sort_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sort: requires 1 argument",
                     0, 0, "sort", "", "numkit:sort:nargin");
    // sort(X[, dim][, direction]): a numeric trailing arg is the dim, a
    // string is the direction ('ascend' default / 'descend'). MATLAB
    // accepts sort(X,direction) and sort(X,dim,direction).
    int dim = -1;
    bool descend = false;
    NanPlace nanPlace = NanPlace::Auto;
    auto lower = [](std::string s) {
        for (char &ch : s) if (ch >= 'A' && ch <= 'Z') ch = char(ch + 32);
        return s;
    };
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i].isEmpty()) continue;
        if (args[i].isChar() || args[i].isString()) {
            const std::string d = lower(args[i].toString());
            if (d == "descend") descend = true;
            else if (d == "ascend") descend = false;
            else if (d == "missingplacement" && i + 1 < args.size()) {
                const std::string v = lower(args[i + 1].toString());
                nanPlace = (v == "first") ? NanPlace::First
                         : (v == "last")  ? NanPlace::Last
                                          : NanPlace::Auto;
                ++i;  // consume the placement value
            }
            // ignore other Name-Value tokens (e.g. ComparisonMethod)
        } else {
            dim = static_cast<int>(args[i].toScalar());
        }
    }
    auto *mr = ctx.engine->resource();
    // Integer types: MATLAB keeps the class on the sorted VALUES (the index
    // output stays double). Sort through the double path (the order is
    // identical) then cast the sorted values back to the integer class.
    if (isIntegerType(args[0].type())) {
        const ValueType vt = args[0].type();
        Value xd = copyToDouble(args[0], mr);
        auto [sortedD, idxD] = sort(xd, dim, descend, nanPlace, mr);
        outs[0] = doubleToIntegerExact(sortedD, vt, mr);
        if (nargout > 1)
            outs[1] = std::move(idxD);
        return;
    }
    // Logical: MATLAB keeps the logical class on the sorted VALUES (the index
    // output stays double) — same shape as the integer path above. Sort via
    // the double path (order/indices identical) then narrow 0/1 -> LOGICAL.
    // (Only logical is handled here; sorting a CHAR array is a separate
    // still-unsupported case — see bugs/builtin/sort-logical.md "Related".)
    if (args[0].isLogical()) {
        Value xd = copyToDouble(args[0], mr);
        auto [sortedD, idxD] = sort(xd, dim, descend, nanPlace, mr);
        outs[0] = logicalizeCumResult(sortedD, mr);
        if (nargout > 1)
            outs[1] = std::move(idxD);
        return;
    }
    // Char: MATLAB sorts char by code point and PRESERVES the char class on the
    // VALUES (index stays double) — same shape as the integer/logical branches.
    // Gated on isChar(); string arrays sort through a different path and are
    // left untouched. bugs/builtin/sort-char.md.
    if (args[0].isChar()) {
        Value xd = copyToDouble(args[0], mr);
        auto [sortedD, idxD] = sort(xd, dim, descend, nanPlace, mr);
        outs[0] = charizeSortResult(sortedD, mr);
        if (nargout > 1)
            outs[1] = std::move(idxD);
        return;
    }
    auto [sorted, idx] = sort(args[0], dim, descend, nanPlace, mr);
    outs[0] = std::move(sorted);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

void sortrows_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sortrows: requires at least 1 argument",
                     0, 0, "sortrows", "", "numkit:sortrows:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto cols = ScratchVec<int>(&scratch);

    // 'ascend' (false) / 'descend' (true), case-insensitive.
    auto dirDescend = [](std::string s) -> bool {
        for (auto &ch : s)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (s == "ascend") return false;
        if (s == "descend") return true;
        throw Error("sortrows: direction must be 'ascend' or 'descend'",
                     0, 0, "sortrows", "", "numkit:sortrows:badDirection");
    };
    // CHAR/STRING scalar → one flag; CELL or multi-element STRING → one per
    // element (both store their elements as Values in cellDataVec()).
    auto collectDirs = [&](const Value &v, ScratchVec<uint8_t> &out) {
        if (v.type() == ValueType::CELL ||
            (v.type() == ValueType::STRING && v.numel() > 1)) {
            const auto &vec = v.cellDataVec();
            for (const auto &e : vec)
                out.push_back(dirDescend(e.toString()) ? 1u : 0u);
        } else {
            out.push_back(dirDescend(v.toString()) ? 1u : 0u);
        }
    };
    auto isDirArg = [](const Value &v) {
        return v.type() == ValueType::CHAR || v.type() == ValueType::STRING ||
               v.type() == ValueType::CELL;
    };

    if (args.size() >= 2 && !args[1].isEmpty()) {
        const auto &c = args[1];
        if (isDirArg(c)) {
            // sortrows(A, direction[s]) — direction(s) applied over ALL
            // columns. A single direction covers every column; a cell/string
            // array must supply exactly one direction per column.
            const size_t C = args[0].dims().cols();
            ScratchVec<uint8_t> dirs(&scratch);
            collectDirs(c, dirs);
            if (dirs.size() == 1) {
                const bool d = dirs[0] != 0;
                for (size_t k = 1; k <= C; ++k)
                    cols.push_back(d ? -static_cast<int>(k) : static_cast<int>(k));
            } else {
                if (dirs.size() != C)
                    throw Error("sortrows: number of directions must match the "
                                "number of columns",
                                 0, 0, "sortrows", "", "numkit:sortrows:dirCount");
                for (size_t k = 0; k < C; ++k)
                    cols.push_back(dirs[k] ? -static_cast<int>(k + 1)
                                           : static_cast<int>(k + 1));
            }
        } else {
            cols.reserve(c.numel());
            for (size_t i = 0; i < c.numel(); ++i) {
                const double v = c.elemAsDouble(i);
                if (v != std::floor(v))
                    throw Error("sortrows: column index must be an integer",
                                 0, 0, "sortrows", "", "numkit:sortrows:badCol");
                cols.push_back(static_cast<int>(v));
            }
            // Optional direction argument: sortrows(A, cols, direction[s]).
            // Re-signs the listed columns by direction (overriding any sign in
            // the numeric spec). A single direction covers all listed columns.
            if (args.size() >= 3 && !args[2].isEmpty()) {
                if (!isDirArg(args[2]))
                    throw Error("sortrows: direction must be 'ascend' or "
                                "'descend'",
                                 0, 0, "sortrows", "", "numkit:sortrows:badDirection");
                ScratchVec<uint8_t> dirs(&scratch);
                collectDirs(args[2], dirs);
                if (dirs.size() == 1) {
                    const bool d = dirs[0] != 0;
                    for (auto &cc : cols) {
                        const int a = cc < 0 ? -cc : cc;
                        cc = d ? -a : a;
                    }
                } else {
                    if (dirs.size() != cols.size())
                        throw Error("sortrows: number of directions must match "
                                    "the number of sort columns",
                                     0, 0, "sortrows", "", "numkit:sortrows:dirCount");
                    for (size_t i = 0; i < cols.size(); ++i) {
                        const int a = cols[i] < 0 ? -cols[i] : cols[i];
                        cols[i] = dirs[i] ? -a : a;
                    }
                }
            }
        }
    }
    auto [sorted, idx] = sortrows(args[0], Span<const int>(cols.data(), cols.size()), mr);
    outs[0] = std::move(sorted);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

void find_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("find: requires 1 argument",
                     0, 0, "find", "", "numkit:find:nargin");
    auto *mr = ctx.engine->resource();
    const Value &x = args[0];

    // Optional count K (args[1]) and direction (args[2]: 'first' default /
    // 'last'). MATLAB: K must be a positive scalar integer; K exceeding the
    // nonzero count returns all. 'last' returns the last K nonzeros, still in
    // ascending index order. Applies to BOTH the single-output (linear
    // indices) and the [r,c] / [r,c,v] subscript forms.
    bool haveK = args.size() >= 2 && !args[1].isEmpty();
    size_t kReq = 0;
    if (haveK) {
        if (args[1].numel() != 1)
            throw Error("find: Second argument must be a positive scalar integer.",
                         0, 0, "find", "", "numkit:find:badK");
        const double kd = args[1].toScalar();
        if (!std::isfinite(kd) || kd < 1.0 || kd != std::floor(kd))
            throw Error("find: Second argument must be a positive scalar integer.",
                         0, 0, "find", "", "numkit:find:badK");
        kReq = static_cast<size_t>(kd);
    }
    bool fromLast = false;
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (!(args[2].isChar() || args[2].isString()))
            throw Error("find: third argument must be 'first' or 'last'",
                         0, 0, "find", "", "numkit:find:badDir");
        std::string d = args[2].toString();
        for (char &ch : d)
            if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + 32);
        if (d == "last") fromLast = true;
        else if (d == "first") fromLast = false;
        else
            throw Error("find: third argument must be 'first' or 'last'",
                         0, 0, "find", "", "numkit:find:badDir");
    }

    ScratchArena scratch(mr);
    auto lin = ScratchVec<size_t>(&scratch);
    forEachNonzero(x, [&](size_t i) { lin.push_back(i); });

    // Window of [start, start+count) into the ascending nonzero-index list.
    size_t start = 0, count = lin.size();
    if (haveK && kReq < lin.size()) {
        count = kReq;
        start = fromLast ? lin.size() - kReq : 0;
    }

    const size_t R = x.dims().rows() == 0 ? 1 : x.dims().rows();
    const bool rowResult = !x.dims().is3D() && x.dims().rows() == 1;
    auto mk = [&](ValueType t) {
        return rowResult ? Value::matrix(1, count, t, mr)
                         : Value::matrix(count, 1, t, mr);
    };

    if (nargout <= 1) {
        Value r = mk(ValueType::DOUBLE);
        double *rd = r.doubleDataMut();
        for (size_t t = 0; t < count; ++t)
            rd[t] = static_cast<double>(lin[start + t] + 1);
        outs[0] = std::move(r);
        return;
    }

    // [r, c] = find(X) / [r, c, v] = find(X): row/column subscripts (and the
    // nonzero values). Subscripts/values inherit X's vector orientation (row
    // vector → row results, otherwise column results), matching MATLAB.
    Value rowV = mk(ValueType::DOUBLE);
    Value colV = mk(ValueType::DOUBLE);
    double *rd = rowV.doubleDataMut();
    double *cd = colV.doubleDataMut();
    for (size_t t = 0; t < count; ++t) {
        const size_t i = lin[start + t];
        rd[t] = static_cast<double>(i % R + 1);
        cd[t] = static_cast<double>(i / R + 1);
    }
    outs[0] = std::move(rowV);
    outs[1] = std::move(colV);

    if (nargout >= 3) {
        if (x.type() == ValueType::COMPLEX) {
            Value valV = mk(ValueType::COMPLEX);
            const Complex *src = x.complexData();
            Complex *vd = valV.complexDataMut();
            for (size_t t = 0; t < count; ++t) vd[t] = src[lin[start + t]];
            outs[2] = std::move(valV);
        } else {
            Value valV = mk(ValueType::DOUBLE);
            double *vd = valV.doubleDataMut();
            for (size_t t = 0; t < count; ++t) vd[t] = x.elemAsDouble(lin[start + t]);
            outs[2] = std::move(valV);
        }
    }
}

void nnz_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nnz: requires 1 argument",
                     0, 0, "nnz", "", "numkit:nnz:nargin");
    outs[0] = nnz(args[0], ctx.engine->resource());
}

void nonzeros_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nonzeros: requires 1 argument",
                     0, 0, "nonzeros", "", "numkit:nonzeros:nargin");
    outs[0] = nonzeros(args[0], ctx.engine->resource());
}

void horzcat_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = horzcat(args, ctx.engine->resource());
}

void vertcat_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = vertcat(args, ctx.engine->resource());
}

void meshgrid_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("meshgrid: requires at least 1 argument",
                     0, 0, "meshgrid", "", "numkit:meshgrid:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        // meshgrid(x) ≡ meshgrid(x, x). See BUGS.md #21.
        auto [X, Y] = meshgrid(args[0], args[0], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        return;
    }
    if (args.size() == 2) {
        auto [X, Y] = meshgrid(args[0], args[1], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        return;
    }
    if (args.size() == 3) {
        auto [X, Y, Z] = meshgrid(args[0], args[1], args[2], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        if (nargout > 2) outs[2] = std::move(Z);
        return;
    }
    throw Error("meshgrid: 4+ inputs are not supported",
                 0, 0, "meshgrid", "", "numkit:meshgrid:tooManyInputs");
}

void ndgrid_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ndgrid: requires at least 2 arguments",
                     0, 0, "ndgrid", "", "numkit:ndgrid:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 2) {
        auto [X, Y] = ndgrid(args[0], args[1], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        return;
    }
    if (args.size() == 3) {
        auto [X, Y, Z] = ndgrid(args[0], args[1], args[2], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        if (nargout > 2) outs[2] = std::move(Z);
        return;
    }
    throw Error("ndgrid: 4+ inputs are not yet supported",
                 0, 0, "ndgrid", "", "numkit:ndgrid:tooManyInputs");
}

// NOTE: kron_reg migrated to toolboxes/linalg/src/vector_ops.cpp.

// cumsum_reg / cumprod_reg are defined below (after the cum-flag helpers),
// where flip() is in scope — they parse the 'reverse'/'forward' direction
// and 'omitnan'/'includenan' flags like MATLAB.

// MATLAB cummax / cummin accept positional 'reverse' / 'omitnan' /
// 'includenan' string flags after the optional dim. Trick: 'reverse'
// = flip + cum + flip; 'includenan' propagation requires a second pass
// that fills NaN forward from the first NaN onwards (since the cum*
// kernel itself already skips NaN per omitnan default).
namespace {

void parseCumDirNan(Span<const Value> args, size_t start,
                    int &dim, bool &reverse, bool &include_nan)
{
    dim = 0;
    reverse = false;
    include_nan = false;        // matches numkit default = MATLAB default
    size_t i = start;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()
        && !args[i].isEmpty()) {
        dim = static_cast<int>(args[i].toScalar()); ++i;
    }
    while (i < args.size()) {
        if (!(args[i].isChar() || args[i].isString())) {
            throw Error("cummax/cummin: trailing positional must be a string flag",
                        0, 0, "cummax/cummin", "", "numkit:cum:badArg");
        }
        std::string s = args[i].toString();
        for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (s == "reverse")    reverse = true;
        else if (s == "forward")    reverse = false;
        else if (s == "omitnan")    include_nan = false;
        else if (s == "includenan") include_nan = true;
        else
            throw Error("cummax/cummin: unknown flag '" + s + "'",
                        0, 0, "cummax/cummin", "", "numkit:cum:flag");
        ++i;
    }
}

// Propagate NaN forward in `out` based on the NaN positions in the
// (already same-shape) `src` input. Used to implement 'includenan'
// for cummax/cummin: once a NaN is hit in src along the operating
// dim, every subsequent output entry is set to NaN.
void propagateNanFromSrc(Value &out, const Value &src, int dim1Based)
{
    const auto &dd = out.dims();
    const int nd = dd.ndim();
    const int d = dim1Based;
    if (d < 1 || d > nd) return;
    size_t inner = 1;
    for (int i = 0; i < d - 1; ++i) inner *= dd.dim(i);
    size_t outer = 1;
    for (int i = d; i < nd; ++i) outer *= dd.dim(i);
    const size_t L = dd.dim(d - 1);
    double *o = out.doubleDataMut();
    const double *s = src.doubleData();
    for (size_t oc = 0; oc < outer; ++oc)
        for (size_t b = 0; b < inner; ++b) {
            const size_t base = oc * inner * L + b;
            bool seenNaN = false;
            for (size_t k = 0; k < L; ++k) {
                if (!seenNaN && std::isnan(s[base + k * inner]))
                    seenNaN = true;
                if (seenNaN)
                    o[base + k * inner] = std::numeric_limits<double>::quiet_NaN();
            }
        }
}

template <typename Fn>
Value runCumWithFlags(const Value &x, Span<const Value> args, Fn impl, std::pmr::memory_resource *mr)
{
    int dim; bool reverse; bool include_nan;
    parseCumDirNan(args, 1, dim, reverse, include_nan);
    Value src = x;
    if (reverse) src = flip(src, dim, mr);
    Value out = (dim > 0) ? impl(src, dim, mr) : impl(src, 0, mr);
    if (include_nan) {
        // Determine effective dim (firstNonSingleton when dim=0).
        int effDim = dim;
        if (effDim <= 0) {
            const auto &dd = out.dims();
            effDim = 1;
            for (int k = 0; k < dd.ndim(); ++k)
                if (dd.dim(k) > 1) { effDim = k + 1; break; }
        }
        propagateNanFromSrc(out, src, effDim);
    }
    if (reverse) {
        int effDim = dim;
        if (effDim <= 0) {
            const auto &dd = out.dims();
            effDim = 1;
            for (int k = 0; k < dd.ndim(); ++k)
                if (dd.dim(k) > 1) { effDim = k + 1; break; }
        }
        out = flip(out, effDim, mr);
    }
    return out;
}

} // anonymous

void cummax_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cummax: requires at least 1 argument",
                     0, 0, "cummax", "", "numkit:cummax:nargin");
    outs[0] = runCumWithFlags(args[0], args, [](const Value &v, int d, std::pmr::memory_resource *mr) {
                                  return cummax(v, d, mr);
                              }, ctx.engine->resource());
}

void cummin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cummin: requires at least 1 argument",
                     0, 0, "cummin", "", "numkit:cummin:nargin");
    outs[0] = runCumWithFlags(args[0], args, [](const Value &v, int d, std::pmr::memory_resource *mr) {
                                  return cummin(v, d, mr);
                              }, ctx.engine->resource());
}

namespace {

// cumsum/cumprod option handling. Unlike cummax/cummin (whose kernel skips
// NaN), the cumsum/cumprod kernels PROPAGATE NaN — which is MATLAB's
// 'includenan' default. So 'omitnan' is implemented by replacing NaN with
// the additive/multiplicative identity (0 / 1) BEFORE the scan. 'reverse'
// = flip → scan → flip along the operating dimension.
Value cumScanFlags(const Value &x, Span<const Value> args, bool isProd,
                   std::pmr::memory_resource *mr)
{
    int dim = 0; bool reverse = false; bool omitnan = false;
    size_t i = 1;
    if (i < args.size() && !args[i].isEmpty()
        && !args[i].isChar() && !args[i].isString()) {
        dim = static_cast<int>(args[i].toScalar()); ++i;
    }
    for (; i < args.size(); ++i) {
        if (!(args[i].isChar() || args[i].isString())) continue;
        std::string s = args[i].toString();
        for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (s == "reverse")    reverse = true;
        else if (s == "forward")    reverse = false;
        else if (s == "omitnan")    omitnan = true;
        else if (s == "includenan") omitnan = false;
    }
    // Effective (1-based) dim: first non-singleton when unspecified.
    int effDim = dim;
    if (effDim <= 0) {
        const auto &dd = x.dims();
        effDim = 1;
        for (int k = 0; k < dd.ndim(); ++k)
            if (dd.dim(k) > 1) { effDim = k + 1; break; }
    }
    Value src = x;
    if (omitnan && src.type() == ValueType::DOUBLE) {
        const double id = isProd ? 1.0 : 0.0;
        double *d = src.doubleDataMut();
        const size_t n = src.numel();
        for (size_t k = 0; k < n; ++k)
            if (std::isnan(d[k])) d[k] = id;
    }
    if (reverse) src = flip(src, effDim, mr);
    Value out = isProd ? cumprod(src, effDim, mr) : cumsum(src, effDim, mr);
    if (reverse) out = flip(out, effDim, mr);
    return out;
}

} // anonymous

void cumsum_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cumsum: requires at least 1 argument",
                     0, 0, "cumsum", "", "numkit:cumsum:nargin");
    outs[0] = cumScanFlags(args[0], args, /*isProd=*/false, ctx.engine->resource());
}

void cumprod_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cumprod: requires at least 1 argument",
                     0, 0, "cumprod", "", "numkit:cumprod:nargin");
    outs[0] = cumScanFlags(args[0], args, /*isProd=*/true, ctx.engine->resource());
}

void diff_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("diff: requires at least 1 argument",
                     0, 0, "diff", "", "numkit:diff:nargin");
    int n = 1;
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        // MATLAB: the difference order N must be a positive integer scalar
        // (a zero, negative, fractional or non-scalar order is an error).
        if (args[1].numel() != 1)
            throw Error("diff: Difference order N must be a positive integer scalar",
                         0, 0, "diff", "", "numkit:diff:badOrder");
        const double nv = args[1].toScalar();
        if (!std::isfinite(nv) || nv != std::floor(nv) || nv < 1.0)
            throw Error("diff: Difference order N must be a positive integer scalar",
                         0, 0, "diff", "", "numkit:diff:badOrder");
        n = static_cast<int>(nv);
    }
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = diff(args[0], n, dim, ctx.engine->resource());
}

#undef NK_CUM_REG

#define NK_LOGICAL_RED_REG(name, fn)                                           \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,               \
                    Span<Value> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.empty())                                                      \
            throw Error(#name ": requires at least 1 argument",               \
                         0, 0, #name, "", "numkit:" #name ":nargin");               \
        int dim = 0;                                                           \
        if (args.size() >= 2 && !args[1].isEmpty())                            \
            dim = static_cast<int>(args[1].toScalar());                        \
        outs[0] = fn(args[0], dim, ctx.engine->resource());                   \
    }

NK_LOGICAL_RED_REG(any, numkit::math::anyOf)
NK_LOGICAL_RED_REG(all, numkit::math::allOf)

#undef NK_LOGICAL_RED_REG

void xor_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("xor: requires 2 arguments",
                     0, 0, "xor", "", "numkit:xor:nargin");
    outs[0] = xorOf(args[0], args[1], ctx.engine->resource());
}

// NOTE: cross_reg / dot_reg migrated to toolboxes/linalg/src/vector_ops.cpp.

} // namespace detail

} // namespace numkit::builtin
