// libs/builtin/src/datatypes/struct/struct.cpp
//
// Struct construction (struct, fieldnames, isfield, rmfield) + structfun.
// Shares function-handle dispatch helpers with cell.cpp via the inline
// header below.

#include <numkit/builtin/datatypes/struct/struct.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch_arena.hpp>
#include <numkit/core/types.hpp>

#include "../_handlefn_helpers.hpp"

namespace numkit::builtin {

namespace hf = ::numkit::builtin::detail::handlefn;

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

Value structure(std::pmr::memory_resource *)
{
    return Value::structure();
}

Value structure(std::pmr::memory_resource *, Span<const Value> nameValuePairs)
{
    auto s = Value::structure();
    for (size_t i = 0; i + 1 < nameValuePairs.size(); i += 2) {
        if (!nameValuePairs[i].isChar() && !nameValuePairs[i].isString())
            throw Error("struct: field names must be char arrays", 0, 0, "struct", "",
                         "m:struct:invalidFieldName");
        s.field(nameValuePairs[i].toString()) = nameValuePairs[i + 1];
    }
    return s;
}

Value fieldnames(std::pmr::memory_resource *mr, const Value &s)
{
    if (!s.isStruct())
        throw Error("fieldnames requires a struct", 0, 0, "fieldnames", "",
                     "m:fieldnames:notStruct");
    const auto &fields = s.structFields();
    auto c = Value::cell(fields.size(), 1);
    size_t i = 0;
    for (const auto &[k, v] : fields)
        c.cellAt(i++) = Value::fromString(k, mr);
    return c;
}

Value isfield(std::pmr::memory_resource *mr, const Value &s, const Value &name)
{
    if (!s.isStruct())
        return Value::logicalScalar(false, mr);
    return Value::logicalScalar(s.hasField(name.toString()), mr);
}

Value rmfield(std::pmr::memory_resource *, const Value &s, const Value &name)
{
    if (!s.isStruct())
        throw Error("rmfield requires a struct", 0, 0, "rmfield", "",
                     "m:rmfield:notStruct");
    Value out = s;
    out.structFields().erase(name.toString());
    return out;
}

Value structfun(std::pmr::memory_resource *mr, const Value &fn, const Value &s,
                 bool uniformOutput, Engine *engine)
{
    if (!s.isStruct())
        throw Error("structfun: second argument must be a scalar struct",
                     0, 0, "structfun", "", "m:structfun:notStruct");
    hf::BuiltinFn f = hf::BuiltinFn::Numel;  // placeholder
    const bool isBuiltin = hf::tryParseBuiltinHandle(fn, f, "structfun");

    const auto &fields = s.structFields();
    const size_t n = fields.size();
    ScratchArena scratch_arena(mr);
    ScratchVec<Value> results(&scratch_arena);
    results.reserve(n);
    for (const auto &kv : fields)
        results.push_back(hf::applyHandle(mr, fn, f, isBuiltin,
                                          kv.second, engine, "structfun"));

    if (uniformOutput) {
        // Uniform: column vector of length n.
        if (isBuiltin && hf::builtinReturnsString(f))
            throw Error("structfun: @class output must use UniformOutput=false",
                         0, 0, "structfun", "", "m:structfun:nonUniform");
        // For built-in handles use the static return-type tag; for
        // anonymous handles infer from the first result.
        ValueType outT = ValueType::DOUBLE;
        if (isBuiltin && hf::builtinReturnsLogical(f))
            outT = ValueType::LOGICAL;
        else if (!isBuiltin && n > 0 && results[0].isLogical())
            outT = ValueType::LOGICAL;
        auto out = Value::matrix(n, 1, outT, mr);
        for (size_t i = 0; i < n; ++i) {
            const Value &v = results[i];
            if (!v.isScalar())
                throw Error("structfun: fn returned a non-scalar; pass 'UniformOutput', false",
                             0, 0, "structfun", "", "m:structfun:notScalar");
            if (outT == ValueType::LOGICAL)
                out.logicalDataMut()[i] = v.toBool() ? 1 : 0;
            else
                out.doubleDataMut()[i]  = v.toScalar();
        }
        return out;
    }

    // Cell output, column vector (1 entry per field).
    auto out = Value::cell(n, 1);
    for (size_t i = 0; i < n; ++i)
        out.cellAt(i) = std::move(results[i]);
    return out;
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void struct_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    outs[0] = structure(ctx.engine->resource(), args);
}

void fieldnames_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fieldnames: requires 1 argument", 0, 0, "fieldnames", "",
                     "m:fieldnames:nargin");
    outs[0] = fieldnames(ctx.engine->resource(), args[0]);
}

void isfield_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("isfield requires 2 arguments", 0, 0, "isfield", "",
                     "m:isfield:nargin");
    outs[0] = isfield(ctx.engine->resource(), args[0], args[1]);
}

void rmfield_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rmfield requires 2 arguments", 0, 0, "rmfield", "",
                     "m:rmfield:nargin");
    outs[0] = rmfield(ctx.engine->resource(), args[0], args[1]);
}

void structfun_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("structfun: requires at least 2 arguments (fn, S)",
                     0, 0, "structfun", "", "m:structfun:nargin");
    bool uniform = hf::parseUniformOutputFlag(args, 2, "structfun");
    outs[0] = structfun(ctx.engine->resource(), args[0], args[1], uniform, ctx.engine);
}

} // namespace detail

} // namespace numkit::builtin
