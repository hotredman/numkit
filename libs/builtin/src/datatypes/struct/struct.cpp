// libs/builtin/src/datatypes/struct/struct.cpp
//
// Struct construction (struct, fieldnames, isfield, rmfield) + structfun.
// Shares function-handle dispatch helpers with cell.cpp via the inline
// header below.

#include <numkit/m/builtin/datatypes/struct/struct.hpp>
#include <numkit/m/builtin/MStdLibrary.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "../_handlefn_helpers.hpp"

#include <vector>

namespace numkit::m::builtin {

namespace hf = ::numkit::m::builtin::detail::handlefn;

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

MValue structure(Allocator &)
{
    return MValue::structure();
}

MValue structure(Allocator &, Span<const MValue> nameValuePairs)
{
    auto s = MValue::structure();
    for (size_t i = 0; i + 1 < nameValuePairs.size(); i += 2) {
        if (!nameValuePairs[i].isChar() && !nameValuePairs[i].isString())
            throw MError("struct: field names must be char arrays", 0, 0, "struct", "",
                         "m:struct:invalidFieldName");
        s.field(nameValuePairs[i].toString()) = nameValuePairs[i + 1];
    }
    return s;
}

MValue fieldnames(Allocator &alloc, const MValue &s)
{
    if (!s.isStruct())
        throw MError("fieldnames requires a struct", 0, 0, "fieldnames", "",
                     "m:fieldnames:notStruct");
    const auto &fields = s.structFields();
    auto c = MValue::cell(fields.size(), 1);
    size_t i = 0;
    for (const auto &[k, v] : fields)
        c.cellAt(i++) = MValue::fromString(k, &alloc);
    return c;
}

MValue isfield(Allocator &alloc, const MValue &s, const MValue &name)
{
    if (!s.isStruct())
        return MValue::logicalScalar(false, &alloc);
    return MValue::logicalScalar(s.hasField(name.toString()), &alloc);
}

MValue rmfield(Allocator &, const MValue &s, const MValue &name)
{
    if (!s.isStruct())
        throw MError("rmfield requires a struct", 0, 0, "rmfield", "",
                     "m:rmfield:notStruct");
    MValue out = s;
    out.structFields().erase(name.toString());
    return out;
}

MValue structfun(Allocator &alloc, const MValue &fn, const MValue &s,
                 bool uniformOutput, Engine *engine)
{
    if (!s.isStruct())
        throw MError("structfun: second argument must be a scalar struct",
                     0, 0, "structfun", "", "m:structfun:notStruct");
    hf::BuiltinFn f = hf::BuiltinFn::Numel;  // placeholder
    const bool isBuiltin = hf::tryParseBuiltinHandle(fn, f, "structfun");

    const auto &fields = s.structFields();
    const size_t n = fields.size();
    std::vector<MValue> results;
    results.reserve(n);
    for (const auto &kv : fields)
        results.push_back(hf::applyHandle(alloc, fn, f, isBuiltin,
                                          kv.second, engine, "structfun"));

    if (uniformOutput) {
        // Uniform: column vector of length n.
        if (isBuiltin && hf::builtinReturnsString(f))
            throw MError("structfun: @class output must use UniformOutput=false",
                         0, 0, "structfun", "", "m:structfun:nonUniform");
        // For built-in handles use the static return-type tag; for
        // anonymous handles infer from the first result.
        MType outT = MType::DOUBLE;
        if (isBuiltin && hf::builtinReturnsLogical(f))
            outT = MType::LOGICAL;
        else if (!isBuiltin && n > 0 && results[0].isLogical())
            outT = MType::LOGICAL;
        auto out = MValue::matrix(n, 1, outT, &alloc);
        for (size_t i = 0; i < n; ++i) {
            const MValue &v = results[i];
            if (!v.isScalar())
                throw MError("structfun: fn returned a non-scalar; pass 'UniformOutput', false",
                             0, 0, "structfun", "", "m:structfun:notScalar");
            if (outT == MType::LOGICAL)
                out.logicalDataMut()[i] = v.toBool() ? 1 : 0;
            else
                out.doubleDataMut()[i]  = v.toScalar();
        }
        return out;
    }

    // Cell output, column vector (1 entry per field).
    auto out = MValue::cell(n, 1);
    for (size_t i = 0; i < n; ++i)
        out.cellAt(i) = std::move(results[i]);
    return out;
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void struct_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    outs[0] = structure(ctx.engine->allocator(), args);
}

void fieldnames_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("fieldnames: requires 1 argument", 0, 0, "fieldnames", "",
                     "m:fieldnames:nargin");
    outs[0] = fieldnames(ctx.engine->allocator(), args[0]);
}

void isfield_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("isfield requires 2 arguments", 0, 0, "isfield", "",
                     "m:isfield:nargin");
    outs[0] = isfield(ctx.engine->allocator(), args[0], args[1]);
}

void rmfield_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("rmfield requires 2 arguments", 0, 0, "rmfield", "",
                     "m:rmfield:nargin");
    outs[0] = rmfield(ctx.engine->allocator(), args[0], args[1]);
}

void structfun_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("structfun: requires at least 2 arguments (fn, S)",
                     0, 0, "structfun", "", "m:structfun:nargin");
    bool uniform = hf::parseUniformOutputFlag(args, 2, "structfun");
    outs[0] = structfun(ctx.engine->allocator(), args[0], args[1], uniform, ctx.engine);
}

} // namespace detail

} // namespace numkit::m::builtin
