// libs/ode/src/library.cpp
//
// Registers all ODE-toolbox functions under ode.<name> and the flat
// compat.<name> alias.

#include <numkit/ode/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

namespace numkit::ode::detail {

void ode45_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void odeset_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void odeget_reg(Span<const Value>, size_t, Span<Value>, CallContext &);

} // namespace numkit::ode::detail

namespace numkit {

void OdeLibrary::install(Engine &engine)
{
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("ode.") + sub, name, fn);
        engine.registerFunction("compat", name, fn);
    };

    reg("solvers", "ode45",  &ode::detail::ode45_reg);
    reg("options", "odeset", &ode::detail::odeset_reg);
    reg("options", "odeget", &ode::detail::odeget_reg);
}

} // namespace numkit
