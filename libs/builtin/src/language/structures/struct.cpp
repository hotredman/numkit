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
            // Per-element assignment from cell — use setField (preserves order)
            for (size_t k = 0; k < numel; ++k)
                s.setField(k, fname, v.cellAt(k));
        } else {
            s.setFieldAll(fname, v);
        }
    }
    return s;
}

Value fieldnames(std::pmr::memory_resource *mr, const Value &s)
{
    if (!s.isStruct())
        throw Error("fieldnames requires a struct", 0, 0, "fieldnames", "",
                     "m:fieldnames:notStruct");
    // BUG #15 fix: iterate insertion order (fieldNamesInOrder) instead
    // of std::map alphabetical iteration. fieldNamesInOrder() falls back
    // to map iteration for legacy/cloned structs missing fieldOrder.
    const auto names = s.fieldNamesInOrder();
    auto c = Value::cell(names.size(), names.empty() ? 0 : 1, mr);
    for (size_t i = 0; i < names.size(); ++i)
        c.cellAt(i) = Value::fromString(names[i], mr);
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
    out.removeField(name.toString());  // BUG #15: also clears fieldOrder
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
// Pack 14: getfield / setfield / orderfields / struct2cell / cell2struct
// ════════════════════════════════════════════════════════════════════════

Value getfield(std::pmr::memory_resource *, const Value &s, const Value &name)
{
    if (!s.isStruct())
        throw Error("getfield requires a struct", 0, 0, "getfield", "",
                     "m:getfield:notStruct");
    const std::string n = name.toString();
    if (s.isStructArray()) {
        if (s.numel() == 0)
            throw Error("getfield: struct array is empty", 0, 0, "getfield", "",
                         "m:getfield:emptyArray");
        const auto &elem0 = s.structArrayElem(0);
        auto it = elem0.find(n);
        if (it == elem0.end())
            throw Error("getfield: no such field '" + n + "'",
                         0, 0, "getfield", "", "m:getfield:noField");
        return it->second;
    }
    if (!s.hasField(n))
        throw Error("getfield: no such field '" + n + "'",
                     0, 0, "getfield", "", "m:getfield:noField");
    return s.field(n);
}

Value setfield(std::pmr::memory_resource *mr, const Value &s,
               const Value &name, const Value &value)
{
    Value out;
    if (s.isStruct()) {
        out = s;
    } else if (s.isEmpty()) {
        out = Value::structure(mr);
    } else {
        throw Error("setfield: first argument must be a struct or []",
                     0, 0, "setfield", "", "m:setfield:notStruct");
    }
    if (out.isStructArray()) {
        out.setFieldAll(name.toString(), value);  // BUG #15
    } else {
        out.field(name.toString()) = value;       // tracks order via field()
    }
    return out;
}

Value orderfields(std::pmr::memory_resource *mr, const Value &s)
{
    if (!s.isStruct())
        throw Error("orderfields requires a struct", 0, 0, "orderfields", "",
                     "m:orderfields:notStruct");
    // BUG #15 follow-up: orderfields explicitly sorts alphabetically
    // (MATLAB documented behaviour). Now that fieldOrder defaults to
    // insertion order, build a copy with the order tracker re-sorted.
    Value out = s;
    if (out.isStructArray() ? out.numel() > 0 : true) {
        // Use map iteration (alphabetical) to seed a fresh tracker.
        std::vector<std::string> sorted;
        if (out.isStructArray()) {
            for (const auto &[k, _] : out.structArrayElem(0))
                sorted.push_back(std::string(k));
        } else {
            for (const auto &[k, _] : out.structFields())
                sorted.push_back(std::string(k));
        }
        // Build a fresh struct preserving values but in alphabetical order.
        const auto rows = out.dims().rows();
        const auto cols = out.dims().cols();
        Value re = Value::structArray(rows, cols, mr);
        for (const auto &name : sorted) {
            for (size_t i = 0; i < re.numel(); ++i) {
                const auto &srcMap = out.structArrayElem(i);
                auto it = srcMap.find(name);
                if (it != srcMap.end()) re.setField(i, name, it->second);
            }
        }
        return re;
    }
    return out;
}

Value struct2cell(std::pmr::memory_resource *mr, const Value &s)
{
    if (!s.isStruct())
        throw Error("struct2cell requires a struct", 0, 0, "struct2cell", "",
                     "m:struct2cell:notStruct");
    if (s.isStructArray())
        throw Error("struct2cell: struct-array inputs not yet supported",
                     0, 0, "struct2cell", "", "m:struct2cell:array");
    const auto &fields = s.structFields();
    auto c = Value::cell(fields.size(), 1, mr);
    size_t i = 0;
    for (const auto &[k, v] : fields)
        c.cellAt(i++) = v;
    return c;
}

Value cell2struct(std::pmr::memory_resource *mr, const Value &c, const Value &fields)
{
    if (!c.isCell())
        throw Error("cell2struct: first argument must be a cell array",
                     0, 0, "cell2struct", "", "m:cell2struct:notCell");
    if (!fields.isCell() && !fields.isString())
        throw Error("cell2struct: fields must be a cell of strings",
                     0, 0, "cell2struct", "", "m:cell2struct:fieldsType");
    const size_t nFields = fields.numel();
    if (c.numel() != nFields)
        throw Error("cell2struct: cell size must equal number of fields",
                     0, 0, "cell2struct", "", "m:cell2struct:shape");
    auto out = Value::structure(mr);
    for (size_t i = 0; i < nFields; ++i) {
        const std::string name = fields.cellAt(i).toString();
        out.field(name) = c.cellAt(i);
    }
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

void getfield_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("getfield requires (S, name)",
                     0, 0, "getfield", "", "m:getfield:nargin");
    outs[0] = getfield(ctx.engine->resource(), args[0], args[1]);
}

void setfield_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("setfield requires (S, name, value)",
                     0, 0, "setfield", "", "m:setfield:nargin");
    outs[0] = setfield(ctx.engine->resource(), args[0], args[1], args[2]);
}

void orderfields_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("orderfields requires 1 argument",
                     0, 0, "orderfields", "", "m:orderfields:nargin");
    outs[0] = orderfields(ctx.engine->resource(), args[0]);
}

void struct2cell_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("struct2cell requires 1 argument",
                     0, 0, "struct2cell", "", "m:struct2cell:nargin");
    outs[0] = struct2cell(ctx.engine->resource(), args[0]);
}

void cell2struct_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cell2struct requires (C, fields)",
                     0, 0, "cell2struct", "", "m:cell2struct:nargin");
    outs[0] = cell2struct(ctx.engine->resource(), args[0], args[1]);
}

} // namespace detail

} // namespace numkit::builtin
