/// @file library.hpp
/// @ingroup group_comm
// toolboxes/comm/include/numkit/comm/library.hpp
//
// Communications builtins — function-form only, no System Object
// hierarchy.

#pragma once

namespace numkit { class Engine; }  // fwd-decl — keep this public header core-free

namespace numkit {

class CommLibrary
{
public:
    static void install(Engine &engine);
};

} // namespace numkit
