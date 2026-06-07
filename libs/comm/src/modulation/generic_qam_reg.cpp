// libs/comm/src/modulation/generic_qam_reg.cpp
//
// Register half of the comm generic-constellation builtins: the CallContext
// wrappers genqammod / genqamdemod that delegate to the engine-free compute
// in generic_qam.cpp. library.cpp forward-declares + registers these by
// name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/modulation/generic_qam.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

namespace numkit::comm {
namespace detail {

void genqammod_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("genqammod: requires (x, constellation)",
                    0, 0, "genqammod", "", "numkit:genqammod:nargin");
    // Bit-input mode (name-value 'InputType','bit') -> deferred.
    if (args.size() > 2)
        throw Error("genqammod: name-value options not yet supported "
                    "(bit-input mode deferred)",
                    0, 0, "genqammod", "", "numkit:genqammod:NotSupported");
    outs[0] = genqammod(args[0], args[1], ctx.engine->resource());
}

void genqamdemod_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("genqamdemod: requires (y, constellation)",
                    0, 0, "genqamdemod", "", "numkit:genqamdemod:nargin");
    if (args.size() > 2)
        throw Error("genqamdemod: name-value options not yet supported "
                    "(bit-output mode deferred)",
                    0, 0, "genqamdemod", "", "numkit:genqamdemod:NotSupported");
    outs[0] = genqamdemod(args[0], args[1], ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::comm
