// src/bundle/include/numkit/bundle/builtin_library.hpp
//
// Registration hub for the Standard Builtin Library on an Engine instance.
#pragma once

namespace numkit {
class Engine;

class BuiltinLibrary {
public:
    static void install(Engine &engine);
    static void registerWorkspaceBuiltins(Engine &engine);
    static void registerBinaryOps(Engine &engine);
    static void registerUnaryOps(Engine &engine);
};

} // namespace numkit