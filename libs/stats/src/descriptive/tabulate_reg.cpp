// libs/signal/src/descriptive/tabulate_reg.cpp
//
// CallContext register half of descriptive/tabulate.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/descriptive/descriptive.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace detail {

void tabulate_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("tabulate: requires (x)",
                    0, 0, "tabulate", "", "numkit:tabulate:nargin");
    if (args[0].isChar() || args[0].isString())
        throw Error("tabulate: string/cell inputs not yet supported",
                    0, 0, "tabulate", "", "numkit:tabulate:NotSupported");
    outs[0] = tabulate(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
