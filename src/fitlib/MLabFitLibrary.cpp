#include "MLabFitLibrary.hpp"

namespace mlab {

void FitLibrary::install(Engine &engine)
{
    registerInterpFunctions(engine);
}

} // namespace mlab
