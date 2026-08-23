// src/runtime/src/runtime.cpp
//
// RuntimeLibrary orchestrator. Registers language runtime, containers,
// cells, structures, reflection, and execution engines.

#include <numkit/runtime/runtime.hpp>

namespace numkit {

namespace runtime {

// Per-cluster registrars (defined in sibling TUs).
void registerEvalFamily(Engine &engine);              // eval.cpp
void registerWorkspaceRuntime(Engine &engine);        // workspace.cpp
void registerFunctionHandles(Engine &engine);         // function_handles.cpp
void registerContainersRuntime(Engine &engine);       // containers.cpp
void registerCellsRuntime(Engine &engine);            // cell.cpp
void registerStructuresRuntime(Engine &engine);       // struct.cpp
void registerEnvRuntime(Engine &engine);              // env.cpp
void registerReflectionRuntime(Engine &engine);       // reflection.cpp
void registerDiagnosticsRuntime(Engine &engine);      // diagnostics.cpp
void registerSplitapplyCallbackBuiltin(Engine &engine); // splitapply_callback.cpp

void installRuntimeLibrary(Engine &engine)
{
    RuntimeLibrary::install(engine);
}

} // namespace runtime

void RuntimeLibrary::install(Engine &engine)
{
    runtime::registerEvalFamily(engine);
    runtime::registerWorkspaceRuntime(engine);
    runtime::registerFunctionHandles(engine);
    runtime::registerContainersRuntime(engine);
    runtime::registerCellsRuntime(engine);
    runtime::registerStructuresRuntime(engine);
    runtime::registerEnvRuntime(engine);
    runtime::registerDiagnosticsRuntime(engine);
    runtime::registerReflectionRuntime(engine);
    runtime::registerSplitapplyCallbackBuiltin(engine);
}

} // namespace numkit
