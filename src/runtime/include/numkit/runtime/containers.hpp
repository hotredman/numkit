/// @file containers.hpp
/// @ingroup group_datatypes
// src/runtime/include/numkit/runtime/containers.hpp
//
// Public C++ API for the key–value container objects (`containers.Map`, `dictionary`).
#pragma once

#include <numkit/value/value.hpp>

#include <cstddef>
#include <memory_resource>

namespace numkit {
class Engine;
}

namespace numkit::runtime {

void registerContainersRuntime(Engine &engine);

namespace containers {

// ── Constructors ─────────────────────────────────────────────
// Empty containers.Map (handle semantics: copies alias shared state).
Value map(std::pmr::memory_resource *mr = nullptr);
// Empty dictionary (value semantics: copies are independent).
Value dictionary(std::pmr::memory_resource *mr = nullptr);

// ── Operations (work on either container) ────────────────────
// Insert or update key -> val. Mutates `m` in place.
void set(Value &m, const Value &key, const Value &val);
// Look up key; throws if absent.
Value get(const Value &m, const Value &key);
bool isKey(const Value &m, const Value &key);
void remove(Value &m, const Value &key);
std::size_t count(const Value &m);
// Keys / values as arrays (string|double column, or cell for mixed).
Value keys(const Value &m, std::pmr::memory_resource *mr = nullptr);
Value values(const Value &m, std::pmr::memory_resource *mr = nullptr);

} // namespace containers
} // namespace numkit::runtime

namespace numkit::containers {
using namespace ::numkit::runtime::containers;
}
