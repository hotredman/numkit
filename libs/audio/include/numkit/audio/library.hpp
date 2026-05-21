// libs/audio/include/numkit/audio/library.hpp
//
// Audio builtins — function-form only, no System Object hierarchy.

#pragma once

#include <numkit/core/engine.hpp>

namespace numkit {

class AudioLibrary
{
public:
    static void install(Engine &engine);
};

} // namespace numkit
