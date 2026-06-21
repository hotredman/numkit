// toolboxes/control/src/response/response_reg.cpp
//
// Register half of the time-response builtins: the CallContext wrappers
// step / impulse / lsim that delegate to the engine-free compute in
// response.cpp. library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/control/response/response.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/value.hpp>   // Value::matrix, ValueType (default t/x0 args)
#include <numkit/value/error.hpp>

#include <cstddef>
#include <utility>

namespace numkit::control {
namespace detail {

void step_reg(Span<const Value> a, size_t /*nargout*/, Span<Value> outs,
              CallContext &c)
{
    if (a.empty())
        throw Error("step: requires (sys [, t])",
                    0, 0, "step", "", "numkit:step:nargin");
    Value tArg = (a.size() >= 2) ? a[1] : Value::matrix(1, 0, ValueType::DOUBLE, c.engine->resource());
    Value xOut;
    auto [y, t] = step_response(a[0], tArg, c.engine->resource(),
                                outs.size() >= 3 ? &xOut : nullptr);
    if (outs.size() >= 1) outs[0] = std::move(y);
    if (outs.size() >= 2) outs[1] = std::move(t);
    if (outs.size() >= 3) outs[2] = std::move(xOut);
}

void impulse_reg(Span<const Value> a, size_t /*nargout*/, Span<Value> outs,
                 CallContext &c)
{
    if (a.empty())
        throw Error("impulse: requires (sys [, t])",
                    0, 0, "impulse", "", "numkit:impulse:nargin");
    Value tArg = (a.size() >= 2) ? a[1] : Value::matrix(1, 0, ValueType::DOUBLE, c.engine->resource());
    Value xOut;
    auto [y, t] = impulse_response(a[0], tArg, c.engine->resource(),
                                   outs.size() >= 3 ? &xOut : nullptr);
    if (outs.size() >= 1) outs[0] = std::move(y);
    if (outs.size() >= 2) outs[1] = std::move(t);
    if (outs.size() >= 3) outs[2] = std::move(xOut);
}

void initial_reg(Span<const Value> a, size_t /*nargout*/, Span<Value> outs,
                 CallContext &c)
{
    if (a.size() < 2)
        throw Error("initial: requires (sys, x0 [, t])",
                    0, 0, "initial", "", "numkit:initial:nargin");
    Value tArg = (a.size() >= 3) ? a[2] : Value::matrix(1, 0, ValueType::DOUBLE, c.engine->resource());
    Value xOut;
    auto [y, t] = initial_response(a[0], a[1], tArg, c.engine->resource(),
                                   outs.size() >= 3 ? &xOut : nullptr);
    if (outs.size() >= 1) outs[0] = std::move(y);
    if (outs.size() >= 2) outs[1] = std::move(t);
    if (outs.size() >= 3) outs[2] = std::move(xOut);
}

void lsim_reg(Span<const Value> a, size_t /*nargout*/, Span<Value> outs,
              CallContext &c)
{
    if (a.size() < 3)
        throw Error("lsim: requires (sys, u, t [, x0])",
                    0, 0, "lsim", "", "numkit:lsim:nargin");
    Value x0 = (a.size() >= 4) ? a[3] : Value::matrix(0, 0, ValueType::DOUBLE, c.engine->resource());
    Value xOut;
    outs[0] = lsim(a[0], a[1], a[2], x0, c.engine->resource(),
                   outs.size() >= 3 ? &xOut : nullptr);
    // MATLAB: [y, t, x] = lsim(...). t echoes the input time grid.
    if (outs.size() >= 2) {
        Value t = Value::matrix(a[2].numel(), 1, ValueType::DOUBLE, c.engine->resource());
        for (std::size_t i = 0; i < a[2].numel(); ++i)
            t.doubleDataMut()[i] = a[2].elemAsDouble(i);
        outs[1] = std::move(t);
    }
    if (outs.size() >= 3) outs[2] = std::move(xOut);
}

} // namespace detail
} // namespace numkit::control
