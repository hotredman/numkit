#include "MFitLibrary.hpp"

namespace numkit::m::m {

void FitLibrary::install(Engine &engine)
{
    registerInterpFunctions(engine);
}

} // namespace numkit::m::m
