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

/// @addtogroup group_datatypes
/// @{

class Engine;
}

namespace numkit::runtime {

/// @brief Registers `containers.Map` and `dictionary` builtins in the engine.
/// @param engine Engine instance.
void registerContainersRuntime(Engine &engine);

namespace containers {

// ── Constructors ─────────────────────────────────────────────

/// @brief Creates an empty `containers.Map` (handle semantics: copies alias shared state).
/// @param mr Memory resource for container state.
/// @return Container Value object.
/// @see dictionary
Value map(std::pmr::memory_resource *mr = nullptr);

/// @brief Creates an empty `dictionary` (value semantics: copies are independent).
/// @param mr Memory resource for container state.
/// @return Dictionary Value object.
/// @see map
Value dictionary(std::pmr::memory_resource *mr = nullptr);

// ── Operations (work on either container) ────────────────────

/// @brief Inserts or updates key-value pair in container in place.
/// @param m Container Value object.
/// @param key Key Value (string, scalar, or numeric).
/// @param val Value to store.
void set(Value &m, const Value &key, const Value &val);

/// @brief Looks up value associated with specified key (throws if key absent).
/// @param m Container Value object.
/// @param key Key Value to search.
/// @return Value associated with key.
/// @see isKey
Value get(const Value &m, const Value &key);

/// @brief Checks whether specified key exists in container.
/// @param m Container Value object.
/// @param key Key Value to search.
/// @return True if key exists, false otherwise.
/// @see get
bool isKey(const Value &m, const Value &key);

/// @brief Removes specified key and its associated value from container.
/// @param m Container Value object.
/// @param key Key Value to remove.
void remove(Value &m, const Value &key);

/// @brief Returns the number of key-value pairs in container.
/// @param m Container Value object.
/// @return Number of entries.
std::size_t count(const Value &m);

/// @brief Extracts all keys from container as a column array.
/// @param m Container Value object.
/// @param mr Memory resource for output allocation.
/// @return Keys array (string, numeric, or cell).
/// @see values
Value keys(const Value &m, std::pmr::memory_resource *mr = nullptr);

/// @brief Extracts all values from container as an array or cell array.
/// @param m Container Value object.
/// @param mr Memory resource for output allocation.
/// @return Values array (homogeneous array or cell).
/// @see keys
Value values(const Value &m, std::pmr::memory_resource *mr = nullptr);

} // namespace containers
} // namespace numkit::runtime

namespace numkit::containers {
using namespace ::numkit::runtime::containers;

/// @}
}
