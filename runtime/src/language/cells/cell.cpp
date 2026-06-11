// toolboxes/builtin/src/datatypes/cell/cell.cpp
//
// Cell construction (cell(n) / cell(r, c) / cell(r, c, p) / cell(dims))
// + cellfun. Shares the function-handle dispatch helpers with
// struct.cpp via the inline header below.

#include <numkit/runtime/language/cells/cell.hpp>
#include <numkit/lang/arrays/matrix.hpp>  // horzcat / vertcat
#include <numkit/builtin/library.hpp>

#include <numkit/core/callback_builtin.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/vm.hpp>

#include "language/handles/_handlefn_helpers.hpp"

namespace numkit::builtin {
using namespace numkit::lang;  // C4c cross-area
using namespace numkit::math;  // C4c cross-area

namespace hf = ::numkit::builtin::detail::handlefn;

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

Value cell(size_t n, std::pmr::memory_resource *)
{
    return Value::cell(n, n);
}

Value cell(size_t rows, size_t cols, std::pmr::memory_resource *)
{
    return Value::cell(rows, cols);
}

Value cell(size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *)
{
    if (pages > 0)
        return Value::cell3D(rows, cols, pages);
    return Value::cell(rows, cols);
}

Value cellfun(FnHandle fn, const Value &c, bool uniformOutput,
              std::pmr::memory_resource *mr)
{
    if (!c.isCell())
        throw Error("cellfun: second argument must be a cell array",
                     0, 0, "cellfun", "", "numkit:cellfun:notCell");

    const size_t n = c.numel();
    ScratchArena scratch(mr);
    ScratchVec<Value> results(&scratch);
    results.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        Value arg = c.cellAt(i);
        Value out;
        Span<const Value> ar(&arg, 1);
        Span<Value>       ou(&out, 1);
        fn(ar, ou, mr);
        results.push_back(std::move(out));
    }

    if (uniformOutput) {
        // Pack as DOUBLE / LOGICAL based on first result's type.
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
                             0, 0, "cellfun", "", "numkit:cellfun:notScalar");
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

Value num2cell(const Value &x, std::pmr::memory_resource *mr)
{
    // Wraps each element of x in its own scalar cell. Supports DOUBLE,
    // SINGLE, integer, LOGICAL, COMPLEX, CHAR. Output mirrors input
    // shape (2-D fast path; 3-D handled by 3-D cell ctor).
    const auto &d = x.dims();
    if (d.ndim() > 3)
        throw Error("num2cell: ND inputs (>3) not yet supported",
                     0, 0, "num2cell", "", "numkit:num2cell:rank");
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

// num2cell(x, dims): collapse the listed dimension(s) into each cell. The
// result cell has size(x) with the collapsed dims set to 1; each cell holds
// the corresponding sub-array. 2-D inputs only (N-D deferred); dims entries
// > 2 are trivial singleton collapses and ignored. dims = {} or no real
// collapse falls back to the element-wise num2cell.
static Value num2cellDim(const Value &x, const std::vector<int> &dims,
                         std::pmr::memory_resource *mr)
{
    if (x.dims().ndim() > 2)
        throw Error("num2cell: the dimension form is supported for 2-D "
                    "inputs only (N-D deferred)",
                     0, 0, "num2cell", "", "numkit:num2cell:ndDim");
    bool c1 = false, c2 = false;
    for (int d : dims) {
        if (d < 1)
            throw Error("num2cell: dimension argument must be a positive integer",
                         0, 0, "num2cell", "", "numkit:num2cell:badDim");
        if (d == 1) c1 = true;
        else if (d == 2) c2 = true;
        // d > 2: a singleton dimension of a 2-D array — collapsing it is a no-op.
    }
    if (!c1 && !c2) return num2cell(x, mr);

    const size_t r = static_cast<size_t>(x.dims().dim(0));
    const size_t c = (x.dims().ndim() >= 2) ? static_cast<size_t>(x.dims().dim(1)) : 1;
    const bool cplx = x.isComplex();

    auto makeSlice = [&](size_t rr, size_t ccx, auto srcIndex) -> Value {
        if (cplx) {
            auto v = Value::matrix(rr, ccx, ValueType::COMPLEX, mr);
            auto *vd = v.complexDataMut();
            const auto *xd = x.complexData();
            for (size_t k = 0; k < rr * ccx; ++k) vd[k] = xd[srcIndex(k)];
            return v;
        }
        auto v = Value::matrix(rr, ccx, ValueType::DOUBLE, mr);
        auto *vd = v.doubleDataMut();
        for (size_t k = 0; k < rr * ccx; ++k) vd[k] = x.elemAsDouble(srcIndex(k));
        return v;
    };

    if (c1 && c2) {
        auto out = Value::cell(1, 1, mr);
        out.cellAt(0) = makeSlice(r, c, [](size_t k) { return k; });
        return out;
    }
    if (c1) {                                   // collapse rows -> 1 x c cell
        auto out = Value::cell(1, c, mr);
        for (size_t j = 0; j < c; ++j) {
            const size_t base = j * r;
            out.cellAt(j) = makeSlice(r, 1, [base](size_t i) { return base + i; });
        }
        return out;
    }
    auto out = Value::cell(r, 1, mr);           // collapse cols -> r x 1 cell
    for (size_t i = 0; i < r; ++i)
        out.cellAt(i) = makeSlice(1, c, [i, r](size_t j) { return j * r + i; });
    return out;
}

Value cell2mat(const Value &c, std::pmr::memory_resource *mr)
{
    if (!c.isCell())
        throw Error("cell2mat: input must be a cell array",
                     0, 0, "cell2mat", "", "numkit:cell2mat:notCell");
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
                         0, 0, "cell2mat", "", "numkit:cell2mat:rank");
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
                     0, 0, "cell2mat", "", "numkit:cell2mat:rank");
    const size_t R = cd.rows(), C = cd.cols();
    if (R == 1) {
        ScratchArena scratch(mr);
        ScratchVec<Value> row(C, &scratch);
        for (size_t j = 0; j < C; ++j) row[j] = c.cellAt(j);
        return horzcat(Span<const Value>(row.data(), C), mr);
    }
    if (C == 1) {
        ScratchArena scratch(mr);
        ScratchVec<Value> col(R, &scratch);
        for (size_t i = 0; i < R; ++i) col[i] = c.cellAt(i);
        return vertcat(Span<const Value>(col.data(), R), mr);
    }
    // Full 2-D: horzcat each row, then vertcat results.
    ScratchArena scratch(mr);
    ScratchVec<Value> rowBlocks(R, &scratch);
    for (size_t i = 0; i < R; ++i) {
        ScratchVec<Value> rowCells(C, &scratch);
        for (size_t j = 0; j < C; ++j)
            rowCells[j] = c.cellAt(j * R + i);  // column-major linear index
        rowBlocks[i] = horzcat(Span<const Value>(rowCells.data(), C), mr);
    }
    return vertcat(Span<const Value>(rowBlocks.data(), R), mr);
}

Value iscellstr(const Value &c, std::pmr::memory_resource *mr)
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

Value cellstr(const Value &x, std::pmr::memory_resource *mr)
{
    // char row → 1×1 cell of that char row.
    // M-by-N char matrix → M-by-1 cell, each element a deblank'd row.
    // string scalar/array → numel × 1 cell of char rows.
    // cell-of-strings → unchanged.
    if (x.isCell()) {
        // Validate it's already a cellstr; otherwise copy regardless
        // (MATLAB's cellstr just rebuilds char from any input).
        return x;
    }
    if (x.isChar()) {
        const size_t nr = x.dims().rows();
        const size_t nc = x.dims().cols();
        if (nr <= 1) {
            auto c = Value::cell(1, 1, mr);
            c.cellAt(0) = x;
            return c;
        }
        // Multi-row char matrix: split per row and deblank trailing
        // spaces. Char data is column-major, so element (r, c) lives at
        // index r + c * nr. See BUGS.md #17.
        const char *src = x.charData();
        auto out = Value::cell(nr, 1, mr);
        std::string row;
        row.reserve(nc);
        for (size_t r = 0; r < nr; ++r) {
            row.clear();
            for (size_t c = 0; c < nc; ++c)
                row.push_back(src[r + c * nr]);
            // deblank: strip trailing spaces (but not other whitespace,
            // matching MATLAB's deblank).
            while (!row.empty() && row.back() == ' ')
                row.pop_back();
            out.cellAt(r) = Value::fromString(row, mr);
        }
        return out;
    }
    if (x.isString()) {
        const size_t n = x.numel();
        auto c = Value::cell(n, 1, mr);
        for (size_t i = 0; i < n; ++i)
            c.cellAt(i) = Value::fromString(x.stringElem(i), mr);
        return c;
    }
    throw Error("cellstr: input must be a string array or char array",
                 0, 0, "cellstr", "", "numkit:cellstr:type");
}

Value mat2cell(const Value &x, const Value &rowSizesV, const Value &colSizesV, std::pmr::memory_resource *mr)
{
    if (x.dims().ndim() > 2)
        throw Error("mat2cell: only 2-D inputs are supported",
                     0, 0, "mat2cell", "", "numkit:mat2cell:rank");
    if (x.type() != ValueType::DOUBLE)
        throw Error("mat2cell: only DOUBLE inputs are supported",
                     0, 0, "mat2cell", "", "numkit:mat2cell:type");

    const size_t R = x.dims().rows(), C = x.dims().cols();
    const size_t nRow = rowSizesV.numel();
    const size_t nCol = colSizesV.numel();

    // Vector form: mat2cell(v, sizes). Treat sizes as row-direction
    // when v is a column, column-direction when v is a row.
    const bool vectorForm = colSizesV.isEmpty();
    ScratchArena scratch(mr);
    auto rowS = ScratchVec<size_t>(&scratch);
    auto colS = ScratchVec<size_t>(&scratch);

    if (vectorForm) {
        if (R == 1) {
            rowS.assign({R});
            colS.reserve(nRow);
            for (size_t i = 0; i < nRow; ++i)
                colS.push_back(static_cast<size_t>(rowSizesV.elemAsDouble(i)));
        } else {
            rowS.reserve(nRow);
            for (size_t i = 0; i < nRow; ++i)
                rowS.push_back(static_cast<size_t>(rowSizesV.elemAsDouble(i)));
            colS.assign({C});
        }
    } else {
        rowS.reserve(nRow);
        for (size_t i = 0; i < nRow; ++i)
            rowS.push_back(static_cast<size_t>(rowSizesV.elemAsDouble(i)));
        colS.reserve(nCol);
        for (size_t j = 0; j < nCol; ++j)
            colS.push_back(static_cast<size_t>(colSizesV.elemAsDouble(j)));
    }

    size_t rsum = 0, csum = 0;
    for (size_t s : rowS) rsum += s;
    for (size_t s : colS) csum += s;
    if (rsum != R)
        throw Error("mat2cell: row sizes must sum to size(A,1)",
                     0, 0, "mat2cell", "", "numkit:mat2cell:rowSum");
    if (csum != C)
        throw Error("mat2cell: column sizes must sum to size(A,2)",
                     0, 0, "mat2cell", "", "numkit:mat2cell:colSum");

    auto c = Value::cell(rowS.size(), colS.size(), mr);
    const double *src = x.doubleData();
    size_t rowOff = 0;
    for (size_t i = 0; i < rowS.size(); ++i) {
        size_t colOff = 0;
        for (size_t j = 0; j < colS.size(); ++j) {
            const size_t br = rowS[i], bc = colS[j];
            auto block = Value::matrix(br, bc, ValueType::DOUBLE, mr);
            double *dst = block.doubleDataMut();
            for (size_t cc = 0; cc < bc; ++cc)
                for (size_t rr = 0; rr < br; ++rr)
                    dst[cc * br + rr] = src[(colOff + cc) * R + (rowOff + rr)];
            c.cellAt(j * rowS.size() + i) = std::move(block);
            colOff += bc;
        }
        rowOff += rowS[i];
    }
    return c;
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

// ── State-machine cellfun (VM_CALLBACKS_PLAN.md) ─────────────────────────────
// Drives cellfun(@userfunc, c [, 'UniformOutput', tf]) one element at a time via
// the shared LoopContinuation, running each callback as a pausable VM frame
// instead of the synchronous callReentrant path. Only the user-code-handle
// single-cell form is taken here; everything else (builtin handle, multi-output,
// bad options) falls back to the synchronous cellfun_reg via tryStart → nullptr.
struct CellfunCallbackBuiltin : CallbackBuiltin
{
    std::shared_ptr<VmContinuation> tryStart(Span<const Value> args, std::size_t nargout,
                                             Value *dest, Engine &eng) override
    {
        if (args.size() < 2 || nargout > 1)
            return nullptr; // nargin / multi-output → synchronous path
        if (!eng.isUserCodeHandle(args[0]))
            return nullptr; // builtin handle → fast synchronous path
        if (!args[1].isCell())
            return nullptr; // synchronous path reports notCell
        // Multi-cell form (cellfun(fn, C1, C2, ...)) → synchronous path, which
        // zips the leading cell arrays; the pausable state machine only drives
        // the single-cell user-handle case.
        if (args.size() > 2 && args[2].isCell())
            return nullptr;
        // Parse 'UniformOutput' exactly as the synchronous path (throws on
        // unsupported options — same error the user would otherwise get).
        const bool uniform = hf::parseUniformOutputFlag(args, 2, "cellfun");
        auto *mr = eng.resource();
        Value cellArg = args[1];
        const auto &d = cellArg.dims();
        const std::size_t rows = d.rows(), cols = d.cols(), pages = d.is3D() ? d.pages() : 0;
        auto cont = std::make_shared<LoopContinuation>();
        cont->handle = args[0];
        cont->n = cellArg.numel();
        cont->dest = dest;
        cont->makeArgs = [cellArg](std::size_t i) -> std::vector<Value> {
            return {cellArg.cellAt(i)};
        };
        // Mirror the synchronous cellfun() helper's output construction.
        cont->pack = [uniform, rows, cols, pages, mr](std::vector<Value> &results) -> Value {
            const std::size_t n = results.size();
            if (uniform) {
                const ValueType outT = (n > 0 && results[0].isLogical()) ? ValueType::LOGICAL
                                                                         : ValueType::DOUBLE;
                Value out = (pages > 0) ? Value::matrix3d(rows, cols, pages, outT, mr)
                                        : Value::matrix(rows, cols, outT, mr);
                for (std::size_t k = 0; k < n; ++k) {
                    const Value &v = results[k];
                    if (!v.isScalar())
                        throw Error("cellfun: fn returned a non-scalar; pass 'UniformOutput', "
                                    "false",
                                    0, 0, "cellfun", "", "numkit:cellfun:notScalar");
                    if (outT == ValueType::LOGICAL)
                        out.logicalDataMut()[k] = v.toBool() ? 1 : 0;
                    else
                        out.doubleDataMut()[k] = v.toScalar();
                }
                return out;
            }
            Value out = (pages > 0) ? Value::cell3D(rows, cols, pages) : Value::cell(rows, cols);
            for (std::size_t k = 0; k < n; ++k)
                out.cellAt(k) = results[k];
            return out;
        };
        cont->results.reserve(cont->n);
        return cont;
    }
};

// Multi-cell cellfun: zip the leading cell arrays and apply `apply` per element
// (apply receives {C1{i}, C2{i}, ...}). Packs a uniform (scalar) result or a
// cell, with the shape of the first cell. Mirrors the single-cell cellfun().
template <typename Apply>
static Value cellfunN(Span<const Value> cells, bool uniformOutput, Apply apply,
                      std::pmr::memory_resource *mr)
{
    const size_t nc = cells.size();
    const Value &C0 = cells[0];
    const size_t n  = C0.numel();
    ScratchArena scratch(mr);
    ScratchVec<Value> results(&scratch);
    results.reserve(n);
    ScratchVec<Value> argbuf(nc, &scratch);
    for (size_t i = 0; i < n; ++i) {
        for (size_t k = 0; k < nc; ++k) argbuf[k] = cells[k].cellAt(i);
        Value out;
        Span<const Value> ar(argbuf.data(), nc);
        Span<Value>       ou(&out, 1);
        apply(ar, ou, mr);
        results.push_back(std::move(out));
    }
    const auto &d = C0.dims();
    const size_t r = d.rows(), cc = d.cols(), p = d.is3D() ? d.pages() : 0;
    if (uniformOutput) {
        const ValueType outT = (n > 0 && results[0].isLogical())
                               ? ValueType::LOGICAL : ValueType::DOUBLE;
        Value out = (p > 0) ? Value::matrix3d(r, cc, p, outT, mr)
                            : Value::matrix(r, cc, outT, mr);
        for (size_t i = 0; i < n; ++i) {
            const Value &v = results[i];
            if (!v.isScalar())
                throw Error("cellfun: fn returned a non-scalar; pass 'UniformOutput', false",
                             0, 0, "cellfun", "", "numkit:cellfun:notScalar");
            if (outT == ValueType::LOGICAL) out.logicalDataMut()[i] = v.toBool() ? 1 : 0;
            else                            out.doubleDataMut()[i]  = v.toScalar();
        }
        return out;
    }
    Value out = (p > 0) ? Value::cell3D(r, cc, p) : Value::cell(r, cc);
    for (size_t i = 0; i < n; ++i) out.cellAt(i) = std::move(results[i]);
    return out;
}

// Legacy string-function-name forms — MATLAB cellfun('name', C[, extra]):
//   isempty | islogical | isreal  → logical per cell
//   length | ndims | prodofsize   → double per cell
//   size, C, k                    → size(C{i}, k)
//   isclass, C, 'classname'       → isa(C{i}, classname)
static Value cellfunStringForm(Span<const Value> args, std::pmr::memory_resource *mr)
{
    const std::string name = hf::lowerName(args[0].toString());
    if (args.size() < 2 || !args[1].isCell())
        throw Error("cellfun: the '" + name + "' form requires a cell array",
                     0, 0, "cellfun", "", "numkit:cellfun:notCell");
    const Value &C = args[1];
    const size_t n = C.numel();

    int sizeDim = 0;
    std::string clsName;
    const bool isSize    = (name == "size");
    const bool isIsclass = (name == "isclass");
    const bool wantLogical =
        (name == "isempty" || name == "isreal" || name == "islogical" || isIsclass);

    if (isSize) {
        if (args.size() < 3)
            throw Error("cellfun: the 'size' form needs a dimension: cellfun('size', C, k)",
                         0, 0, "cellfun", "", "numkit:cellfun:badName");
        sizeDim = static_cast<int>(args[2].toScalar());
        if (sizeDim < 1)
            throw Error("cellfun: 'size' dimension must be a positive integer",
                         0, 0, "cellfun", "", "numkit:cellfun:badName");
    } else if (isIsclass) {
        if (args.size() < 3 || !(args[2].isChar() || args[2].isString()))
            throw Error("cellfun: the 'isclass' form needs a class name: cellfun('isclass', C, 'cls')",
                         0, 0, "cellfun", "", "numkit:cellfun:badName");
        clsName = args[2].toString();
    } else if (!(name == "isempty" || name == "length" || name == "ndims"
              || name == "prodofsize" || name == "isreal" || name == "islogical")) {
        throw Error("cellfun: '" + name + "' is not a recognised function name; "
                    "pass a function handle (e.g. @" + name + ") instead",
                     0, 0, "cellfun", "", "numkit:cellfun:badName");
    }

    const auto &d = C.dims();
    const size_t r = d.rows(), cc = d.cols(), p = d.is3D() ? d.pages() : 0;
    const ValueType outT = wantLogical ? ValueType::LOGICAL : ValueType::DOUBLE;
    Value out = (p > 0) ? Value::matrix3d(r, cc, p, outT, mr)
                        : Value::matrix(r, cc, outT, mr);
    for (size_t i = 0; i < n; ++i) {
        const Value e = C.cellAt(i);
        double val = 0.0;
        if (name == "isempty")          val = (e.isEmpty() || e.numel() == 0) ? 1.0 : 0.0;
        else if (name == "islogical")   val = e.isLogical() ? 1.0 : 0.0;
        else if (name == "isreal")      val = e.isComplex() ? 0.0 : 1.0;
        else if (name == "ndims")       val = static_cast<double>(e.dims().ndim());
        else if (name == "prodofsize")  val = static_cast<double>(e.numel());
        else if (name == "length") {
            if (e.numel() == 0) val = 0.0;
            else {
                size_t mx = 0; const auto &ed = e.dims();
                for (int k = 0; k < ed.ndim(); ++k)
                    if (ed.dim(k) > mx) mx = ed.dim(k);
                val = static_cast<double>(mx);
            }
        }
        else if (isSize)                val = static_cast<double>(e.dims().dim(sizeDim - 1));
        else if (isIsclass)             val = (clsName == hf::classNameOf(e)) ? 1.0 : 0.0;
        if (wantLogical) out.logicalDataMut()[i] = (val != 0.0) ? 1 : 0;
        else             out.doubleDataMut()[i]  = val;
    }
    return out;
}

void cellfun_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cellfun: requires at least 2 arguments (fn, C)",
                     0, 0, "cellfun", "", "numkit:cellfun:nargin");
    auto *mr = ctx.engine->resource();

    // Legacy string-function-name form: cellfun('isempty', C), cellfun('size', C, k), …
    if (args[0].isChar() || args[0].isString()) {
        outs[0] = cellfunStringForm(args, mr);
        return;
    }

    // Collect the leading cell arrays (multi-cell zip); name/value options follow.
    size_t nCells = 0;
    while (1 + nCells < args.size() && args[1 + nCells].isCell()) ++nCells;
    if (nCells == 0)
        throw Error("cellfun: second argument must be a cell array",
                     0, 0, "cellfun", "", "numkit:cellfun:notCell");
    const bool uniform = hf::parseUniformOutputFlag(args, 1 + nCells, "cellfun");

    if (nCells == 1) {
        // Single cell — built-in fast-path or engine handle (unchanged behaviour).
        hf::BuiltinFn f = hf::BuiltinFn::Numel;
        const bool isBuiltin = hf::tryParseBuiltinHandle(args[0], f, "cellfun");
        if (uniform && isBuiltin && hf::builtinReturnsString(f))
            throw Error("cellfun: @class output must use UniformOutput=false",
                         0, 0, "cellfun", "", "numkit:cellfun:nonUniform");
        if (isBuiltin) {
            auto cb = [f](Span<const Value> ar, Span<Value> ou,
                          std::pmr::memory_resource *mr_) {
                ou[0] = hf::applyBuiltin(mr_, f, ar[0], "cellfun");
            };
            outs[0] = cellfun(cb, args[1], uniform, mr);
        } else {
            const auto &handle = args[0];
            auto cb = [&ctx, &handle](Span<const Value> ar, Span<Value> ou,
                                       std::pmr::memory_resource * /*mr*/) {
                auto r = ctx.engine->callFunctionHandleMulti(handle, ar, ou.size());
                for (size_t i = 0; i < ou.size() && i < r.size(); ++i)
                    ou[i] = std::move(r[i]);
            };
            outs[0] = cellfun(cb, args[1], uniform, mr);
        }
        return;
    }

    // Multiple cells: all must be the same size; apply fn(C1{i}, …, Cn{i})
    // through the engine handle (covers @(a,b)… and multi-arg builtins like
    // @plus; the unary built-in fast-path doesn't fit a multi-cell call).
    const size_t n0 = args[1].numel();
    for (size_t k = 1; k < nCells; ++k)
        if (args[1 + k].numel() != n0)
            throw Error("cellfun: all of the input cell arrays must be the same size",
                         0, 0, "cellfun", "", "numkit:cellfun:size");
    const auto &handle = args[0];
    auto cb = [&ctx, &handle](Span<const Value> ar, Span<Value> ou,
                               std::pmr::memory_resource * /*mr*/) {
        auto r = ctx.engine->callFunctionHandleMulti(handle, ar, ou.size());
        for (size_t i = 0; i < ou.size() && i < r.size(); ++i)
            ou[i] = std::move(r[i]);
    };
    outs[0] = cellfunN(Span<const Value>(&args[1], nCells), uniform, cb, mr);
}

void cell_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.empty())
        throw Error("cell: requires 1 argument", 0, 0, "cell", "", "numkit:cell:nargin");
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
                     0, 0, "num2cell", "", "numkit:num2cell:nargin");
    auto *mr = ctx.engine->resource();
    // num2cell(A, dims): the trailing arg(s) list the dimension(s) to
    // collapse into each cell.
    if (args.size() >= 2 && !args[1].isEmpty()) {
        std::vector<int> dims;
        for (size_t i = 1; i < args.size(); ++i) {
            const Value &dv = args[i];
            for (size_t k = 0; k < dv.numel(); ++k)
                dims.push_back(static_cast<int>(dv.elemAsDouble(k)));
        }
        outs[0] = num2cellDim(args[0], dims, mr);
        return;
    }
    outs[0] = num2cell(args[0], mr);
}

void cell2mat_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cell2mat requires 1 argument",
                     0, 0, "cell2mat", "", "numkit:cell2mat:nargin");
    outs[0] = cell2mat(args[0], ctx.engine->resource());
}

void iscellstr_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("iscellstr requires 1 argument",
                     0, 0, "iscellstr", "", "numkit:iscellstr:nargin");
    outs[0] = iscellstr(args[0], ctx.engine->resource());
}

void cellstr_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cellstr requires 1 argument",
                     0, 0, "cellstr", "", "numkit:cellstr:nargin");
    outs[0] = cellstr(args[0], ctx.engine->resource());
}

void mat2cell_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mat2cell: requires (A, R[, C])",
                     0, 0, "mat2cell", "", "numkit:mat2cell:nargin");
    auto *mr = ctx.engine->resource();
    const Value &rArg = args[1];
    if (args.size() == 2) {
        outs[0] = mat2cell(args[0], rArg, Value::Empty, mr);
    } else {
        outs[0] = mat2cell(args[0], rArg, args[2], mr);
    }
}

} // namespace detail

void registerCellfunCallbackBuiltin(Engine &engine)
{
    engine.registerCallbackBuiltin("cellfun", std::make_shared<detail::CellfunCallbackBuiltin>());
}

} // namespace numkit::builtin
