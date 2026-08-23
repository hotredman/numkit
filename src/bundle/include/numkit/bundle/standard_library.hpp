/// @file standard_library.hpp
/// @ingroup group_matlab
// src/bundle/include/numkit/bundle/standard_library.hpp
//
// The composition layer for NumKit. StandardLibrary::install wires all
// standard subsystems (Runtime, Builtin, and all Toolboxes) into an Engine instance.
#pragma once

#include <memory>
#include <memory_resource>

namespace numkit {

class Engine;

class StandardLibrary {
public:
    static void install(Engine &engine);
};

/// @brief Forwarder for backward compatibility.
inline void installStandardLibrary(Engine &engine) {
    StandardLibrary::install(engine);
}

// Convenience: construct an Engine on the heap with the standard library
// already installed. Returned by unique_ptr because Engine is non-movable.
std::unique_ptr<Engine> makeStandardEngine(std::pmr::memory_resource *mr = nullptr);

} // namespace numkit
