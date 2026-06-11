// toolboxes/builtin/src/language/types/types_reg.cpp
//
// CallContext register half (Phase 2b multi-block split).
#include <numkit/core/engine.hpp>
#include <numkit/lang/strings/strings.hpp>
#include <numkit/lang/types/types.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/ops/helpers.hpp>
#include "types/types_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::builtin {
using namespace numkit::lang;  // C4c localized (umbrella removed)

namespace detail {

// Numeric-constructor adapters need the zero-arg MATLAB form:
// double(), int32(), etc. → scalar zero of that type.
template <typename T, ValueType targetType>
void numericConstructor_reg(Span<const Value> args, size_t, Span<Value> outs,
                            CallContext &ctx)
{
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.empty()) {
        auto r = Value::matrix(1, 1, targetType, mr);
        *static_cast<T *>(r.rawDataMut()) = static_cast<T>(0);
        outs[0] = std::move(r);
        return;
    }
    outs[0] = numericConstructor<T>(targetType, args[0], mr);
}

void double_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<double, ValueType::DOUBLE>(args, n, outs, ctx); }

void single_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<float, ValueType::SINGLE>(args, n, outs, ctx); }

void int8_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<int8_t, ValueType::INT8>(args, n, outs, ctx); }
void int16_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<int16_t, ValueType::INT16>(args, n, outs, ctx); }
void int32_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<int32_t, ValueType::INT32>(args, n, outs, ctx); }
void int64_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<int64_t, ValueType::INT64>(args, n, outs, ctx); }
void uint8_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<uint8_t, ValueType::UINT8>(args, n, outs, ctx); }
void uint16_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<uint16_t, ValueType::UINT16>(args, n, outs, ctx); }
void uint32_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<uint32_t, ValueType::UINT32>(args, n, outs, ctx); }
void uint64_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<uint64_t, ValueType::UINT64>(args, n, outs, ctx); }

void logical_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("logical: requires 1 argument", 0, 0, "logical", "",
                     "numkit:logical:nargin");
    outs[0] = logical(args[0], ctx.engine->resource());
}

// ── Simple predicate adapters ────────────────────────────────────────────
#define NK_PRED_REG(FN)                                                             \
    void FN##_reg(Span<const Value> args, size_t, Span<Value> outs,               \
                  CallContext &ctx)                                                 \
    {                                                                               \
        if (args.empty())                                                           \
            throw Error(#FN ": requires 1 argument", 0, 0, #FN, "",                \
                         "numkit:" #FN ":nargin");                                  \
        outs[0] = FN(args[0], ctx.engine->resource());                             \
    }

NK_PRED_REG(isnumeric)
NK_PRED_REG(islogical)
NK_PRED_REG(ischar)
NK_PRED_REG(isstring)
NK_PRED_REG(iscell)
NK_PRED_REG(isstruct)
NK_PRED_REG(isempty)
NK_PRED_REG(isscalar)
NK_PRED_REG(isreal)
NK_PRED_REG(isinteger)
NK_PRED_REG(isfloat)
NK_PRED_REG(issingle)
NK_PRED_REG(issparse)
NK_PRED_REG(isnan)
NK_PRED_REG(isinf)
NK_PRED_REG(isfinite)
NK_PRED_REG(isvector)
NK_PRED_REG(isrow)
NK_PRED_REG(iscolumn)
NK_PRED_REG(ismatrix)
NK_PRED_REG(issortedrows)
NK_PRED_REG(isuniform)
NK_PRED_REG(anymissing)

#undef NK_PRED_REG

void ismissing_reg(Span<const Value> args, size_t, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.empty())
        throw Error("ismissing: requires at least 1 argument",
                    0, 0, "ismissing", "", "numkit:ismissing:nargin");
    const Value &ind = (args.size() >= 2) ? args[1] : Value::Empty;
    outs[0] = ismissing(args[0], ind, ctx.engine->resource());
}

void standardizeMissing_reg(Span<const Value> args, size_t, Span<Value> outs,
                            CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("standardizeMissing: requires (A, indicator)",
                    0, 0, "standardizeMissing", "",
                    "numkit:standardizeMissing:nargin");
    outs[0] = standardizeMissing(args[0], args[1], ctx.engine->resource());
}

void issorted_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("issorted: requires 1 argument", 0, 0, "issorted", "",
                     "numkit:issorted:nargin");
    const Value &mode = (args.size() >= 2) ? args[1] : Value::Empty;
    outs[0] = issorted(args[0], mode, ctx.engine->resource());
}

// flintmax/intmax/intmin/realmax/realmin all share an "optional type-name
// string" calling convention; one adapter per fn keeps error messages
// useful.
#define NK_LIMIT_REG(FN)                                                              \
    void FN##_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) \
    {                                                                                  \
        const Value &t = args.empty() ? Value::Empty : args[0];                       \
        outs[0] = FN(t, ctx.engine->resource());                                       \
    }

NK_LIMIT_REG(flintmax)
NK_LIMIT_REG(intmax)
NK_LIMIT_REG(intmin)
NK_LIMIT_REG(realmax)
NK_LIMIT_REG(realmin)

#undef NK_LIMIT_REG

void allfinite_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("allfinite: requires 1 argument", 0, 0, "allfinite", "",
                     "numkit:allfinite:nargin");
    outs[0] = allfinite(args[0], ctx.engine->resource());
}

void anynan_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("anynan: requires 1 argument", 0, 0, "anynan", "",
                     "numkit:anynan:nargin");
    outs[0] = anynan(args[0], ctx.engine->resource());
}

void isequal_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("isequal requires at least 2 arguments", 0, 0, "isequal", "",
                     "numkit:isequal:nargin");
    bool eq = true;
    for (size_t i = 1; i < args.size() && eq; ++i)
        eq = valuesEqual(args[0], args[i], false);
    outs[0] = Value::logicalScalar(eq, ctx.engine->resource());
}

void isequaln_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("isequaln requires at least 2 arguments", 0, 0, "isequaln", "",
                     "numkit:isequaln:nargin");
    bool eq = true;
    for (size_t i = 1; i < args.size() && eq; ++i)
        eq = valuesEqual(args[0], args[i], true);
    outs[0] = Value::logicalScalar(eq, ctx.engine->resource());
}

void class_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("class: requires 1 argument", 0, 0, "class", "",
                     "numkit:class:nargin");
    outs[0] = classOf(args[0], ctx.engine->resource());
}

// ── Pack 36 adapters ─────────────────────────────────────────────────
void cast_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cast: requires (x, classname) or (x, 'like', y)",
                     0, 0, "cast", "", "numkit:cast:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("cast: second arg must be a class name or 'like'",
                     0, 0, "cast", "", "numkit:cast:badClass");
    auto *mr = ctx.engine->resource();
    // 'like' form: cast(x, 'like', y) — pull class name from y.
    if (args[1].toString() == "like") {
        if (args.size() < 3)
            throw Error("cast: 'like' form requires (x, 'like', y)",
                         0, 0, "cast", "", "numkit:cast:nargin");
        // mtypeName mirrors MATLAB's class() output (double / single /
        // int*/ uint* / logical / char / string); cast() dispatches on
        // these strings.
        outs[0] = cast(args[0], mtypeName(args[2].type()), mr);
        return;
    }
    outs[0] = cast(args[0], args[1].toString(), mr);
}

void swapbytes_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("swapbytes: requires 1 argument",
                     0, 0, "swapbytes", "", "numkit:swapbytes:nargin");
    outs[0] = swapbytes(args[0], ctx.engine->resource());
}

void typecast_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("typecast: requires 2 arguments (x, classname)",
                     0, 0, "typecast", "", "numkit:typecast:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("typecast: classname must be a char or string",
                     0, 0, "typecast", "", "numkit:typecast:badClass");
    outs[0] = typecast(args[0], args[1].toString(), ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
