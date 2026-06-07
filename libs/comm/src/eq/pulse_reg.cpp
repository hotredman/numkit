// libs/comm/src/eq/pulse_reg.cpp
//
// Register half of the comm pulse-shaping builtins: the CallContext
// wrappers rcosdesign / gaussdesign / rectpulse / intdump that parse args
// and delegate to the engine-free compute in pulse.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/eq/pulse.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <string>

namespace numkit::comm {
namespace detail {

void rcosdesign_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("rcosdesign: requires (beta, span, sps [, shape])",
                    0, 0, "rcosdesign", "", "numkit:rcosdesign:nargin");
    const double beta = args[0].toScalar();
    const int span    = static_cast<int>(args[1].toScalar());
    const int sps     = static_cast<int>(args[2].toScalar());
    std::string shape = "normal";
    if (args.size() >= 4 && !args[3].isEmpty()) {
        if (!args[3].isChar() && !args[3].isString())
            throw Error("rcosdesign: shape must be a string",
                        0, 0, "rcosdesign", "", "numkit:rcosdesign:shape");
        shape = args[3].toString();
    }
    outs[0] = rcosdesign(beta, span, sps, shape, ctx.engine->resource());
}

void gaussdesign_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("gaussdesign: requires (BT, span, sps)",
                    0, 0, "gaussdesign", "", "numkit:gaussdesign:nargin");
    const double BT  = args[0].toScalar();
    const int span   = static_cast<int>(args[1].toScalar());
    const int sps    = static_cast<int>(args[2].toScalar());
    outs[0] = gaussdesign(BT, span, sps, ctx.engine->resource());
}

void rectpulse_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rectpulse: requires (x, n)",
                    0, 0, "rectpulse", "", "numkit:rectpulse:nargin");
    const int n = static_cast<int>(args[1].toScalar());
    outs[0] = rectpulse(args[0], n, ctx.engine->resource());
}

void intdump_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("intdump: requires (x, n)",
                    0, 0, "intdump", "", "numkit:intdump:nargin");
    const int n = static_cast<int>(args[1].toScalar());
    outs[0] = intdump(args[0], n, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::comm
