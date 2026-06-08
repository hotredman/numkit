// libs/ode/src/library.cpp
//
// Registers all ODE-toolbox functions under ode.<name> and the flat
// compat.<name> alias.

#include <numkit/ode/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

namespace numkit::ode::detail {

void ode45_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ode23_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void odeset_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void odeget_reg(Span<const Value>, size_t, Span<Value>, CallContext &);

} // namespace numkit::ode::detail

namespace numkit::ode {
// Defined in ode45.cpp / ode23.cpp — install the `.m` solver wrappers (pausable
// RHS; the C++ `Value ode45/ode23(...)` APIs remain the synchronous embedder
// path).
void registerOde45M(Engine &engine);
void registerOde23M(Engine &engine);
} // namespace numkit::ode

namespace numkit {

void OdeLibrary::install(Engine &engine)
{
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("ode.") + sub, name, fn);
        engine.registerFunction("compat", name, fn);
    };

    // ode45 is an embedded `.m` wrapper (VM_CALLBACKS_PLAN.md): the RHS f(t,y)
    // is called from bytecode and is pausable under the debugger. Registered as
    // a top-level user function (shadows externals on both backends), so no
    // ode.solvers/compat external alias is needed — the C++ `Value ode45(...)`
    // API (ode45_reg) is retained as the synchronous embedder path.
    ode::registerOde45M(engine);
    ode::registerOde23M(engine);   // embedded `.m` wrapper (pausable RHS)
    reg("options", "odeset", &ode::detail::odeset_reg);
    reg("options", "odeget", &ode::detail::odeget_reg);
}

} // namespace numkit
