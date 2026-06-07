// libs/comm/src/source/dpcmopt_reg.cpp
//
// Register half of the comm `dpcmopt` builtin: the CallContext wrapper that
// reads the predictor order + optional initial codebook and destructures the
// DpcmOptResult from the engine-free compute in dpcmopt.cpp. library.cpp
// forward-declares + registers it by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/source/dpcmopt.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <utility>

namespace numkit::comm {
namespace detail {

void dpcmopt_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dpcmopt: requires (training_set, ord [, ini_codebook])",
                    0, 0, "dpcmopt", "", "numkit:dpcmopt:nargin");
    auto *mr = ctx.engine->resource();
    const int ord = static_cast<int>(args[1].toScalar());

    const Value &ini = (args.size() >= 3 && !args[2].isEmpty())
                          ? args[2] : Value::Empty;
    if (ini.isEmpty() && nargout > 1)
        throw Error("dpcmopt: ini_codebook required for codebook/partition outputs",
                    0, 0, "dpcmopt", "", "numkit:dpcmopt:NeedIniCodebook");

    auto res = dpcmopt(args[0], ord, ini, mr);
    outs[0] = std::move(res.predictor);
    if (nargout > 1) outs[1] = std::move(res.codebook);
    if (nargout > 2) outs[2] = std::move(res.partition);
}

} // namespace detail

} // namespace numkit::comm
