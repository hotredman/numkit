// src/runtime/include/numkit/runtime/runtime.hpp
//
// Language-runtime layer (L2, engine-coupled). Scripting runtime execution,
// workspace commands, cells, structures, containers, diagnostics, I/O, and callbacks.
#pragma once

namespace numkit {
class Engine;

class RuntimeLibrary {
public:
    static void install(Engine &engine);
};

namespace runtime {

/// @brief Forwarder for backward compatibility.
void installRuntimeLibrary(Engine &engine);
void registerWorkspaceRuntime(Engine &engine);

} // namespace runtime
} // namespace numkit
