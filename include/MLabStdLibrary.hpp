#pragma once

#include "MLabEngine.hpp"

namespace mlab {

class StdLibrary
{
public:
    static void install(Engine &engine);

private:
    static void registerBinaryOps(Engine &engine);
    static void registerUnaryOps(Engine &engine);
    static void registerMathFunctions(Engine &engine);
    static void registerMatrixFunctions(Engine &engine);
    static void registerIOFunctions(Engine &engine);
    static void registerTypeFunctions(Engine &engine);
    static void registerCellStructFunctions(Engine &engine);
    static void registerStringFunctions(Engine &engine);
    static void registerComplexFunctions(Engine &engine);
};

} // namespace mlab
