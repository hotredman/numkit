// toolboxes/builtin/include/numkit/builtin/containers.hpp
//
// Public, engine-free C++ API for the key–value container objects
// (`containers.Map`, `dictionary`). Mirrors the rest of toolboxes/builtin:
// free functions over a `Value` + a `std::pmr::memory_resource *` — no
// Engine needed. The objects are plain `Value`s of ValueType::OBJECT,
// so they round-trip through the engine (setVariable / eval / return
// from a builtin); the class only needs to be *registered* in an Engine
// for SCRIPT-side dispatch (m('a')), which BuiltinLibrary::install does.
//
// The interpreter's registry hooks (subsref / subsasgn / methods) are
// thin adapters over exactly these functions — one source of truth.
#pragma once

#include <numkit/value/value.hpp>

#include <cstddef>
#include <memory_resource>

namespace numkit::containers {

// ── Constructors ─────────────────────────────────────────────
// Empty containers.Map (handle semantics: copies alias shared state).
Value map(std::pmr::memory_resource *mr = nullptr);
// Empty dictionary (value semantics: copies are independent).
Value dictionary(std::pmr::memory_resource *mr = nullptr);

// ── Operations (work on either container) ────────────────────
// Insert or update key → val. Mutates `m` in place (the value/handle
// COW rule applies automatically: a dictionary copy stays independent).
void set(Value &m, const Value &key, const Value &val);
// Look up key; throws if absent.
Value get(const Value &m, const Value &key);
bool isKey(const Value &m, const Value &key);
void remove(Value &m, const Value &key);
std::size_t count(const Value &m);
// Keys / values as arrays (string|double column, or cell for mixed).
Value keys(const Value &m, std::pmr::memory_resource *mr = nullptr);
Value values(const Value &m, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::containers
