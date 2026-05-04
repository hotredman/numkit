// libs/control/src/library.cpp
//
// Registration hub for the Control System Toolbox builtins.
// Namespace: control.<sub>.<name>; every function is also aliased
// into `compat.<name>` so MATLAB-style scripts can call them flat.

#include <numkit/control/library.hpp>

#include <numkit/core/types.hpp>

namespace numkit::control::detail {
// lti/lti.cpp
void tf_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void zpk_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void ss_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
// (Conversion entry points like tf2zp / zp2tf / tf2ss / ss2tf already
//  live in libs/builtin and libs/signal — we don't shadow them here.
//  The C++ implementations under conversion/conversion.cpp are kept
//  for internal composition by upcoming Wave-6 cycles, but we expose
//  them only via the libs/control C++ API, not as duplicated builtins.)
} // namespace numkit::control::detail

namespace numkit {

void ControlLibrary::install(Engine &engine)
{
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("control.") + sub, name, fn);
        engine.registerFunction("compat", name, fn);
    };

    reg("lti", "tf",  &control::detail::tf_reg);
    reg("lti", "zpk", &control::detail::zpk_reg);
    reg("lti", "ss",  &control::detail::ss_reg);

    // tf2zp / zp2tf already live in libs/builtin (poly), and tf2ss /
    // ss2tf in libs/signal (filter_implementation). See library.cpp
    // header comment.
}

} // namespace numkit
