#include "MFitLibrary.hpp"

namespace numkit {

void FitLibrary::install(Engine &engine)
{
    registerInterpFunctions(engine);
}

} // namespace numkit
