#include "MDspLibrary.hpp"

namespace numkit {

void DspLibrary::install(Engine &engine)
{
    registerSignalCoreFunctions(engine);
    registerConvolutionFunctions(engine);
    registerWindowFunctions(engine);
    registerFilterFunctions(engine);
    registerFilterDesignFunctions(engine);
    registerSpectralFunctions(engine);
    registerResampleFunctions(engine);
    registerTransformFunctions(engine);
}

} // namespace numkit
