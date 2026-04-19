#pragma once

#include "MEngine.hpp"

namespace numkit {

class FitLibrary
{
public:
    static void install(Engine &engine);

private:
    static void registerInterpFunctions(Engine &engine);
};

} // namespace numkit
