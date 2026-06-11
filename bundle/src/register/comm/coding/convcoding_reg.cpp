// toolboxes/comm/src/coding/convcoding_reg.cpp
//
// Register half of the comm convolutional-coding builtins: the CallContext
// wrappers poly2trellis / convenc / vitdec / istrellis that parse args and
// delegate to the engine-free compute in convcoding.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/coding/convcoding.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <string>

namespace numkit::comm {
namespace detail {

void poly2trellis_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("poly2trellis: requires (ConstraintLength, CodeGenerator)",
                    0, 0, "poly2trellis", "", "numkit:poly2trellis:nargin");
    if (args.size() >= 3 && !args[2].isEmpty())
        throw Error("poly2trellis: FeedbackConnection (feedback codes) is not "
                    "supported in this revision",
                    0, 0, "poly2trellis", "", "numkit:poly2trellis:feedback");
    outs[0] = poly2trellis(args[0], args[1], ctx.engine->resource());
}

void convenc_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("convenc: requires (msg, trellis)",
                    0, 0, "convenc", "", "numkit:convenc:nargin");
    if (args.size() >= 3 && !args[2].isEmpty())
        throw Error("convenc: puncture pattern / initial state are not "
                    "supported in this revision",
                    0, 0, "convenc", "", "numkit:convenc:opts");
    outs[0] = convenc(args[0], args[1], ctx.engine->resource());
}

void vitdec_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("vitdec: requires (code, trellis, tblen[, opmode[, dectype]])",
                    0, 0, "vitdec", "", "numkit:vitdec:nargin");
    const long long tblen = static_cast<long long>(args[2].toScalar());
    const std::string opmode =
        (args.size() >= 4 && (args[3].isChar() || args[3].isString()))
            ? args[3].toString() : std::string("trunc");
    const std::string dectype =
        (args.size() >= 5 && (args[4].isChar() || args[4].isString()))
            ? args[4].toString() : std::string("hard");
    outs[0] = vitdec(args[0], args[1], tblen, opmode, dectype,
                     ctx.engine->resource());
}

void istrellis_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("istrellis: requires 1 argument",
                    0, 0, "istrellis", "", "numkit:istrellis:nargin");
    outs[0] = istrellis(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::comm
