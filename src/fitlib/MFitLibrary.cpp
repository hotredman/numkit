#include "MFitLibrary.hpp"

namespace mlab {

void FitLibrary::install(Engine &engine)
{
    registerInterpFunctions(engine);
}

} // namespace mlab
