// libs/builtin/include/numkit/builtin/datatypes/struct/struct.hpp
#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

namespace numkit { class Engine; }

namespace numkit::builtin {

using ::numkit::Engine;

// ── Struct ────────────────────────────────────────────────────────────
/// Empty struct scalar. Named `structure` in C++ because `struct` is a
/// keyword (the MATLAB registered name remains `struct`).
Value structure(Allocator &alloc);

/// Build a struct from alternating {name, value, name, value, ...} pairs.
/// Odd arg count silently drops the trailing unmatched name. Non-char
/// names throw Error.
Value structure(Allocator &alloc, Span<const Value> nameValuePairs);

/// Return 1-column cell array of struct field names (insertion order).
/// Throws Error if s is not a struct.
Value fieldnames(Allocator &alloc, const Value &s);

/// Logical scalar: does s contain a field named `name`?
/// Returns false (not error) if s is not a struct.
Value isfield(Allocator &alloc, const Value &s, const Value &name);

/// Copy of s with field `name` removed. Throws Error if s is not a struct.
/// Silently ignores missing field names.
Value rmfield(Allocator &alloc, const Value &s, const Value &name);

// ── structfun ─────────────────────────────────────────────────────────
//
// Apply a function handle to each field of `S`. Built-in fast-path set
// matches cellfun (see datatypes/cell/cell.hpp). uniformOutput=true
// produces a column vector of length numFields; false → 1×N cell row.
Value structfun(Allocator &alloc, const Value &fn, const Value &s,
                 bool uniformOutput, Engine *engine = nullptr);

} // namespace numkit::builtin
