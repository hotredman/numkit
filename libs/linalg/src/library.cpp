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
// Functions land here per migration group as they move out of
// libs/builtin/src/language/arrays/.

#include <numkit/linalg/library.hpp>
#include <numkit/core/types.hpp>

namespace numkit::linalg::detail {
// vector_ops.cpp
void cross_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void dot_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void kron_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
} // namespace numkit::linalg::detail

namespace numkit {

void LinalgLibrary::install(Engine &engine)
{
    // Mirror libs/stats/library.cpp pattern: every function lives at
    // linalg.<sub>.<name> AND is aliased into compat.<name>.
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("linalg.") + sub, name, fn);
        engine.registerFunction("compat", name, fn);
    };

    // ── Vector ops ───────────────────────────────────────────────
    reg("vector", "cross", &linalg::detail::cross_reg);
    reg("vector", "dot",   &linalg::detail::dot_reg);
    reg("vector", "kron",  &linalg::detail::kron_reg);
}

} // namespace numkit
