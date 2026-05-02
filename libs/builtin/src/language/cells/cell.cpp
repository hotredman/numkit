// libs/builtin/src/datatypes/cell/cell.cpp
//
// Cell construction (cell(n) / cell(r, c) / cell(r, c, p) / cell(dims))
// + cellfun. Shares the function-handle dispatch helpers with
// struct.cpp via the inline header below.

#include <numkit/builtin/language/cells/cell.hpp>
#include <numkit/builtin/language/arrays/matrix.hpp>  // horzcat / vertcat
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

Value cell(std::pmr::memory_resource *, size_t n)
{
    return Value::cell(n, n);
}

Value cell(std::pmr::memory_resource *, size_t rows, size_t cols)
{
    return Value::cell(rows, cols);
}

Value cell(std::pmr::memory_resource *, size_t rows, size_t cols, size_t pages)
{
    if (pages > 0)
        return Value::cell3D(rows, cols, pages);
    return Value::cell(rows, cols);
}

Value cellfun(std::pmr::memory_resource *mr, const Value &fn, const Value &c,
               bool uniformOutput, Engine *engine)
{
    if (!c.isCell())
        throw Error("cellfun: second argument must be a cell array",
                     0, 0, "cellfun", "", "m:cellfun:notCell");
    hf::BuiltinFn f = hf::BuiltinFn::Numel;  // placeholder
    const bool isBuiltin = hf::tryParseBuiltinHandle(fn, f, "cellfun");

    const size_t n = c.numel();
    ScratchArena scratch(mr);
    ScratchVec<Value> results(&scratch);
    results.reserve(n);
    for (size_t i = 0; i < n; ++i)
        results.push_back(hf::applyHandle(mr, fn, f, isBuiltin,
                                          c.cellAt(i), engine, "cellfun"));

    if (uniformOutput) {
        if (isBuiltin)
            return hf::packUniform(mr, f, results.data(), results.size(),
                                    c.dims(), "cellfun");
        // Anonymous: pack as DOUBLE / LOGICAL based on first result's type.
        const ValueType outT = (n > 0 && results[0].isLogical())
                           ? ValueType::LOGICAL : ValueType::DOUBLE;
        const auto &d = c.dims();
        const size_t r = d.rows();
        const size_t cc = d.cols();
        const size_t p = d.is3D() ? d.pages() : 0;
        auto out = (p > 0) ? Value::matrix3d(r, cc, p, outT, mr)
                           : Value::matrix(r, cc, outT, mr);
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
// Pack 15: num2cell / cell2mat / iscellstr / cellstr
// ════════════════════════════════════════════════════════════════════════

Value num2cell(std::pmr::memory_resource *mr, const Value &x)
{
    // Wraps each element of x in its own scalar cell. Supports DOUBLE,
    // SINGLE, integer, LOGICAL, COMPLEX, CHAR. Output mirrors input
    // shape (2-D fast path; 3-D handled by 3-D cell ctor).
    const auto &d = x.dims();
    if (d.ndim() > 3)
        throw Error("num2cell: ND inputs (>3) not yet supported",
                     0, 0, "num2cell", "", "m:num2cell:rank");
    auto c = d.is3D()
                ? Value::cell3D(d.rows(), d.cols(), d.pages(), mr)
                : Value::cell(d.rows(), d.cols(), mr);
    const size_t n = x.numel();
    if (x.isComplex()) {
        for (size_t i = 0; i < n; ++i)
            c.cellAt(i) = Value::complexScalar(x.complexData()[i], mr);
    } else {
        for (size_t i = 0; i < n; ++i)
            c.cellAt(i) = Value::scalar(x.elemAsDouble(i), mr);
    }
    return c;
}

Value cell2mat(std::pmr::memory_resource *mr, const Value &c)
{
    if (!c.isCell())
        throw Error("cell2mat: input must be a cell array",
                     0, 0, "cell2mat", "", "m:cell2mat:notCell");
    if (c.numel() == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Fast path: every entry is a DOUBLE scalar → cell shape becomes
    // matrix shape.
    bool allDoubleScalar = true;
    for (size_t i = 0; i < c.numel(); ++i) {
        const auto &e = c.cellAt(i);
        if (!e.isScalar() || e.type() != ValueType::DOUBLE) {
            allDoubleScalar = false;
            break;
        }
    }
    if (allDoubleScalar) {
        const auto &cd = c.dims();
        if (cd.ndim() > 2)
            throw Error("cell2mat: 3-D scalar cells not yet supported",
                         0, 0, "cell2mat", "", "m:cell2mat:rank");
        auto m = Value::matrix(cd.rows(), cd.cols(), ValueType::DOUBLE, mr);
        for (size_t i = 0; i < c.numel(); ++i)
            m.doubleDataMut()[i] = c.cellAt(i).toScalar();
        return m;
    }

    // General 2-D case: horzcat each row of the cell, then vertcat the
    // resulting row blocks. Sub-cell elements may themselves be matrices.
    const auto &cd = c.dims();
    if (cd.ndim() > 2)
        throw Error("cell2mat: only 2-D cell arrays of matrices are supported",
                     0, 0, "cell2mat", "", "m:cell2mat:rank");
    const size_t R = cd.rows(), C = cd.cols();
    if (R == 1) {
        ScratchArena scratch(mr);
        ScratchVec<Value> row(C, &scratch);
        for (size_t j = 0; j < C; ++j) row[j] = c.cellAt(j);
        return horzcat(mr, row.data(), C);
    }
    if (C == 1) {
        ScratchArena scratch(mr);
        ScratchVec<Value> col(R, &scratch);
        for (size_t i = 0; i < R; ++i) col[i] = c.cellAt(i);
        return vertcat(mr, col.data(), R);
    }
    // Full 2-D: horzcat each row, then vertcat results.
    ScratchArena scratch(mr);
    ScratchVec<Value> rowBlocks(R, &scratch);
    for (size_t i = 0; i < R; ++i) {
        ScratchVec<Value> rowCells(C, &scratch);
        for (size_t j = 0; j < C; ++j)
            rowCells[j] = c.cellAt(j * R + i);  // column-major linear index
        rowBlocks[i] = horzcat(mr, rowCells.data(), C);
    }
    return vertcat(mr, rowBlocks.data(), R);
}

Value iscellstr(std::pmr::memory_resource *mr, const Value &c)
{
    if (!c.isCell()) return Value::logicalScalar(false, mr);
    for (size_t i = 0; i < c.numel(); ++i) {
        const auto &e = c.cellAt(i);
        // Each cell entry must be a char row (or empty char []).
        if (!(e.isChar() || (e.isEmpty() && e.type() == ValueType::CHAR)))
            return Value::logicalScalar(false, mr);
    }
    return Value::logicalScalar(true, mr);
}

Value cellstr(std::pmr::memory_resource *mr, const Value &x)
{
    // char row → 1×1 cell of that char row.
    // string scalar/array → numel × 1 cell of char rows.
    // cell-of-strings → unchanged.
    if (x.isCell()) {
        // Validate it's already a cellstr; otherwise copy regardless
        // (MATLAB's cellstr just rebuilds char from any input).
        return x;
    }
    if (x.isChar()) {
        auto c = Value::cell(1, 1, mr);
        c.cellAt(0) = x;
        return c;
    }
    if (x.isString()) {
        const size_t n = x.numel();
        auto c = Value::cell(n, 1, mr);
        for (size_t i = 0; i < n; ++i)
            c.cellAt(i) = Value::fromString(x.stringElem(i), mr);
        return c;
    }
    throw Error("cellstr: input must be a string array or char array",
                 0, 0, "cellstr", "", "m:cellstr:type");
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
    outs[0] = cellfun(ctx.engine->resource(), args[0], args[1], uniform, ctx.engine);
}

void cell_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.empty())
        throw Error("cell: requires 1 argument", 0, 0, "cell", "", "m:cell:nargin");
    ScratchArena scratch(mr);
    // Single vector arg: cell([m n p ...]).
    if (args.size() == 1 && !args[0].isScalar() && args[0].numel() >= 2) {
        const size_t n = args[0].numel();
        auto dims = ScratchVec<size_t>(n, &scratch);
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
    auto dims = ScratchVec<size_t>(args.size(), &scratch);
    for (size_t i = 0; i < args.size(); ++i)
        dims[i] = static_cast<size_t>(args[i].toScalar());
    outs[0] = Value::cellND(dims.data(), static_cast<int>(args.size()));
}

void num2cell_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("num2cell requires 1 argument",
                     0, 0, "num2cell", "", "m:num2cell:nargin");
    outs[0] = num2cell(ctx.engine->resource(), args[0]);
}

void cell2mat_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cell2mat requires 1 argument",
                     0, 0, "cell2mat", "", "m:cell2mat:nargin");
    outs[0] = cell2mat(ctx.engine->resource(), args[0]);
}

void iscellstr_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("iscellstr requires 1 argument",
                     0, 0, "iscellstr", "", "m:iscellstr:nargin");
    outs[0] = iscellstr(ctx.engine->resource(), args[0]);
}

void cellstr_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cellstr requires 1 argument",
                     0, 0, "cellstr", "", "m:cellstr:nargin");
    outs[0] = cellstr(ctx.engine->resource(), args[0]);
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
