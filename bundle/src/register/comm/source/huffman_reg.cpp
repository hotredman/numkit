// toolboxes/comm/src/source/huffman_reg.cpp
//
// Register half of the comm Huffman builtins: the CallContext wrappers
// huffmandict / huffmanenco / huffmandeco that destructure the (dict,
// avglen) pair from huffmandict and delegate to the engine-free compute in
// huffman.cpp. library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/source/huffman.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <utility>

namespace numkit::comm {
namespace detail {

void huffmandict_reg(Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("huffmandict: requires (symbols, probs)",
                    0, 0, "huffmandict", "", "numkit:huffmandict:nargin");
    auto *mr = ctx.engine->resource();
    auto [dict, avglen] = huffmandict(args[0], args[1], mr);
    outs[0] = std::move(dict);
    if (nargout > 1)
        outs[1] = Value::scalar(avglen, mr);
}

void huffmanenco_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("huffmanenco: requires (sig, dict)",
                    0, 0, "huffmanenco", "", "numkit:huffmanenco:nargin");
    outs[0] = huffmanenco(args[0], args[1], ctx.engine->resource());
}

void huffmandeco_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("huffmandeco: requires (bits, dict)",
                    0, 0, "huffmandeco", "", "numkit:huffmandeco:nargin");
    outs[0] = huffmandeco(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::comm
