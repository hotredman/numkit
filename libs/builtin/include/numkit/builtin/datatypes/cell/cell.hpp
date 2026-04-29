// libs/builtin/include/numkit/builtin/datatypes/cell/cell.hpp
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

} // namespace numkit::builtin
