/// @file library.hpp
/// @ingroup group_optim
#pragma once

namespace nk {

/// @addtogroup group_optim
/// @{
 class Engine; }  // fwd-decl — keep this public header core-free

namespace nk {

class OptimLibrary
{
public:
    static void install(Engine &engine);
};


/// @}
} // namespace nk
