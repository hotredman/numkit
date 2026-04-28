// libs/builtin/include/numkit/m/builtin/datatypes/struct/struct.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MSpan.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m { class Engine; }

namespace numkit::m::builtin {

using ::numkit::m::Engine;

// ── Struct ────────────────────────────────────────────────────────────
/// Empty struct scalar. Named `structure` in C++ because `struct` is a
/// keyword (the MATLAB registered name remains `struct`).
MValue structure(Allocator &alloc);

/// Build a struct from alternating {name, value, name, value, ...} pairs.
/// Odd arg count silently drops the trailing unmatched name. Non-char
/// names throw MError.
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

// ── structfun ─────────────────────────────────────────────────────────
//
// Apply a function handle to each field of `S`. Built-in fast-path set
// matches cellfun (see datatypes/cell/cell.hpp). uniformOutput=true
// produces a column vector of length numFields; false → 1×N cell row.
MValue structfun(Allocator &alloc, const MValue &fn, const MValue &s,
                 bool uniformOutput, Engine *engine = nullptr);

} // namespace numkit::m::builtin
