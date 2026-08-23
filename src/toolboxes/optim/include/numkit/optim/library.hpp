/// @file library.hpp
/// @ingroup group_optim
#pragma once

namespace numkit {

/// @addtogroup group_optim
/// @{
 class Engine; }  // fwd-decl — keep this public header core-free

namespace numkit {

class OptimLibrary
{
public:
    static void install(Engine &engine);
};


/// @}
} // namespace numkit
