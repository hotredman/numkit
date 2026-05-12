// libs/builtin/include/numkit/builtin/language/structures/struct.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

namespace numkit { class Engine; }

namespace numkit::builtin {

using ::numkit::Engine;

//// Empty struct scalar. Named `structure` in C++ because `struct` is a
//// keyword (the MATLAB registered name remains `struct`).
Value structure(std::pmr::memory_resource *mr = nullptr);

/// Build a struct from alternating {name, value, name, value, ...} pairs.
/// Odd arg count silently drops the trailing unmatched name. Non-char
/// names throw Error.
Value structure(Span<const Value> nameValuePairs, std::pmr::memory_resource *mr = nullptr);

/// Return 1-column cell array of struct field names (insertion order).
/// Throws Error if s is not a struct.
Value fieldnames(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// Logical scalar: does s contain a field named `name`?
/// Returns false (not error) if s is not a struct.
Value isfield(const Value &s, const Value &name, std::pmr::memory_resource *mr = nullptr);

/// Copy of s with field `name` removed. Throws Error if s is not a struct.
/// Silently ignores missing field names.
Value rmfield(const Value &s, const Value &name, std::pmr::memory_resource *mr = nullptr);

/// Apply a function handle to each field of `S`. Built-in fast-path set
/// matches cellfun (see datatypes/cell/cell.hpp). uniformOutput=true
/// produces a column vector of length numFields; false → 1×N cell row.
Value structfun(const Value &fn, const Value &s, bool uniformOutput, Engine *engine = nullptr, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
