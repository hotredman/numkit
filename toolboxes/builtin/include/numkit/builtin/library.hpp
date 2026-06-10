#pragma once

#include <numkit/core/engine.hpp>

namespace numkit {

class BuiltinLibrary
{
public:
    static void install(Engine &engine);

private:
    // Category registrators (implemented in separate TUs)
    static void registerBinaryOps(Engine &engine);
    static void registerUnaryOps(Engine &engine);
    static void registerTypeFunctions(Engine &engine);
    static void registerCellStructFunctions(Engine &engine);
    static void registerStringFunctions(Engine &engine);
    static void registerComplexFunctions(Engine &engine);
    // Key-value container classes: dictionary + containers.Map (object model).
    static void registerContainers(Engine &engine);

    // Workspace / session builtins (clear, who, whos, tic, toc, etc.)
    static void registerWorkspaceBuiltins(Engine &engine);
};

} // namespace numkit

// ── Phase 3-A step C4 transitional namespace-compatibility shim ──────────────
// The relocated math/ + lang/ compute is being renamed numkit::builtin ->
// numkit::math / numkit::lang area-by-area. These using-directives keep every
// existing `numkit::builtin::<fn>` reference — the ~185 cross-toolbox call-sites
// AND the in-place *_reg adapters that still live in numkit::builtin(::detail) —
// compiling UNCHANGED during the migration. Removed in step C4c once the
// call-sites are retargeted. library.hpp is included broadly, so the shim is
// visible wherever relocated builtin math/lang names are referenced.
namespace numkit { namespace math {} namespace lang {} }  // ensure both declared
namespace numkit::builtin {
    using namespace numkit::math;
    using namespace numkit::lang;
}