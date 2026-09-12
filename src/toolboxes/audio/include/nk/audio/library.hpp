/// @file library.hpp
/// @ingroup group_audio
// toolboxes/audio/include/nk/audio/library.hpp
//
// Audio builtins — function-form only, no System Object hierarchy.

#pragma once

namespace nk {

/// @addtogroup group_audio
/// @{
 class Engine; }  // fwd-decl — keep this public header core-free

namespace nk {

class AudioLibrary
{
public:
    static void install(Engine &engine);
};


/// @}
} // namespace nk
