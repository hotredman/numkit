#pragma once

#include <numkit/m/core/MEngine.hpp>

namespace numkit::m {

class StdLibrary
{
public:
    static void install(Engine &engine);

private:
    // Category registrators (implemented in separate TUs)
    static void registerBinaryOps(Engine &engine);
    static void registerUnaryOps(Engine &engine);
    static void registerTypeFunctions(Engine &engine);
    static void registerCellStructFunctions(Engine &engine);
    static void registerStringFunctions(Engine &engine);
    static void registerComplexFunctions(Engine &engine);

    // Workspace / session builtins (clear, who, whos, tic, toc, etc.)
    static void registerWorkspaceBuiltins(Engine &engine);
};

} // namespace numkit::m