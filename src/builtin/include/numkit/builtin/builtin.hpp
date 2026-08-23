#pragma once

#include <numkit/core/engine.hpp>

namespace numkit {

class BuiltinLibrary {
public:
    static void install(Engine &engine);
    static void registerWorkspaceBuiltins(Engine &engine);
    static void registerBinaryOps(Engine &engine);
    static void registerUnaryOps(Engine &engine);
};

} // namespace numkit
