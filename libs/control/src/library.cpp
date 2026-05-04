// libs/control/src/library.cpp
//
// Registration hub for the Control System Toolbox builtins.
// Namespace: control.<sub>.<name>; most entry points are also aliased
// into `compat.<name>` so MATLAB-style scripts can call them flat.
// A few names (e.g. `isstable`) collide with builtins from another
// toolbox — for those we register only the qualified form via
// `regOnly` so engine startup doesn't trip the duplicate-registration
// guard.

#include <numkit/control/library.hpp>

#include <numkit/core/types.hpp>

namespace numkit::control::detail {
// lti/lti.cpp
void tf_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void zpk_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void ss_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
// props/props.cpp
void isct_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void isdt_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void issiso_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void isproper_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void isstable_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void order_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void pole_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void zero_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void damp_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
// connect/connect.cpp
void series_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void parallel_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void feedback_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
// response/response.cpp
void step_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void impulse_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void lsim_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
// freq/freq.cpp
void evalfr_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void freqresp_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void bode_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void nyquist_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
// analyze/analyze.cpp
void dcgain_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void margin_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void stepinfo_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
// (Conversion entry points like tf2zp / zp2tf / tf2ss / ss2tf already
//  live in libs/builtin and libs/signal — we don't shadow them here.)
} // namespace numkit::control::detail

namespace numkit {

void ControlLibrary::install(Engine &engine)
{
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("control.") + sub, name, fn);
        engine.registerFunction("compat", name, fn);
    };
    auto regOnly = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("control.") + sub, name, fn);
    };

    reg("lti", "tf",  &control::detail::tf_reg);
    reg("lti", "zpk", &control::detail::zpk_reg);
    reg("lti", "ss",  &control::detail::ss_reg);

    reg("props", "isct",     &control::detail::isct_reg);
    reg("props", "isdt",     &control::detail::isdt_reg);
    reg("props", "issiso",   &control::detail::issiso_reg);
    reg("props", "isproper", &control::detail::isproper_reg);
    // isstable already lives in libs/signal (operates on coefficient
    // pairs (b,a)). Keep ours qualified so a script can opt-in via
    // `import control.props.*` when working with sys structs.
    regOnly("props", "isstable", &control::detail::isstable_reg);
    reg("props", "order",    &control::detail::order_reg);
    reg("props", "pole",     &control::detail::pole_reg);
    reg("props", "zero",     &control::detail::zero_reg);
    reg("props", "damp",     &control::detail::damp_reg);

    reg("connect", "series",   &control::detail::series_reg);
    reg("connect", "parallel", &control::detail::parallel_reg);
    reg("connect", "feedback", &control::detail::feedback_reg);

    reg("response", "step",     &control::detail::step_reg);
    reg("response", "impulse",  &control::detail::impulse_reg);
    reg("response", "lsim",     &control::detail::lsim_reg);
    reg("response", "evalfr",   &control::detail::evalfr_reg);
    reg("response", "freqresp", &control::detail::freqresp_reg);
    reg("response", "bode",     &control::detail::bode_reg);
    reg("response", "nyquist",  &control::detail::nyquist_reg);

    reg("analyze", "dcgain",   &control::detail::dcgain_reg);
    reg("analyze", "margin",   &control::detail::margin_reg);
    reg("analyze", "stepinfo", &control::detail::stepinfo_reg);
}

} // namespace numkit
