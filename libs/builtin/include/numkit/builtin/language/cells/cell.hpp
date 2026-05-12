// libs/builtin/include/numkit/builtin/language/cells/cell.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit { class Engine; }

namespace numkit::builtin {

using ::numkit::Engine;

//// cell(n) — n×n cell array. MATLAB behavior.
Value cell(size_t n, std::pmr::memory_resource *mr = nullptr);

/// cell(r, c) — r×c cell array.
Value cell(size_t rows, size_t cols, std::pmr::memory_resource *mr = nullptr);

/// cell(r, c, p) — 3D cell array when p > 0; else 2D r×c.
Value cell(size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr = nullptr);

/// Apply a function handle to each cell of `C`.
/// Built-in handles supported (fast path via funcHandleName()):
///   shape:    numel, length, ndims, isempty
///   type:     isnumeric, ischar, islogical, iscell, isstruct,
///             isreal, isnan, isinf, isfinite
///   reduce:   sum, prod, mean
///   text:     class           (always non-uniform — string output)
/// Custom (anonymous) handles route through `Engine::callFunctionHandle`
/// when an Engine pointer is supplied. Without an Engine, custom handles
/// throw `m:cellfun:fnUnsupported`.
/// Default uniformOutput=true packs scalars into a numeric/logical array
/// of the same shape as `C`. uniformOutput=false packs into a cell array
/// of the same shape.
Value cellfun(const Value &fn, const Value &c, bool uniformOutput, Engine *engine = nullptr, std::pmr::memory_resource *mr = nullptr);

//// num2cell(A) — wrap each element of A in a scalar cell.
Value num2cell(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// cell2mat(C) — concatenate cells back into a single matrix. Fast
/// path for cell of DOUBLE scalars; general case horzcat-then-vertcat.
Value cell2mat(const Value &c, std::pmr::memory_resource *mr = nullptr);

/// iscellstr(C) — true iff C is a cell whose every entry is a char row.
Value iscellstr(const Value &c, std::pmr::memory_resource *mr = nullptr);

/// cellstr(s) — char row → 1×1 cell; string array → N×1 cell;
/// cell-of-strings → identity.
Value cellstr(const Value &x, std::pmr::memory_resource *mr = nullptr);

//// mat2cell(A, R)        — vector input: split into a 1×N cell where
////                         element i is A[..., R(i)].
//// mat2cell(A, R, C)     — 2-D input: split rows by R and cols by C.
//// sum(R) == size(A,1), sum(C) == size(A,2). Block at (i, j) has
//// shape R(i) × C(j).
Value mat2cell(const Value &x, const double *rowSizes, size_t nRow, const double *colSizes, size_t nCol, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
