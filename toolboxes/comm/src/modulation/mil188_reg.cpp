// toolboxes/comm/src/modulation/mil188_reg.cpp
//
// Register half of the comm MIL-STD-188 QAM builtins: the CallContext
// wrappers mil188qammod / mil188qamdemod that delegate to the engine-free
// compute in mil188.cpp. library.cpp forward-declares + registers these by
// name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/modulation/mil188.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

namespace numkit::comm {
namespace detail {

void mil188qammod_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mil188qammod: requires (x, M)",
                    0, 0, "mil188qammod", "",
                    "numkit:mil188qammod:nargin");
    const int M = static_cast<int>(args[1].toScalar());
    outs[0] = mil188qammod(args[0], M, ctx.engine->resource());
}

void mil188qamdemod_reg(Span<const Value> args, size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mil188qamdemod: requires (y, M)",
                    0, 0, "mil188qamdemod", "",
                    "numkit:mil188qamdemod:nargin");
    const int M = static_cast<int>(args[1].toScalar());
    outs[0] = mil188qamdemod(args[0], M, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::comm
