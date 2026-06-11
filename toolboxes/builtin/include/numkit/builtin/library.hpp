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
    // Key-value container classes: dictionary + containers.Map (object model).
    static void registerContainers(Engine &engine);

    // Workspace / session builtins (clear, who, whos, tic, toc, etc.)
    static void registerWorkspaceBuiltins(Engine &engine);
};

} // namespace numkit

// ── Phase 3-A step C4 transitional namespace-compatibility shim ──────────────
// C4 relocated math/ + lang/ compute to numkit::math / numkit::lang. The
// transitional umbrella using-shim was removed in C4c — consumers now reference
// the qualified numkit::math|lang:: names (or carry a localized per-TU using).
namespace numkit { namespace math {} namespace lang {} }  // forward-declare both