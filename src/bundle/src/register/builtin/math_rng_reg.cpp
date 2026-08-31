// toolboxes/signal/src/math/random/rng_reg.cpp
//
// CallContext register half of math/random/rng.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/datafun.hpp>
#include <numkit/ops/rng.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include <numkit/ops/helpers.hpp>
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
using namespace numkit::builtin;
using namespace numkit::ops;

namespace detail {

// Cast a DOUBLE Value buffer to single in-place (returns a new Value).
// Used by rand/randn when the user requests 'single'.
namespace { Value castDoubleToSingle(const Value &src, std::pmr::memory_resource *mr)
{
    Value dst;
    if (src.dims().is3D())
        dst = Value::matrix3d(src.dims().rows(), src.dims().cols(),
                              src.dims().pages(), ValueType::SINGLE, mr);
    else if (src.dims().ndim() > 3) {
        const auto &dimsRef = src.dims();
        size_t dimsBuf[Dims::kMaxRank];
        for (int i = 0; i < dimsRef.ndim(); ++i) dimsBuf[i] = dimsRef.dim(i);
        dst = Value::matrixND(dimsBuf, dimsRef.ndim(), ValueType::SINGLE, mr);
    } else
        dst = Value::matrix(src.dims().rows(), src.dims().cols(),
                            ValueType::SINGLE, mr);
    const size_t n = src.numel();
    const double *sp = src.doubleData();
    float *dp = dst.singleDataMut();
    for (size_t i = 0; i < n; ++i) dp[i] = static_cast<float>(sp[i]);
    return dst;
}}

// rand / randn supersede the earlier static-RNG versions. Same shape
// API (parseDimsArgs); the only change is they now share the engine
// that rng() controls.
void rand_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    if (t != ValueType::DOUBLE && t != ValueType::SINGLE)
        throw Error("rand: type must be 'double' or 'single'",
                    0, 0, "rand", "", "numkit:rand:badType");
    // Legacy state syntax (pre-`rng` MATLAB): rand('seed', n) /
    // rand('state', n) / rand('twister', n). Seeds the engine stream with
    // n. Sequence VALUES differ from MATLAB's legacy generators (numkit
    // has one MT19937 stream, `rng(n)` semantics) — but the call must
    // control the stream, not error (bugs/closed/stats/randn-legacy-seed-syntax.md).
    // Legacy QUERY: rand('seed') / randn('seed') -> the last seed (double).
    if (dimArgs.size() == 1 && (dimArgs[0].isChar() || dimArgs[0].isString())
        && dimArgs[0].toString() == "seed") {
        outs[0] = Value::scalar(static_cast<double>(ctx.engine->rng().legacySeed()),
                                ctx.engine->resource());
        return;
    }
    if (dimArgs.size() == 2 && (dimArgs[0].isChar() || dimArgs[0].isString())
        && dimArgs[1].isScalar() && !dimArgs[1].isChar()) {
        const std::string flag = dimArgs[0].toString();
        if (flag == "seed") {
            // True MATLAB v4 generator — bit-identical (Park-Miller +
            // polar; bugs/closed/stats/randn-legacy-seed-syntax.md).
            ctx.engine->rng().setLegacyV4(
                static_cast<std::uint64_t>(dimArgs[1].toScalar()));
            outs[0] = Value();
            return;
        }
        if (flag == "state" || flag == "twister") {
            // v5-MT / init_by-array seeding not yet replicated: seed the
            // modern stream (documented divergence, todo
            // partial_fix_followups) — the call must control, not error.
            ctx.engine->rng().seed(
                static_cast<std::uint64_t>(dimArgs[1].toScalar()));
            outs[0] = Value();
            return;
        }
    }
    ScratchArena scratch(mr);
    auto dims = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(dims);
    Value out;
    {
        if (dims.size() <= 3) {
            const size_t r = dims.size() >= 1 ? dims[0] : 1;
            const size_t c = dims.size() >= 2 ? dims[1] : 1;
            const size_t p = dims.size() >= 3 ? dims[2] : 0;
            out = rand(ctx.engine->rng(), r, c, p, mr);
        } else {
            out = randND(ctx.engine->rng(), Span<const size_t>(dims.data(), dims.size()), mr);
        }
    }
    outs[0] = (t == ValueType::SINGLE) ? castDoubleToSingle(out, mr) : std::move(out);
}

void randn_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    if (t != ValueType::DOUBLE && t != ValueType::SINGLE)
        throw Error("randn: type must be 'double' or 'single'",
                    0, 0, "randn", "", "numkit:randn:badType");
    // Legacy state syntax — same as rand (see the comment above).
    // Legacy QUERY: rand('seed') / randn('seed') -> the last seed (double).
    if (dimArgs.size() == 1 && (dimArgs[0].isChar() || dimArgs[0].isString())
        && dimArgs[0].toString() == "seed") {
        outs[0] = Value::scalar(static_cast<double>(ctx.engine->rng().legacySeed()),
                                ctx.engine->resource());
        return;
    }
    if (dimArgs.size() == 2 && (dimArgs[0].isChar() || dimArgs[0].isString())
        && dimArgs[1].isScalar() && !dimArgs[1].isChar()) {
        const std::string flag = dimArgs[0].toString();
        if (flag == "seed") {
            // True MATLAB v4 generator — bit-identical (Park-Miller +
            // polar; bugs/closed/stats/randn-legacy-seed-syntax.md).
            ctx.engine->rng().setLegacyV4(
                static_cast<std::uint64_t>(dimArgs[1].toScalar()));
            outs[0] = Value();
            return;
        }
        if (flag == "state" || flag == "twister") {
            // v5-MT / init_by-array seeding not yet replicated: seed the
            // modern stream (documented divergence, todo
            // partial_fix_followups) — the call must control, not error.
            ctx.engine->rng().seed(
                static_cast<std::uint64_t>(dimArgs[1].toScalar()));
            outs[0] = Value();
            return;
        }
    }
    ScratchArena scratch(mr);
    auto dims = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(dims);
    Value out;
    {
        if (dims.size() <= 3) {
            const size_t r = dims.size() >= 1 ? dims[0] : 1;
            const size_t c = dims.size() >= 2 ? dims[1] : 1;
            const size_t p = dims.size() >= 3 ? dims[2] : 0;
            out = randn(ctx.engine->rng(), r, c, p, mr);
        } else {
            out = randnND(ctx.engine->rng(), Span<const size_t>(dims.data(), dims.size()), mr);
        }
    }
    outs[0] = (t == ValueType::SINGLE) ? castDoubleToSingle(out, mr) : std::move(out);
}

// Cast a DOUBLE Value to any integer or single class (for randi typed
// outputs). Saturating cast (out-of-range int64 values clamp).
namespace { Value castDoubleToType(const Value &src, ValueType t, std::pmr::memory_resource *mr)
{
    if (t == ValueType::DOUBLE) return src;
    Value dst;
    if (src.dims().is3D())
        dst = Value::matrix3d(src.dims().rows(), src.dims().cols(),
                              src.dims().pages(), t, mr);
    else if (src.dims().ndim() > 3) {
        const auto &dimsRef = src.dims();
        size_t dimsBuf[Dims::kMaxRank];
        for (int i = 0; i < dimsRef.ndim(); ++i) dimsBuf[i] = dimsRef.dim(i);
        dst = Value::matrixND(dimsBuf, dimsRef.ndim(), t, mr);
    } else
        dst = Value::matrix(src.dims().rows(), src.dims().cols(), t, mr);
    const size_t n = src.numel();
    const double *sp = src.doubleData();
    auto cast_loop = [&](auto *dp, auto /*tag*/) {
        using T = std::remove_pointer_t<decltype(dp)>;
        for (size_t i = 0; i < n; ++i) dp[i] = static_cast<T>(sp[i]);
    };
    switch (t) {
      case ValueType::SINGLE: cast_loop(dst.singleDataMut(),  float{});    break;
      case ValueType::INT8:   cast_loop(dst.int8DataMut(),    int8_t{});   break;
      case ValueType::INT16:  cast_loop(dst.int16DataMut(),   int16_t{});  break;
      case ValueType::INT32:  cast_loop(dst.int32DataMut(),   int32_t{});  break;
      case ValueType::INT64:  cast_loop(dst.int64DataMut(),   int64_t{});  break;
      case ValueType::UINT8:  cast_loop(dst.uint8DataMut(),   uint8_t{});  break;
      case ValueType::UINT16: cast_loop(dst.uint16DataMut(),  uint16_t{}); break;
      case ValueType::UINT32: cast_loop(dst.uint32DataMut(),  uint32_t{}); break;
      case ValueType::UINT64: cast_loop(dst.uint64DataMut(),  uint64_t{}); break;
      default: throw Error("randi: unsupported type for cast",
                           0, 0, "randi", "", "numkit:randi:badType");
    }
    return dst;
}}

// randi MATLAB forms:
//   randi(imax)                    scalar
//   randi(imax, n)                 n×n
//   randi(imax, m, n[, p])         shape
//   randi(imax, [m n p])           shape via vector
//   randi([imin imax], …)          range form (first arg is 2-vector)
//   randi(..., 'type')             typed output (any int / 'double' / 'single')
void randi_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw Error("randi: requires at least 1 argument",
                     0, 0, "randi", "", "numkit:randi:nargin");

    int64_t imin = 1, imax = 0;
    const Value &first = args[0];
    if (!first.isScalar() && first.numel() == 2) {
        imin = static_cast<int64_t>(first.doubleData()[0]);
        imax = static_cast<int64_t>(first.doubleData()[1]);
    } else {
        imax = static_cast<int64_t>(first.toScalar());
    }

    Span<const Value> dimArgs = (args.size() > 1) ? args.subspan(1) : Span<const Value>{};
    auto *mr = ctx.engine->resource();

    // Strip trailing class-name from dim args.
    ValueType t;
    dimArgs = extractTypeArg(dimArgs, t);

    Value dbl_out;
    if (dimArgs.empty()) {
        // Scalar form.
        dbl_out = randi(ctx.engine->rng(), imin, imax, 1, 1, 0, mr);
    } else {
        ScratchArena scratch(mr);
        auto dims = parseDimsArgsND(&scratch, dimArgs);
        stripTrailingOnes(dims);
        if (dims.size() <= 3) {
            const size_t r = dims.size() >= 1 ? dims[0] : 1;
            const size_t c = dims.size() >= 2 ? dims[1] : 1;
            const size_t p = dims.size() >= 3 ? dims[2] : 0;
            dbl_out = randi(ctx.engine->rng(), imin, imax, r, c, p, mr);
        } else {
            // ND form: allocate matrixND and fill via the same uniform-int pass.
            auto m = Value::matrixND(dims.data(), static_cast<int>(dims.size()),
                                      ValueType::DOUBLE, mr);
            std::uniform_int_distribution<int64_t> dist(imin, imax);
            for (size_t i = 0; i < m.numel(); ++i)
                m.doubleDataMut()[i] = static_cast<double>(dist(ctx.engine->rng()));
            dbl_out = std::move(m);
        }
    }
    outs[0] = (t == ValueType::DOUBLE) ? std::move(dbl_out)
                                       : castDoubleToType(dbl_out, t, mr);
}

void randperm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("randperm: requires at least 1 argument",
                     0, 0, "randperm", "", "numkit:randperm:nargin");
    const size_t n = static_cast<size_t>(args[0].toScalar());
    if (args.size() == 1) {
        outs[0] = randperm(ctx.engine->rng(), n, ctx.engine->resource());
    } else {
        const size_t k = static_cast<size_t>(args[1].toScalar());
        outs[0] = randperm(ctx.engine->rng(), n, k, ctx.engine->resource());
    }
}

// rng MATLAB forms:
//   rng()              return current state struct (read-only snapshot)
//   rng(seed)          seed with integer
//   rng('default')     rng(0)
//   rng('shuffle')     seed from random_device
//   rng(state_struct)  restore previously-snapshotted state
// nargout > 0 : return the current state BEFORE seeding/restoring.
// (Matches MATLAB: `prev = rng(123)` snapshots the old state and seeds.)
void rng_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
             CallContext &ctx)
{
    auto *mr = ctx.engine->resource();

    // Always snapshot current state first if caller asked for it.
    Value prev;
    if (nargout > 0)
        prev = ctx.engine->rng().state(mr);

    if (args.empty()) {
        // rng() with no return value is a no-op; with a return it
        // gives the snapshot.
        if (nargout > 0) outs[0] = std::move(prev);
        return;
    }

    const Value &a = args[0];
    if (a.isStruct()) {
        ctx.engine->rng().restore(a);
    } else if (a.isChar() || a.isString()) {
        const auto s = a.toString();
        if (s == "default") ctx.engine->rng().seed(0);
        else if (s == "shuffle") ctx.engine->rng().shuffle();
        else
            throw Error("rng: string argument must be 'default' or 'shuffle'",
                         0, 0, "rng", "", "numkit:rng:badStringArg");
    } else if (a.isScalar() || a.numel() == 1) {
        const double sd = a.toScalar();
        if (sd < 0.0)
            throw Error("rng: seed must be a non-negative integer",
                         0, 0, "rng", "", "numkit:rng:badSeed");
        ctx.engine->rng().seed(static_cast<uint64_t>(sd));
    } else {
        throw Error("rng: argument must be a non-negative integer, "
                     "a struct from rng(), 'default', or 'shuffle'",
                     0, 0, "rng", "", "numkit:rng:badArg");
    }

    if (nargout > 0) outs[0] = std::move(prev);
}

} // namespace detail

} // namespace numkit::builtin
