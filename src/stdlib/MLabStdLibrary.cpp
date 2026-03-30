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

    PlotLibrary::install(engine);

    // --- arrayfun (basic scalar version) ---
    engine.registerFunction("arrayfun",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                if (args.size() < 2)
                                    throw std::runtime_error(
                                        "arrayfun requires at least 2 arguments");
                                return {args[1]};
                            });
}

} // namespace mlab
