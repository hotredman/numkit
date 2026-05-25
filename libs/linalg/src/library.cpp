// libs/linalg/src/library.cpp
//
// Registration hub for the Linear Algebra Toolbox.
//
// Naming convention:
//   Each function is registered under   `linalg.<sub>.<name>`
//   and aliased into                    `compat.<name>`.
// The `compat.<name>` alias keeps every script that does
//   import compat.*
//   y = svd(A);
// working without code changes — the same shape libs/{stats,signal}
// adopted when they moved out of libs/builtin.
//
// SKELETON: as functions migrate from libs/builtin/src/language/arrays/
// into libs/linalg/src/, they are forward-declared in this file and
// registered in install(). The skeleton intentionally registers nothing
// today — that lands per migration group (vector ops, norms, …).

#include <numkit/linalg/library.hpp>
#include <numkit/core/types.hpp>

namespace numkit {

void LinalgLibrary::install(Engine & /*engine*/)
{
    // Per-group registration blocks land here as functions migrate.
    // Pattern (mirrors libs/stats/src/library.cpp):
    //
    //   auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
    //       engine.registerFunction(std::string("linalg.") + sub, name, fn);
    //       engine.registerFunction("compat", name, fn);
    //   };
    //   reg("vector",  "cross",  &linalg::detail::cross_reg);
    //   reg("vector",  "dot",    &linalg::detail::dot_reg);
    //   reg("vector",  "kron",   &linalg::detail::kron_reg);
    //   …
}

} // namespace numkit
