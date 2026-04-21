#pragma once

#include <numkit/m/core/MEngine.hpp>

namespace numkit::m {

class DspLibrary
{
public:
    static void install(Engine &engine);

private:
    static void registerSignalCoreFunctions(Engine &engine);
};

} // namespace numkit::m
