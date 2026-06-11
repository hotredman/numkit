// toolboxes/signal/src/measurements/pulse_metrics_reg.cpp
//
// CallContext register half of measurements/pulse_metrics.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/signal/measurements/pulse_metrics.hpp>
#include "measurements/pulse_metrics_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

namespace detail {

#define NK_PULSE_REG(name, fn)                                                  \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires at least 1 argument",                 \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const Value &fs = (args.size() >= 2) ? args[1] : Value::Empty;         \
        outs[0] = fn(args[0], fs, ctx.engine->resource());                      \
    }

void statelevels_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("statelevels: requires 1 argument",
                     0, 0, "statelevels", "", "numkit:statelevels:nargin");
    outs[0] = statelevels(args[0], ctx.engine->resource());
}

void settlingtime_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("settlingtime: requires at least 1 argument",
                     0, 0, "settlingtime", "", "numkit:settlingtime:nargin");
    const Value &fs = (args.size() >= 2) ? args[1] : Value::Empty;
    double tol = 0.02;
    if (args.size() >= 3 && !args[2].isEmpty()) tol = args[2].toScalar();
    outs[0] = settlingtime(args[0], fs, tol, ctx.engine->resource());
}

// risetime / falltime forward to the compute worker pulseRiseFall
// (declared in pulse_metrics_detail.hpp), which emits up to 5 outputs:
//   [R, LT, UT, LL, UL] — duration, lower(10%) crossing time, upper(90%)
//   crossing time, and the lower/upper reference LEVELS (scalars).
void risetime_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{ pulseRiseFall(args, nargout, outs, /*rising=*/true, "risetime", ctx.engine->resource()); }

void falltime_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{ pulseRiseFall(args, nargout, outs, /*rising=*/false, "falltime", ctx.engine->resource()); }

NK_PULSE_REG(midcross,    midcross)
NK_PULSE_REG(slewrate,    slewrate)
NK_PULSE_REG(overshoot,   overshoot)
NK_PULSE_REG(undershoot,  undershoot)
NK_PULSE_REG(pulsewidth,  pulsewidth)
NK_PULSE_REG(pulseperiod, pulseperiod)
NK_PULSE_REG(pulsesep,    pulsesep)
NK_PULSE_REG(dutycycle,   dutycycle)

#undef NK_PULSE_REG

} // namespace detail

} // namespace numkit::signal
