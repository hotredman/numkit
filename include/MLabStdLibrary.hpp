#pragma once

#include "MLabEngine.hpp"

namespace mlab {

class StdLibrary
{
public:
    static void install(Engine &engine);

private:
    // Category registrators (implemented in separate TUs)
    static void registerBinaryOps(Engine &engine);
    static void registerUnaryOps(Engine &engine);
    static void registerMathFunctions(Engine &engine);
    static void registerMatrixFunctions(Engine &engine);
    static void registerIOFunctions(Engine &engine);
    static void registerTypeFunctions(Engine &engine);
    static void registerCellStructFunctions(Engine &engine);
    static void registerStringFunctions(Engine &engine);
    static void registerComplexFunctions(Engine &engine);
    static void registerSignalCoreFunctions(Engine &engine);
    static void registerConvolutionFunctions(Engine &engine);
    static void registerWindowFunctions(Engine &engine);
    static void registerFilterFunctions(Engine &engine);
    static void registerFilterDesignFunctions(Engine &engine);
    static void registerSpectralFunctions(Engine &engine);
    static void registerResampleFunctions(Engine &engine);
    static void registerTransformFunctions(Engine &engine);
    static void registerInterpFunctions(Engine &engine);

    // Workspace / session builtins (clear, who, whos, tic, toc, etc.)
    static void registerWorkspaceBuiltins(Engine &engine);
};

} // namespace mlab