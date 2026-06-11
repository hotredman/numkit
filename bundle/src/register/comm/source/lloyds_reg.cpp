// toolboxes/comm/src/source/lloyds_reg.cpp
//
// Register half of the comm `lloyds` builtin: the CallContext wrapper that
// reads the optional tolerance, destructures the (partition, codebook,
// distor, rel) tuple from the engine-free compute in lloyds.cpp, and emits
// up to four outputs. library.cpp forward-declares + registers it by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/source/lloyds.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <tuple>
#include <utility>

namespace numkit::comm {
namespace detail {

void lloyds_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lloyds: requires (training_set, ini_codebook [, tol])",
                    0, 0, "lloyds", "", "numkit:lloyds:nargin");
    auto *mr = ctx.engine->resource();
    double tol = 1e-7;
    if (args.size() >= 3 && !args[2].isEmpty())
        tol = args[2].toScalar();
    auto [partition, codebook, distor, rel] =
        lloyds(args[0], args[1], tol, mr);
    outs[0] = std::move(partition);
    if (nargout > 1) outs[1] = std::move(codebook);
    if (nargout > 2) outs[2] = Value::scalar(distor, mr);
    if (nargout > 3) outs[3] = Value::scalar(rel, mr);
}

} // namespace detail

} // namespace numkit::comm
