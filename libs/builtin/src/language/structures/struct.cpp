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

Value structure(Span<const Value> nameValuePairs, std::pmr::memory_resource *mr)
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
                         "numkit:struct:invalidFieldName");
        const Value &v = nameValuePairs[i + 1];
        if (v.isCell()) {
            if (!isArray) {
                arrRows = v.dims().rows();
                arrCols = v.dims().cols();
                isArray = true;
            } else if (arrRows != v.dims().rows() || arrCols != v.dims().cols()) {
                throw Error("struct: cell-array values must all have the same shape",
                             0, 0, "struct", "", "numkit:struct:cellShape");
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

Value fieldnames(const Value &s, std::pmr::memory_resource *mr)
{
    if (!s.isStruct())
        throw Error("fieldnames requires a struct", 0, 0, "fieldnames", "",
                     "numkit:fieldnames:notStruct");
    // BUG #15 fix: iterate insertion order (fieldNamesInOrder) instead
    // of std::map alphabetical iteration. fieldNamesInOrder() falls back
    // to map iteration for legacy/cloned structs missing fieldOrder.
    const auto names = s.fieldNamesInOrder();
    auto c = Value::cell(names.size(), names.empty() ? 0 : 1, mr);
    for (size_t i = 0; i < names.size(); ++i)
        c.cellAt(i) = Value::fromString(names[i], mr);
    return c;
}

Value isfield(const Value &s, const Value &name, std::pmr::memory_resource *mr)
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

Value rmfield(const Value &s, const Value &name, std::pmr::memory_resource *)
{
    if (!s.isStruct())
        throw Error("rmfield requires a struct", 0, 0, "rmfield", "",
                     "numkit:rmfield:notStruct");
    Value out = s;
    out.removeField(name.toString());  // BUG #15: also clears fieldOrder
    return out;
}

Value structfun(FnHandle fn, const Value &s, bool uniformOutput,
                std::pmr::memory_resource *mr)
{
    if (!s.isStruct())
        throw Error("structfun: second argument must be a scalar struct",
                     0, 0, "structfun", "", "numkit:structfun:notStruct");

    const auto &fields = s.structFields();
    const size_t n = fields.size();
    ScratchArena scratch(mr);
    ScratchVec<Value> results(&scratch);
    results.reserve(n);
    for (const auto &kv : fields) {
        Value arg = kv.second;
        Value out;
        Span<const Value> ar(&arg, 1);
        Span<Value>       ou(&out, 1);
        fn(ar, ou, mr);
        results.push_back(std::move(out));
    }

    if (uniformOutput) {
        // Uniform: column vector of length n.
        // Output type follows the first result.
        const ValueType outT = (n > 0 && results[0].isLogical())
                                  ? ValueType::LOGICAL : ValueType::DOUBLE;
        auto out = Value::matrix(n, 1, outT, mr);
        for (size_t i = 0; i < n; ++i) {
            const Value &v = results[i];
            if (!v.isScalar())
                throw Error("structfun: fn returned a non-scalar; pass 'UniformOutput', false",
                             0, 0, "structfun", "", "numkit:structfun:notScalar");
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

Value getfield(const Value &s, const Value &name, std::pmr::memory_resource *)
{
    if (!s.isStruct())
        throw Error("getfield requires a struct", 0, 0, "getfield", "",
                     "numkit:getfield:notStruct");
    const std::string n = name.toString();
    if (s.isStructArray()) {
        if (s.numel() == 0)
            throw Error("getfield: struct array is empty", 0, 0, "getfield", "",
                         "numkit:getfield:emptyArray");
        const auto &elem0 = s.structArrayElem(0);
        auto it = elem0.find(n);
        if (it == elem0.end())
            throw Error("getfield: no such field '" + n + "'",
                         0, 0, "getfield", "", "numkit:getfield:noField");
        return it->second;
    }
    if (!s.hasField(n))
        throw Error("getfield: no such field '" + n + "'",
                     0, 0, "getfield", "", "numkit:getfield:noField");
    return s.field(n);
}

Value setfield(const Value &s, const Value &name, const Value &value, std::pmr::memory_resource *mr)
{
    Value out;
    if (s.isStruct()) {
        out = s;
    } else if (s.isEmpty()) {
        out = Value::structure(mr);
    } else {
        throw Error("setfield: first argument must be a struct or []",
                     0, 0, "setfield", "", "numkit:setfield:notStruct");
    }
    if (out.isStructArray()) {
        out.setFieldAll(name.toString(), value);  // BUG #15
    } else {
        out.field(name.toString()) = value;       // tracks order via field()
    }
    return out;
}

Value orderfields(const Value &s, std::pmr::memory_resource *mr)
{
    if (!s.isStruct())
        throw Error("orderfields requires a struct", 0, 0, "orderfields", "",
                     "numkit:orderfields:notStruct");
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

// orderfields(S, NAMES) — reorder S's fields to the order given by the
// cell array NAMES (MATLAB's 2-arg form). Provided names that exist are
// placed first in the given order; any existing field not listed is
// appended (never drop data). Used by the Variable Editor's rename to
// keep a renamed field in its original slot.
Value orderfieldsByNames(const Value &s, const Value &names,
                         std::pmr::memory_resource *mr)
{
    using numkit::ValueType;
    if (!s.isStruct())
        throw Error("orderfields requires a struct", 0, 0, "orderfields", "",
                     "numkit:orderfields:notStruct");
    // Empty struct array: no element 0 to read field names from; nothing
    // to reorder. Mirror the 1-arg form's "return unchanged" guard.
    if (s.isStructArray() && s.numel() == 0)
        return s;

    std::vector<std::string> order;
    if (names.isCell()) {
        for (size_t i = 0; i < names.numel(); ++i)
            order.push_back(names.cellAt(i).toString());
    } else if (names.type() == ValueType::CHAR) {
        order.push_back(names.toString());
    } else {
        throw Error("orderfields: second argument must be a cell array of names",
                     0, 0, "orderfields", "", "numkit:orderfields:badPerm");
    }

    // Existing field names (from element 0 / the single struct).
    std::vector<std::string> existing;
    if (s.isStructArray()) {
        for (const auto &[k, _] : s.structArrayElem(0)) existing.push_back(std::string(k));
    } else {
        for (const auto &[k, _] : s.structFields()) existing.push_back(std::string(k));
    }
    auto contains = [](const std::vector<std::string> &v, const std::string &x) {
        for (const auto &e : v) if (e == x) return true;
        return false;
    };

    // Final order: requested names that exist (deduped), then leftovers.
    std::vector<std::string> finalOrder;
    for (const auto &n : order)
        if (contains(existing, n) && !contains(finalOrder, n)) finalOrder.push_back(n);
    for (const auto &n : existing)
        if (!contains(finalOrder, n)) finalOrder.push_back(n);

    const auto rows = s.dims().rows();
    const auto cols = s.dims().cols();
    Value re = Value::structArray(rows, cols, mr);
    for (const auto &name : finalOrder)
        for (size_t i = 0; i < re.numel(); ++i) {
            const auto &srcMap = s.structArrayElem(i);
            auto it = srcMap.find(name);
            if (it != srcMap.end()) re.setField(i, name, it->second);
        }
    return re;
}

Value struct2cell(const Value &s, std::pmr::memory_resource *mr)
{
    if (!s.isStruct())
        throw Error("struct2cell requires a struct", 0, 0, "struct2cell", "",
                     "numkit:struct2cell:notStruct");
    if (s.isStructArray())
        throw Error("struct2cell: struct-array inputs not yet supported",
                     0, 0, "struct2cell", "", "numkit:struct2cell:array");
    const auto &fields = s.structFields();
    auto c = Value::cell(fields.size(), 1, mr);
    size_t i = 0;
    for (const auto &[k, v] : fields)
        c.cellAt(i++) = v;
    return c;
}

Value cell2struct(const Value &c, const Value &fields, std::pmr::memory_resource *mr)
{
    if (!c.isCell())
        throw Error("cell2struct: first argument must be a cell array",
                     0, 0, "cell2struct", "", "numkit:cell2struct:notCell");
    if (!fields.isCell() && !fields.isString())
        throw Error("cell2struct: fields must be a cell of strings",
                     0, 0, "cell2struct", "", "numkit:cell2struct:fieldsType");
    const size_t nFields = fields.numel();
    if (c.numel() != nFields)
        throw Error("cell2struct: cell size must equal number of fields",
                     0, 0, "cell2struct", "", "numkit:cell2struct:shape");
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
    outs[0] = structure(args, ctx.engine->resource());
}

void fieldnames_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fieldnames: requires 1 argument", 0, 0, "fieldnames", "",
                     "numkit:fieldnames:nargin");
    outs[0] = fieldnames(args[0], ctx.engine->resource());
}

void isfield_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("isfield requires 2 arguments", 0, 0, "isfield", "",
                     "numkit:isfield:nargin");
    outs[0] = isfield(args[0], args[1], ctx.engine->resource());
}

void rmfield_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rmfield requires 2 arguments", 0, 0, "rmfield", "",
                     "numkit:rmfield:nargin");
    outs[0] = rmfield(args[0], args[1], ctx.engine->resource());
}

void structfun_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("structfun: requires at least 2 arguments (fn, S)",
                     0, 0, "structfun", "", "numkit:structfun:nargin");
    bool uniform = hf::parseUniformOutputFlag(args, 2, "structfun");

    auto *mr = ctx.engine->resource();
    hf::BuiltinFn f = hf::BuiltinFn::Numel;
    const bool isBuiltin = hf::tryParseBuiltinHandle(args[0], f, "structfun");

    if (uniform && isBuiltin && hf::builtinReturnsString(f))
        throw Error("structfun: @class output must use UniformOutput=false",
                     0, 0, "structfun", "", "numkit:structfun:nonUniform");

    if (isBuiltin) {
        auto cb = [f](Span<const Value> ar, Span<Value> ou,
                      std::pmr::memory_resource *mr_) {
            ou[0] = hf::applyBuiltin(mr_, f, ar[0], "structfun");
        };
        outs[0] = structfun(cb, args[1], uniform, mr);
    } else {
        const auto &handle = args[0];
        auto cb = [&ctx, &handle](Span<const Value> ar, Span<Value> ou,
                                   std::pmr::memory_resource * /*mr*/) {
            auto r = ctx.engine->callFunctionHandleMulti(handle, ar, ou.size());
            for (size_t i = 0; i < ou.size() && i < r.size(); ++i)
                ou[i] = std::move(r[i]);
        };
        outs[0] = structfun(cb, args[1], uniform, mr);
    }
}

void getfield_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("getfield requires (S, name)",
                     0, 0, "getfield", "", "numkit:getfield:nargin");
    outs[0] = getfield(args[0], args[1], ctx.engine->resource());
}

void setfield_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("setfield requires (S, name, value)",
                     0, 0, "setfield", "", "numkit:setfield:nargin");
    outs[0] = setfield(args[0], args[1], args[2], ctx.engine->resource());
}

void orderfields_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("orderfields requires 1 argument",
                     0, 0, "orderfields", "", "numkit:orderfields:nargin");
    // 1-arg → alphabetical sort; 2-arg → reorder to the given name list.
    if (args.size() >= 2)
        outs[0] = orderfieldsByNames(args[0], args[1], ctx.engine->resource());
    else
        outs[0] = orderfields(args[0], ctx.engine->resource());
}

void struct2cell_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("struct2cell requires 1 argument",
                     0, 0, "struct2cell", "", "numkit:struct2cell:nargin");
    outs[0] = struct2cell(args[0], ctx.engine->resource());
}

void cell2struct_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cell2struct requires (C, fields)",
                     0, 0, "cell2struct", "", "numkit:cell2struct:nargin");
    outs[0] = cell2struct(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
