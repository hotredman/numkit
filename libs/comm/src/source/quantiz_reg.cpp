// libs/comm/src/source/quantiz_reg.cpp
//
// Register half of the comm `quantiz` builtin: the CallContext wrapper that
// selects the one- vs three-output form and delegates to the engine-free
// compute in quantiz.cpp. library.cpp forward-declares + registers it by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/source/quantiz.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <utility>

namespace numkit::comm {
namespace detail {

void quantiz_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("quantiz: requires (sig, partition [, codebook])",
                    0, 0, "quantiz", "", "numkit:quantiz:nargin");
    auto *mr = ctx.engine->resource();

    if (nargout <= 1 && args.size() == 2) {
        // 2-arg form, single output: just indx.
        outs[0] = quantiz_indx(args[0], args[1], mr);
        return;
    }
    if (args.size() < 3)
        throw Error("quantiz: codebook required for quantv/distor outputs",
                    0, 0, "quantiz", "", "numkit:quantiz:fewInputs");
    auto r = quantiz(args[0], args[1], args[2], mr);
    outs[0] = std::move(r.indx);
    if (nargout > 1) outs[1] = std::move(r.quantv);
    if (nargout > 2) outs[2] = Value::scalar(r.distor, mr);
}

} // namespace detail

} // namespace numkit::comm
