// toolboxes/comm/src/channel/fading_reg.cpp
//
// Register half of the comm fading-channel builtins: the CallContext
// wrappers rayleighchan / ricianchan that delegate to the engine-free
// compute in fading.cpp. library.cpp forward-declares + registers these by
// name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/channel/fading.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

namespace numkit::comm {
namespace detail {

void rayleighchan_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rayleighchan: requires (x)",
                    0, 0, "rayleighchan", "", "numkit:rayleighchan:nargin");
    outs[0] = rayleighchan(args[0], ctx.engine->resource());
}

void ricianchan_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ricianchan: requires (x, K)",
                    0, 0, "ricianchan", "", "numkit:ricianchan:nargin");
    outs[0] = ricianchan(args[0], args[1].toScalar(),
                         ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::comm
