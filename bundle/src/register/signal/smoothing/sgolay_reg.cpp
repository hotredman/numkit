// toolboxes/signal/src/smoothing/sgolay_reg.cpp
//
// Register half of the signal sgolay builtins: the CallContext wrappers
// delegating to the engine-free compute in sgolay.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/signal/smoothing/sgolay.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

namespace detail {

void sgolay_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("sgolay: requires 2 arguments (order, framelen)",
                     0, 0, "sgolay", "", "numkit:sgolay:nargin");
    auto *mr = ctx.engine->resource();
    const int order    = static_cast<int>(args[0].toScalar());
    const int framelen = static_cast<int>(args[1].toScalar());
    outs[0] = sgolay(order, framelen, mr);
    // [B,G] = sgolay(...): G is the framelen × (order+1) differentiation-
    // filter matrix (MATLAB's second output).
    if (nargout >= 2)
        outs[1] = sgolayDiff(order, framelen, mr);
}

void sgolayfilt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("sgolayfilt: requires 3 arguments (x, order, framelen)",
                     0, 0, "sgolayfilt", "", "numkit:sgolayfilt:nargin");
    auto *res = ctx.engine->resource();
    const int order    = static_cast<int>(args[1].toScalar());
    const int framelen = static_cast<int>(args[2].toScalar());
    // sgolayfilt(x, order, framelen, weights, dim): weights (4th) and dim
    // (5th) are optional; an empty [] in either slot selects the default.
    const int dim = (args.size() >= 5 && !args[4].isEmpty())
                        ? static_cast<int>(args[4].toScalar()) : 0;
    const Value weights = (args.size() >= 4)
                              ? args[3]
                              : Value::matrix(0, 0, ValueType::DOUBLE, res);
    outs[0] = sgolayfilt(args[0], order, framelen, weights, dim, res);
}

} // namespace detail

} // namespace numkit::signal
