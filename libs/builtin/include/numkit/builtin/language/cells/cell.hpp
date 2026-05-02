// libs/builtin/include/numkit/builtin/language/cells/cell.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit { class Engine; }

namespace numkit::builtin {

using ::numkit::Engine;

// ── Cell construction ─────────────────────────────────────────────────
/// cell(n) — n×n cell array. MATLAB behavior.
Value cell(std::pmr::memory_resource *mr, size_t n);

/// cell(r, c) — r×c cell array.
Value cell(std::pmr::memory_resource *mr, size_t rows, size_t cols);

/// cell(r, c, p) — 3D cell array when p > 0; else 2D r×c.
Value cell(std::pmr::memory_resource *mr, size_t rows, size_t cols, size_t pages);

// ── cellfun ───────────────────────────────────────────────────────────
//
// Apply a function handle to each cell of `C`.
// Built-in handles supported (fast path via funcHandleName()):
//
//   shape:    numel, length, ndims, isempty
//   type:     isnumeric, ischar, islogical, iscell, isstruct,
//             isreal, isnan, isinf, isfinite
//   reduce:   sum, prod, mean
//   text:     class           (always non-uniform — string output)
//
// Custom (anonymous) handles route through `Engine::callFunctionHandle`
// when an Engine pointer is supplied. Without an Engine, custom handles
// throw `m:cellfun:fnUnsupported`.
//
// Default uniformOutput=true packs scalars into a numeric/logical array
// of the same shape as `C`. uniformOutput=false packs into a cell array
// of the same shape.
Value cellfun(std::pmr::memory_resource *mr, const Value &fn, const Value &c,
               bool uniformOutput, Engine *engine = nullptr);

// ── Pack 15: cell idioms ──────────────────────────────────────────────
/// num2cell(A) — wrap each element of A in a scalar cell.
Value num2cell(std::pmr::memory_resource *mr, const Value &x);

/// cell2mat(C) — concatenate cells back into a single matrix. Fast
/// path for cell of DOUBLE scalars; general case horzcat-then-vertcat.
Value cell2mat(std::pmr::memory_resource *mr, const Value &c);

/// iscellstr(C) — true iff C is a cell whose every entry is a char row.
Value iscellstr(std::pmr::memory_resource *mr, const Value &c);

/// cellstr(s) — char row → 1×1 cell; string array → N×1 cell;
/// cell-of-strings → identity.
Value cellstr(std::pmr::memory_resource *mr, const Value &x);

} // namespace numkit::builtin
