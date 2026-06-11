// toolboxes/comm/src/eq/scrambler_reg.cpp
//
// Register half of the comm scrambler builtins: the CallContext wrappers
// scrambler / descrambler that delegate to the engine-free compute in
// scrambler.cpp. library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/eq/scrambler.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

namespace numkit::comm {
namespace detail {

void scrambler_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("scrambler: requires (x, poly, initState)",
                    0, 0, "scrambler", "", "numkit:scrambler:nargin");
    outs[0] = scrambler(args[0], args[1], args[2],
                        ctx.engine->resource());
}

void descrambler_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("descrambler: requires (y, poly, initState)",
                    0, 0, "descrambler", "", "numkit:descrambler:nargin");
    outs[0] = descrambler(args[0], args[1], args[2],
                          ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::comm
