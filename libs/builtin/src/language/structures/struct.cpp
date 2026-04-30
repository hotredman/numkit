// libs/builtin/src/datatypes/struct/struct.cpp
//
// Struct construction (struct, fieldnames, isfield, rmfield) + structfun.
// Shares function-handle dispatch helpers with cell.cpp via the inline
// header below.

#include <numkit/builtin/language/structures/struct.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "language/handles/_handlefn_helpers.hpp"

namespace numkit::builtin {

namespace hf = ::numkit::builtin::detail::handlefn;

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

Value structure(std::pmr::memory_resource *)
{
    return Value::structure();
}

Value structure(std::pmr::memory_resource *mr, Span<const Value> nameValuePairs)
{
    // First pass: validate names and determine array shape. If any value
    // is a CELL, struct() returns a struct array of that cell's shape;
    // every other CELL value must have the same shape. Non-cell values
    // are broadcast to every element.
    size_t arrRows = 1;
    size_t arrCols = 1;
    bool isArray = false;
    for (size_t i = 0; i + 1 < nameValuePairs.size(); i += 2) {
        const Value &name = nameValuePairs[i];
        if (!name.isChar() && !name.isString())
            throw Error("struct: field names must be char arrays", 0, 0, "struct", "",
                         "m:struct:invalidFieldName");
        const Value &v = nameValuePairs[i + 1];
        if (v.isCell()) {
            if (!isArray) {
                arrRows = v.dims().rows();
                arrCols = v.dims().cols();
                isArray = true;
            } else if (arrRows != v.dims().rows() || arrCols != v.dims().cols()) {
                throw Error("struct: cell-array values must all have the same shape",
                             0, 0, "struct", "", "m:struct:cellShape");
            }
        }
    }

    if (!isArray) {
        // Scalar struct path — preserve original behaviour.
        auto s = Value::structure(mr);
        for (size_t i = 0; i + 1 < nameValuePairs.size(); i += 2)
            s.field(nameValuePairs[i].toString()) = nameValuePairs[i + 1];
        return s;
    }

    // Struct array.
    auto s = Value::structArray(arrRows, arrCols, mr);
    const size_t numel = arrRows * arrCols;
    for (size_t i = 0; i + 1 < nameValuePairs.size(); i += 2) {
        std::string fname = nameValuePairs[i].toString();
        const Value &v = nameValuePairs[i + 1];
        if (v.isCell()) {
            for (size_t k = 0; k < numel; ++k)
                s.structArrayElem(k)[fname] = v.cellAt(k);
        } else {
            for (size_t k = 0; k < numel; ++k)
                s.structArrayElem(k)[fname] = v;
        }
    }
    return s;
}

Value fieldnames(std::pmr::memory_resource *mr, const Value &s)
{
    if (!s.isStruct())
        throw Error("fieldnames requires a struct", 0, 0, "fieldnames", "",
                     "m:fieldnames:notStruct");
    // Struct array: read field names from element 0 (struct arrays carry
    // a uniform field set in MATLAB; we mirror that convention).
    if (s.isStructArray()) {
        const auto &elem0 = s.numel() > 0 ? s.structArrayElem(0)
                                          : std::pmr::map<std::string, Value>{mr};
        auto c = Value::cell(elem0.size(), 1, mr);
        size_t i = 0;
        for (const auto &[k, _] : elem0)
            c.cellAt(i++) = Value::fromString(k, mr);
        return c;
    }
    const auto &fields = s.structFields();
    auto c = Value::cell(fields.size(), 1, mr);
    size_t i = 0;
    for (const auto &[k, v] : fields)
        c.cellAt(i++) = Value::fromString(k, mr);
    return c;
}

Value isfield(std::pmr::memory_resource *mr, const Value &s, const Value &name)
{
    if (!s.isStruct())
        return Value::logicalScalar(false, mr);
    if (s.isStructArray()) {
        const std::string n = name.toString();
        if (s.numel() == 0)
            return Value::logicalScalar(false, mr);
        const auto &elem0 = s.structArrayElem(0);
        return Value::logicalScalar(elem0.count(n) > 0, mr);
    }
    return Value::logicalScalar(s.hasField(name.toString()), mr);
}

Value rmfield(std::pmr::memory_resource *, const Value &s, const Value &name)
{
    if (!s.isStruct())
        throw Error("rmfield requires a struct", 0, 0, "rmfield", "",
                     "m:rmfield:notStruct");
    Value out = s;
    if (out.isStructArray()) {
        const std::string n = name.toString();
        for (size_t i = 0; i < out.numel(); ++i)
            out.structArrayElem(i).erase(n);
    } else {
        out.structFields().erase(name.toString());
    }
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
    ScratchArena scratch(mr);
    ScratchVec<Value> results(&scratch);
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
