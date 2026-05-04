// libs/comm/include/numkit/comm/library.hpp
//
// Communications Toolbox builtins. Mirrors MATLAB's documentation root
// `/help/comm/`. Function-form only — no System Object hierarchy.

#pragma once

#include <numkit/core/engine.hpp>

namespace numkit {

class CommLibrary
{
public:
    static void install(Engine &engine);
};

} // namespace numkit
