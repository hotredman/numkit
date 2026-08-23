/// @file library.hpp
/// @ingroup group_control
// toolboxes/control/include/numkit/control/library.hpp
//
// Control-system builtins — function-form only. LTI systems are plain
// numkit struct values with a `kind` tag ('tf' / 'zpk' / 'ss'),
// matching the user's "supercalculator, no OOP" rule.

#pragma once

namespace numkit {

/// @addtogroup group_control
/// @{
 class Engine; }  // fwd-decl — keep this public header core-free

namespace numkit {

class ControlLibrary
{
public:
    static void install(Engine &engine);
};


/// @}
} // namespace numkit
