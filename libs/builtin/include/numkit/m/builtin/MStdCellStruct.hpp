// libs/builtin/include/numkit/m/builtin/MStdCellStruct.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MSpan.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m { class Engine; }

namespace numkit::m::builtin {

using ::numkit::m::Engine;

// ── Struct ───────────────────────────────────────────────────────────
/// Empty struct scalar. Named `structure` in C++ because `struct` is a
/// keyword (the MATLAB registered name remains `struct`).
MValue structure(Allocator &alloc);

/// Build a struct from alternating {name, value, name, value, ...} pairs.
/// Odd arg count silently drops the trailing unmatched name (matching
/// the existing MATLAB-facing behavior). Non-char names throw MError.
MValue structure(Allocator &alloc, Span<const MValue> nameValuePairs);

/// Return 1-column cell array of struct field names (insertion order).
/// Throws MError if s is not a struct.
MValue fieldnames(Allocator &alloc, const MValue &s);

/// Logical scalar: does s contain a field named `name`?
/// Returns false (not error) if s is not a struct.
MValue isfield(Allocator &alloc, const MValue &s, const MValue &name);

/// Copy of s with field `name` removed. Throws MError if s is not a struct.
/// Silently ignores missing field names.
MValue rmfield(Allocator &alloc, const MValue &s, const MValue &name);

// ── Cell ─────────────────────────────────────────────────────────────
/// cell(n) — n×n cell array. MATLAB behavior.
MValue cell(Allocator &alloc, size_t n);

/// cell(r, c) — r×c cell array.
MValue cell(Allocator &alloc, size_t rows, size_t cols);

/// cell(r, c, p) — 3D cell array when p > 0; else 2D r×c.
MValue cell(Allocator &alloc, size_t rows, size_t cols, size_t pages);

// ── cellfun / structfun ──────────────────────────────────────────────
//
// Apply a function handle to each cell of `C` (or each field of `S`).
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
// of the same shape as `C` (or a column vector of length numFields for
// structfun). uniformOutput=false packs into a cell array of the same
// shape (or a 1×N cell row for structfun).
MValue cellfun(Allocator &alloc, const MValue &fn, const MValue &c,
               bool uniformOutput, Engine *engine = nullptr);
MValue structfun(Allocator &alloc, const MValue &fn, const MValue &s,
                 bool uniformOutput, Engine *engine = nullptr);

} // namespace numkit::m::builtin
