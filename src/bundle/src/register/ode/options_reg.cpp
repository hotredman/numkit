// bundle/src/register/ode/options_reg.cpp — Engine adapter + embedded-.m registration relocated from the
// ode toolbox in Phase D (solver 3-way split): the toolbox keeps the
// Engine-free FnHandle kernel; this core-coupled glue lives in bundle.
#include <numkit/ode/solvers.hpp>
#include <numkit/ode/options.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>

namespace numkit::ode {

// ── Engine adapters ─────────────────────────────────────────────────

namespace detail {

void odeset_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    outs[0] = odeset(args.data(), args.size(), ctx.engine->resource());
}

void odeget_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("odeget: requires (opts, name[, default])",
                    0, 0, "odeget", "", "numkit:odeget:nargin");
    auto *mr = ctx.engine->resource();
    const Value def = (args.size() > 2) ? args[2] : Value::Empty;
    outs[0] = odeget(args[0], args[1], def, mr);
}

} // namespace detail

} // namespace numkit::ode
