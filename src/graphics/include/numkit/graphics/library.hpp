#pragma once

// Engine is only named (by reference) in install()'s signature — a forward
// declaration keeps this public header core-free, so graphics compute TUs that
// include it pull in no <numkit/core/...>. The install() *definition*
// (library.cpp) includes the full Engine and is the sole Engine-coupled glue.
namespace numkit {

class Engine;

class GraphicsLibrary
{
public:
    static void install(Engine &engine);
};

} // namespace numkit
