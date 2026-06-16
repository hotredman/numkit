// bundle/src/standard_library.cpp
//
// installStandardLibrary — the one place that knows the full set of toolboxes.
// This is the ONLY translation unit that includes every <numkit/<lib>/library>
// header; core no longer does (its ctor is library-agnostic). Moving these
// installs here is what breaks the core → libs dependency.

#include <numkit/bundle/standard_library.hpp>

#include <numkit/core/engine.hpp>

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
#include <numkit/audio/library.hpp>
#include <numkit/ode/library.hpp>

#include <numkit/runtime/runtime.hpp>

namespace numkit {

// Defined in register/fusion/fused_rules.cpp — registers element-wise fusion
// rules (idiom → ops kernel) on the engine's FusionRule registry.
void registerFusionRules(Engine &engine);

void installStandardLibrary(Engine &engine)
{
    BuiltinLibrary::install(engine);
    runtime::installRuntimeLibrary(engine);  // L2 language runtime (eval-family; more to follow)
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

    // Element-wise fusion rules — registered last, after all builtins exist.
    registerFusionRules(engine);
}

std::unique_ptr<Engine> makeStandardEngine(std::pmr::memory_resource *mr)
{
    auto engine = std::make_unique<Engine>(mr);
    installStandardLibrary(*engine);
    return engine;
}

} // namespace numkit
