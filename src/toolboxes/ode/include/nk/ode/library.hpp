/// @file library.hpp
/// @ingroup group_ode
// toolboxes/ode/include/nk/ode/library.hpp
//
// Ordinary Differential Equations Toolbox. Mirrors MATLAB's `ode*` and
// `odeset` / `odeget` API surface for initial-value problems.

#pragma once

namespace nk {

/// @addtogroup group_ode
/// @{
 class Engine; }  // fwd-decl — keep this public header core-free

namespace nk {

class OdeLibrary
{
public:
    /// Register every ODE function under the `ode.<name>` namespace and
    /// alias each into `compat.<name>` (so scripts using `import compat.*`
    /// can call them flat).
    static void install(Engine &engine);
};

/// @}
} // namespace nk
