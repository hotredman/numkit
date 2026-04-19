#include "MFitLibrary.hpp"

namespace numkit::m {

void FitLibrary::install(Engine &engine)
{
    registerInterpFunctions(engine);
}

} // namespace numkit::m
