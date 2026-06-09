// runtime/src/runtime.cpp
//
// Orchestrator for the runtime language-runtime layer. installRuntimeLibrary is
// the single public entry (called once by bundle/installStandardLibrary); it
// composes the per-cluster registrars, each defined in its own translation unit
// (eval.cpp, workspace.cpp, …) as the extraction out of toolboxes/builtin proceeds.
#include <numkit/runtime/runtime.hpp>

namespace numkit::runtime {

// Per-cluster registrars (defined in sibling TUs).
void registerEvalFamily(Engine &engine);        // eval.cpp     — run / eval / evalin
void registerWorkspaceRuntime(Engine &engine);  // workspace.cpp — assignin / inputname / …
void registerFunctionHandles(Engine &engine);   // function_handles.cpp — str2func / func2str

void installRuntimeLibrary(Engine &engine)
{
    registerEvalFamily(engine);
    registerWorkspaceRuntime(engine);
    registerFunctionHandles(engine);
}

} // namespace numkit::runtime
