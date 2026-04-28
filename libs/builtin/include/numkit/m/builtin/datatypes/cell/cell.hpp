// libs/builtin/include/numkit/m/builtin/datatypes/cell/cell.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m { class Engine; }

namespace numkit::m::builtin {

using ::numkit::m::Engine;

// ── Cell construction ─────────────────────────────────────────────────
/// cell(n) — n×n cell array. MATLAB behavior.
MValue cell(Allocator &alloc, size_t n);

/// cell(r, c) — r×c cell array.
MValue cell(Allocator &alloc, size_t rows, size_t cols);

/// cell(r, c, p) — 3D cell array when p > 0; else 2D r×c.
MValue cell(Allocator &alloc, size_t rows, size_t cols, size_t pages);

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
MValue cellfun(Allocator &alloc, const MValue &fn, const MValue &c,
               bool uniformOutput, Engine *engine = nullptr);

} // namespace numkit::m::builtin
