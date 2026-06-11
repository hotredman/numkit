// bundle/include/numkit/bundle/standard_library.hpp
//
// The composition layer. core (the Engine) is library-agnostic: a freshly
// constructed Engine has the language runtime + constants + primitive
// arithmetic, but ZERO named functions. installStandardLibrary() wires in the
// full MATLAB-compatible function set (every toolbox + the base builtins).
//
// Embedders choose their surface:
//   • full scripting environment → Engine e; installStandardLibrary(e);
//   • pure C++ numerics          → link only value/fs/ops + the toolboxes,
//                                  no Engine at all.

#pragma once

#include <memory>
#include <memory_resource>

namespace numkit {

class Engine;

// Register every standard library (builtin + all toolboxes) into `engine`.
// Call once, immediately after constructing the Engine.
void installStandardLibrary(Engine &engine);

// Convenience: construct an Engine on the heap with the standard library
// already installed. Returned by unique_ptr because Engine is non-movable.
std::unique_ptr<Engine> makeStandardEngine(std::pmr::memory_resource *mr = nullptr);

} // namespace numkit
