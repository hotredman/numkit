#include "MLabStdLibrary.hpp"
#include "MLabPlotLibrary.hpp"

namespace mlab {

void StdLibrary::install(Engine &engine)
{
    registerBinaryOps(engine);
    registerUnaryOps(engine);
    registerMathFunctions(engine);
    registerMatrixFunctions(engine);
    registerIOFunctions(engine);
    registerTypeFunctions(engine);
    registerCellStructFunctions(engine);
    registerStringFunctions(engine);
    registerComplexFunctions(engine);
    registerSignalCoreFunctions(engine);
    registerConvolutionFunctions(engine);
    registerWindowFunctions(engine);
    registerFilterFunctions(engine);
    registerFilterDesignFunctions(engine);
    registerSpectralFunctions(engine);
    registerResampleFunctions(engine);
    registerTransformFunctions(engine);
    registerInterpFunctions(engine);

    PlotLibrary::install(engine);

    // --- arrayfun (basic scalar version) ---
    engine.registerFunction("arrayfun",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                if (args.size() < 2)
                                    throw std::runtime_error(
                                        "arrayfun requires at least 2 arguments");
                                return {args[1]};
                            });
}

} // namespace mlab