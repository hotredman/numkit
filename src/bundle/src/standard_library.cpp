// src/bundle/src/standard_library.cpp
//
// StandardLibrary::install — the one place that wires all toolboxes and core runtime.

#include <nk/bundle/standard_library.hpp>

#include <nk/core/engine.hpp>

#include <nk/runtime/runtime.hpp>
#include <nk/bundle/builtin_library.hpp>
#include <nk/linalg/library.hpp>
#include <nk/signal/library.hpp>
#include <nk/stats/library.hpp>
#include <nk/image/library.hpp>
#include <nk/comm/library.hpp>
#include <nk/wavelet/library.hpp>
#include <nk/control/library.hpp>
#include <nk/graphics/library.hpp>
#include <nk/io/library.hpp>
#include <nk/optim/library.hpp>
#include <nk/bundle/help.hpp>
#include <nk/audio/library.hpp>
#include <nk/ode/library.hpp>

namespace nk {

// Defined in register/fusion/fused_rules.cpp — registers element-wise fusion
// rules (idiom -> ops kernel) on the engine's FusionRule registry.
void registerFusionRules(Engine &engine);

void StandardLibrary::install(Engine &engine)
{
    // 1. Language runtime
    RuntimeLibrary::install(engine);

    // 2. Builtin standard algorithms
    BuiltinLibrary::install(engine);

    // 3. Toolboxes
    LinalgLibrary::install(engine);
    SignalLibrary::install(engine);
    StatsLibrary::install(engine);
    ImageLibrary::install(engine);
    CommLibrary::install(engine);
    WaveletLibrary::install(engine);
    ControlLibrary::install(engine);
    GraphicsLibrary::install(engine);
    IoLibrary::install(engine);
    OptimLibrary::install(engine);
    AudioLibrary::install(engine);
    OdeLibrary::install(engine);

    // 4. Help & Discovery system
    bundle::HelpLibrary::install(engine);

    // Element-wise fusion rules — registered last, after all builtins exist.
    registerFusionRules(engine);
}

std::unique_ptr<Engine> makeStandardEngine(std::pmr::memory_resource *mr)
{
    auto engine = std::make_unique<Engine>(mr);
    StandardLibrary::install(*engine);
    return engine;
}

} // namespace nk
