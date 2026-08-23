// src/bundle/src/standard_library.cpp
//
// StandardLibrary::install — the one place that wires all toolboxes and core runtime.

#include <numkit/bundle/standard_library.hpp>

#include <numkit/core/engine.hpp>

#include <numkit/runtime/runtime.hpp>
#include <numkit/bundle/builtin_library.hpp>
#include <numkit/linalg/library.hpp>
#include <numkit/signal/library.hpp>
#include <numkit/stats/library.hpp>
#include <numkit/image/library.hpp>
#include <numkit/comm/library.hpp>
#include <numkit/wavelet/library.hpp>
#include <numkit/control/library.hpp>
#include <numkit/graphics/library.hpp>
#include <numkit/io/library.hpp>
#include <numkit/optim/library.hpp>
#include <numkit/bundle/help.hpp>
#include <numkit/audio/library.hpp>
#include <numkit/ode/library.hpp>

namespace numkit {

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

} // namespace numkit
