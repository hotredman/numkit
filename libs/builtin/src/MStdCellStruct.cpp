// libs/builtin/src/MStdCellStruct.cpp

#include <numkit/m/builtin/MStdCellStruct.hpp>
#include <numkit/m/builtin/MStdLibrary.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

namespace numkit::m::builtin {

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

MValue cell(Allocator &, size_t n)
{
    return MValue::cell(n, n);
}

MValue cell(Allocator &, size_t rows, size_t cols)
{
    return MValue::cell(rows, cols);
}

MValue cell(Allocator &, size_t rows, size_t cols, size_t pages)
{
    if (pages > 0)
        return MValue::cell3D(rows, cols, pages);
    return MValue::cell(rows, cols);
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

void cell_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    Allocator &alloc = ctx.engine->allocator();
    (void)alloc;
    if (args.empty())
        throw MError("cell: requires 1 argument", 0, 0, "cell", "", "m:cell:nargin");
    // Single vector arg: cell([m n p ...]).
    if (args.size() == 1 && !args[0].isScalar() && args[0].numel() >= 2) {
        const size_t n = args[0].numel();
        std::vector<size_t> dims(n);
        for (size_t i = 0; i < n; ++i)
            dims[i] = static_cast<size_t>(args[0].elemAsDouble(i));
        outs[0] = MValue::cellND(dims.data(), static_cast<int>(n));
        return;
    }
    size_t r = static_cast<size_t>(args[0].toScalar());
    if (args.size() == 1) {
        outs[0] = MValue::cell(r, r);
        return;
    }
    if (args.size() == 2) {
        size_t c = static_cast<size_t>(args[1].toScalar());
        outs[0] = MValue::cell(r, c);
        return;
    }
    if (args.size() == 3) {
        size_t c = static_cast<size_t>(args[1].toScalar());
        size_t p = static_cast<size_t>(args[2].toScalar());
        outs[0] = (p > 0) ? MValue::cell3D(r, c, p) : MValue::cell(r, c);
        return;
    }
    // 4+ scalar args: cell(m, n, p, q, ...).
    std::vector<size_t> dims(args.size());
    for (size_t i = 0; i < args.size(); ++i)
        dims[i] = static_cast<size_t>(args[i].toScalar());
    outs[0] = MValue::cellND(dims.data(), static_cast<int>(args.size()));
}

} // namespace detail

} // namespace numkit::m::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — keep the registerCellStructFunctions hook empty; actual
// wiring happens in MStdLibrary.cpp via Phase-6c function pointers.
// ════════════════════════════════════════════════════════════════════════

namespace numkit::m {

void StdLibrary::registerCellStructFunctions(Engine &)
{
    // Intentionally empty — see StdLibrary::install() in MStdLibrary.cpp.
}

} // namespace numkit::m
