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
// norms.cpp
void norm_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void vecnorm_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
// properties.cpp
void inv_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void trace_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void det_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void rank_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void cond_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void normest_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void rcond_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
} // namespace numkit::linalg::detail

namespace numkit {

void LinalgLibrary::install(Engine &engine)
{
    // Every function lands in THREE places:
    //   1. Bare global name `<name>`           — matches MATLAB, which
    //      exposes the entire linalg surface unqualified (`eig(A)` works
    //      with no import). Also what BuiltinLibrary did for these
    //      functions before they migrated here.
    //   2. Namespaced `linalg.<sub>.<name>`    — addressable explicitly
    //      after `import linalg.*` or as a fully qualified call.
    //   3. `compat.<name>`                     — picked up by the standard
    //      `import compat.*` test fixture (libs/{stats,signal} pattern).
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(name, fn);                                    // 1
        engine.registerFunction(std::string("linalg.") + sub, name, fn);      // 2
        engine.registerFunction("compat", name, fn);                          // 3
    };

    // ── Vector ops ───────────────────────────────────────────────
    reg("vector", "cross", &linalg::detail::cross_reg);
    reg("vector", "dot",   &linalg::detail::dot_reg);
    reg("vector", "kron",  &linalg::detail::kron_reg);

    // ── Norms ────────────────────────────────────────────────────
    reg("norm", "norm",    &linalg::detail::norm_reg);
    reg("norm", "vecnorm", &linalg::detail::vecnorm_reg);

    // ── Properties ───────────────────────────────────────────────
    reg("prop", "inv",     &linalg::detail::inv_reg);
    reg("prop", "trace",   &linalg::detail::trace_reg);
    reg("prop", "det",     &linalg::detail::det_reg);
    reg("prop", "rank",    &linalg::detail::rank_reg);
    reg("prop", "cond",    &linalg::detail::cond_reg);
    reg("prop", "normest", &linalg::detail::normest_reg);
    reg("prop", "rcond",   &linalg::detail::rcond_reg);
}

} // namespace numkit
