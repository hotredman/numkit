#pragma once

#include "MLabEngine.hpp"

namespace mlab {

class FitLibrary
{
public:
    static void install(Engine &engine);

private:
    static void registerInterpFunctions(Engine &engine);
};

} // namespace mlab
