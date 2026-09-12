/// @file library.hpp
/// @ingroup group_comm
// toolboxes/comm/include/nk/comm/library.hpp
//
// Communications builtins — function-form only, no System Object
// hierarchy.

#pragma once

namespace nk {

/// @addtogroup group_comm
/// @{
 class Engine; }  // fwd-decl — keep this public header core-free

namespace nk {

class CommLibrary
{
public:
    static void install(Engine &engine);
};


/// @}
} // namespace nk
