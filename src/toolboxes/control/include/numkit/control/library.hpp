// toolboxes/control/include/numkit/control/library.hpp
//
// Control-system builtins — function-form only. LTI systems are plain
// numkit struct values with a `kind` tag ('tf' / 'zpk' / 'ss'),
// matching the user's "supercalculator, no OOP" rule.

#pragma once

#include <numkit/core/engine.hpp>

namespace numkit {

class ControlLibrary
{
public:
    static void install(Engine &engine);
};

} // namespace numkit
