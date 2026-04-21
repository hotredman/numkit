#pragma once

#include <numkit/m/core/MEngine.hpp>

namespace numkit::m {

class DspLibrary
{
public:
    static void install(Engine &engine);

private:
    static void registerSignalCoreFunctions(Engine &engine);
    static void registerWindowFunctions(Engine &engine);
    static void registerFilterFunctions(Engine &engine);
    static void registerFilterDesignFunctions(Engine &engine);
    static void registerSpectralFunctions(Engine &engine);
    static void registerResampleFunctions(Engine &engine);
    static void registerTransformFunctions(Engine &engine);
};

} // namespace numkit::m
