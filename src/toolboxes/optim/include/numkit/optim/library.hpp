#pragma once

namespace numkit { class Engine; }  // fwd-decl — keep this public header core-free

namespace numkit {

class OptimLibrary
{
public:
    static void install(Engine &engine);
};

} // namespace numkit
