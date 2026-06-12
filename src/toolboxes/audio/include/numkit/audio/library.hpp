// toolboxes/audio/include/numkit/audio/library.hpp
//
// Audio builtins — function-form only, no System Object hierarchy.

#pragma once

namespace numkit { class Engine; }  // fwd-decl — keep this public header core-free

namespace numkit {

class AudioLibrary
{
public:
    static void install(Engine &engine);
};

} // namespace numkit
