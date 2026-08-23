// toolboxes/builtin/include/numkit/builtin/language/cells/cell.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/fn_handle.hpp>
#include <numkit/value/value.hpp>

namespace numkit {
class Engine;
}

namespace numkit::runtime {

void registerCellsRuntime(Engine &engine);
void registerCellfunCallbackBuiltin(Engine &engine);

/// @brief Square cell array (`c = cell(n)`).
///
/// `cell(n)` builds an `n × n` cell of empty matrices.
///
/// @param n   Side length.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `n × n` empty cell array.
/// @see cell(rows, cols, mr), cell(rows, cols, pages, mr)
Value cell(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Rectangular cell array (`c = cell(rows, cols)`).
///
/// @param rows  Number of rows.
/// @param cols  Number of columns.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` empty cell array.
Value cell(size_t rows, size_t cols, std::pmr::memory_resource *mr = nullptr);

/// @brief 3-D cell array (`c = cell(rows, cols, pages)`).
///
/// `pages > 0` produces a 3-D cell; `pages == 0` returns the 2-D form.
///
/// @param rows   Number of rows.
/// @param cols   Number of columns.
/// @param pages  Number of pages (0 → 2-D).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Empty cell array.
Value cell(size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr = nullptr);

/// @brief Apply a callback to each cell
/// (`y = cellfun(fn, c, uniformOutput)`).
///
/// The callback is invoked once per cell with a 1-element `args`
/// holding `c.cellAt(i)` and writes a single Value into `outs[0]`.
///
/// `uniformOutput = true` packs scalar results into a numeric or
/// LOGICAL array of the same shape as `c` (type chosen from the
/// first result). `uniformOutput = false` packs results into a cell
/// of the same shape.
///
/// Engine-side fast path for built-in handles (`@numel`,
/// `@isempty`, `@sum`, `@class`, …) is handled by the engine
/// adapter — it pre-resolves to a direct C++ helper and wraps that
/// in the same callback signature before calling this function.
///
/// @param fn             Callback.
/// @param c              Cell array input.
/// @param uniformOutput  See above.
/// @param mr             Memory resource (nullptr → process default).
/// @return               Per-cell results (shape depends on
///                       `uniformOutput`).
/// @throws Error         Non-cell `c`, or non-scalar result in
///                       uniform mode.
Value cellfun(FnHandle fn, const Value &c, bool uniformOutput,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Wrap each element in a scalar cell (`c = num2cell(A)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Cell array of the same shape, each cell holding one element.
/// @see cell2mat
Value num2cell(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Concatenate cells into a matrix (`A = cell2mat(c)`).
///
/// Fast path for cells of DOUBLE scalars; general case is `horzcat`
/// of each row followed by `vertcat`.
///
/// @param c   Cell array (each cell numeric, shape-compatible).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Concatenated matrix.
/// @see num2cell, mat2cell
Value cell2mat(const Value &c, std::pmr::memory_resource *mr = nullptr);

/// @brief Test whether a cell array holds only char rows
/// (`tf = iscellstr(c)`).
///
/// @param c   Input Value.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL scalar.
Value iscellstr(const Value &c, std::pmr::memory_resource *mr = nullptr);

/// @brief Convert to cell-of-strings (`c = cellstr(x)`).
///
/// - Char row → `1 × 1` cell containing the row.
/// - String array → `N × 1` cell of strings.
/// - Cell-of-strings → identity passthrough.
///
/// @param x   Input Value.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Cell-of-strings.
Value cellstr(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Split a matrix into a cell array of blocks
/// (`c = mat2cell(A, R, C)`).
///
/// Vector input: split into a `1 × N` cell where element `i` is
/// `A(..., R(i))`. 2-D input: split rows by `R` and columns by `C`.
/// `sum(R) == size(A, 1)`, `sum(C) == size(A, 2)`. Block `(i, j)`
/// has shape `R(i) × C(j)`.
///
/// @param x         Input matrix.
/// @param rowSizes  Row partition sizes (non-negative vector).
/// @param colSizes  Column partition sizes (`Value::Empty` for vector mode).
/// @param mr        Memory resource (nullptr → process default).
/// @return          Cell array of blocks.
/// @throws Error  Partition sums don't match input size
///                (`m:mat2cell:rowSum` / `m:mat2cell:colSum`).
/// @see cell2mat
Value mat2cell(const Value &x, const Value &rowSizes,
               const Value &colSizes = Value::Empty,
               std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::runtime
