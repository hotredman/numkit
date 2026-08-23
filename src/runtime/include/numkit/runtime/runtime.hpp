// runtime/include/numkit/runtime/runtime.hpp
//
// Language-runtime layer (L2, engine-coupled). These are NOT math/io toolbox
// functions — they are the scripting runtime itself (the eval-family today;
// the workspace who/whos/clear/clearvars/exist/assignin/inputname and import
// clusters land here as the runtime extraction proceeds). Extracted out of
// toolboxes/builtin so the math/io toolboxes stay free of engine-runtime glue.
// Wired by bundle/installStandardLibrary (NOT by builtin).
#pragma once

namespace numkit {
class Engine;
}

namespace numkit::runtime {

/// @brief Register the language-runtime builtins.
///
/// Currently the eval-family (`run` / `eval` / `evalin`). Engine-coupled (L2);
/// called once from `installStandardLibrary` after the toolbox installs.
///
/// @param engine  Engine to register the runtime builtins on.
void installRuntimeLibrary(Engine &engine);
void registerWorkspaceRuntime(Engine &engine);

} // namespace numkit::runtime
