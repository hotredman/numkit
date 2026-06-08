// core-libs/src/runtime/runtime.cpp
//
// Orchestrator for the core-libs language-runtime layer. installRuntimeLibrary is
// the single public entry (called once by bundle/installStandardLibrary); it
// composes the per-cluster registrars, each defined in its own translation unit
// (eval.cpp, workspace.cpp, …) as the extraction out of toolboxes/builtin proceeds.
#include <numkit/corelibs/runtime.hpp>

namespace numkit::corelibs {

// Per-cluster registrars (defined in sibling TUs).
void registerEvalFamily(Engine &engine);        // eval.cpp     — run / eval / evalin
void registerWorkspaceRuntime(Engine &engine);  // workspace.cpp — assignin / inputname / …

void installRuntimeLibrary(Engine &engine)
{
    registerEvalFamily(engine);
    registerWorkspaceRuntime(engine);
}

} // namespace numkit::corelibs
