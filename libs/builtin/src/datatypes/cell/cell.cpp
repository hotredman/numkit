// libs/builtin/src/datatypes/cell/cell.cpp
//
// Cell construction (cell(n) / cell(r, c) / cell(r, c, p) / cell(dims))
// + cellfun. Shares the function-handle dispatch helpers with
// struct.cpp via the inline header below.

#include <numkit/builtin/datatypes/cell/cell.hpp>
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

Value cell(Allocator &, size_t n)
{
    return Value::cell(n, n);
}

Value cell(Allocator &, size_t rows, size_t cols)
{
    return Value::cell(rows, cols);
}

Value cell(Allocator &, size_t rows, size_t cols, size_t pages)
{
    if (pages > 0)
        return Value::cell3D(rows, cols, pages);
    return Value::cell(rows, cols);
}

Value cellfun(Allocator &alloc, const Value &fn, const Value &c,
               bool uniformOutput, Engine *engine)
{
    if (!c.isCell())
        throw Error("cellfun: second argument must be a cell array",
                     0, 0, "cellfun", "", "m:cellfun:notCell");
    hf::BuiltinFn f = hf::BuiltinFn::Numel;  // placeholder
    const bool isBuiltin = hf::tryParseBuiltinHandle(fn, f, "cellfun");

    const size_t n = c.numel();
    ScratchArena scratch_arena(alloc);
    ScratchVec<Value> results(scratch_arena.resource());
    results.reserve(n);
    for (size_t i = 0; i < n; ++i)
        results.push_back(hf::applyHandle(alloc, fn, f, isBuiltin,
                                          c.cellAt(i), engine, "cellfun"));

    if (uniformOutput) {
        if (isBuiltin)
            return hf::packUniform(alloc, f, results.data(), results.size(),
                                    c.dims(), "cellfun");
        // Anonymous: pack as DOUBLE / LOGICAL based on first result's type.
        const ValueType outT = (n > 0 && results[0].isLogical())
                           ? ValueType::LOGICAL : ValueType::DOUBLE;
        const auto &d = c.dims();
        const size_t r = d.rows();
        const size_t cc = d.cols();
        const size_t p = d.is3D() ? d.pages() : 0;
        auto out = (p > 0) ? Value::matrix3d(r, cc, p, outT, &alloc)
                           : Value::matrix(r, cc, outT, &alloc);
        for (size_t i = 0; i < n; ++i) {
            const Value &v = results[i];
            if (!v.isScalar())
                throw Error("cellfun: fn returned a non-scalar; pass 'UniformOutput', false",
                             0, 0, "cellfun", "", "m:cellfun:notScalar");
            if (outT == ValueType::LOGICAL)
                out.logicalDataMut()[i] = v.toBool() ? 1 : 0;
            else
                out.doubleDataMut()[i]  = v.toScalar();
        }
        return out;
    }

    // Cell output, same shape as C.
    const auto &d = c.dims();
    const size_t r = d.rows();
    const size_t cc = d.cols();
    const size_t p = d.is3D() ? d.pages() : 0;
    Value out = (p > 0) ? Value::cell3D(r, cc, p) : Value::cell(r, cc);
    for (size_t i = 0; i < n; ++i)
        out.cellAt(i) = std::move(results[i]);
    return out;
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void cellfun_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cellfun: requires at least 2 arguments (fn, C)",
                     0, 0, "cellfun", "", "m:cellfun:nargin");
    bool uniform = hf::parseUniformOutputFlag(args, 2, "cellfun");
    outs[0] = cellfun(ctx.engine->allocator(), args[0], args[1], uniform, ctx.engine);
}

void cell_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    Allocator &alloc = ctx.engine->allocator();
    if (args.empty())
        throw Error("cell: requires 1 argument", 0, 0, "cell", "", "m:cell:nargin");
    ScratchArena scratch_arena(alloc);
    // Single vector arg: cell([m n p ...]).
    if (args.size() == 1 && !args[0].isScalar() && args[0].numel() >= 2) {
        const size_t n = args[0].numel();
        auto dims = scratch_arena.vec<size_t>(n);
        for (size_t i = 0; i < n; ++i)
            dims[i] = static_cast<size_t>(args[0].elemAsDouble(i));
        outs[0] = Value::cellND(dims.data(), static_cast<int>(n));
        return;
    }
    size_t r = static_cast<size_t>(args[0].toScalar());
    if (args.size() == 1) {
        outs[0] = Value::cell(r, r);
        return;
    }
    if (args.size() == 2) {
        size_t c = static_cast<size_t>(args[1].toScalar());
        outs[0] = Value::cell(r, c);
        return;
    }
    if (args.size() == 3) {
        size_t c = static_cast<size_t>(args[1].toScalar());
        size_t p = static_cast<size_t>(args[2].toScalar());
        outs[0] = (p > 0) ? Value::cell3D(r, c, p) : Value::cell(r, c);
        return;
    }
    // 4+ scalar args: cell(m, n, p, q, ...).
    auto dims = scratch_arena.vec<size_t>(args.size());
    for (size_t i = 0; i < args.size(); ++i)
        dims[i] = static_cast<size_t>(args[i].toScalar());
    outs[0] = Value::cellND(dims.data(), static_cast<int>(args.size()));
}

} // namespace detail

} // namespace numkit::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — keep the registerCellStructFunctions hook empty; actual
// wiring happens in library.cpp via Phase-6c function pointers.
// ════════════════════════════════════════════════════════════════════════

namespace numkit {

void BuiltinLibrary::registerCellStructFunctions(Engine &)
{
    // Intentionally empty — see BuiltinLibrary::install() in library.cpp.
}

} // namespace numkit
