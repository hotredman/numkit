// toolboxes/signal/src/digital_filtering/poly_utils_reg.cpp
//
// Register half of the signal poly_utils builtins: the CallContext wrappers
// delegating to the engine-free compute in poly_utils.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/signal/digital_filtering/poly_utils.hpp>

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

void polyscale_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("polyscale: requires (p, scale)",
                    0, 0, "polyscale", "", "numkit:polyscale:nargin");
    outs[0] = polyscale(args[0], args[1], ctx.engine->resource());
}

void polystab_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("polystab: requires (a)",
                    0, 0, "polystab", "", "numkit:polystab:nargin");
    outs[0] = polystab(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
