// libs/comm/src/source/arithcoding_reg.cpp
//
// Register half of the comm arithmetic-coding builtins: the CallContext
// wrappers arithenco / arithdeco that delegate to the engine-free compute
// in arithcoding.cpp. library.cpp forward-declares + registers these by
// name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/source/arithcoding.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

namespace numkit::comm {
namespace detail {

void arithenco_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("arithenco: requires (seq, counts)",
                    0, 0, "arithenco", "", "numkit:arithenco:nargin");
    outs[0] = arithenco(args[0], args[1], ctx.engine->resource());
}

void arithdeco_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("arithdeco: requires (code, counts, len)",
                    0, 0, "arithdeco", "", "numkit:arithdeco:nargin");
    const size_t len = static_cast<size_t>(args[2].toScalar());
    outs[0] = arithdeco(args[0], args[1], len, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::comm
