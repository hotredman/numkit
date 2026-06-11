// toolboxes/signal/src/digital_filtering/shiftdata_reg.cpp
//
// Register half of the signal shiftdata builtins: the CallContext wrappers
// delegating to the engine-free compute in shiftdata.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/signal/digital_filtering/shiftdata.hpp>

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

void shiftdata_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("shiftdata: requires (x [, dim])",
                    0, 0, "shiftdata", "", "numkit:shiftdata:nargin");
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        dim = static_cast<int>(args[1].toScalar());
    auto [shifted, permV, nshV] = shiftdata(args[0], dim, ctx.engine->resource());
    outs[0] = shifted;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = permV;
    if (nargout >= 3 && outs.size() >= 3) outs[2] = nshV;
}

void unshiftdata_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("unshiftdata: requires (x, perm, nshifts)",
                    0, 0, "unshiftdata", "", "numkit:unshiftdata:nargin");
    outs[0] = unshiftdata(args[0], args[1], args[2], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
