// toolboxes/signal/src/descriptive/tabulate_reg.cpp
//
// CallContext register half of descriptive/tabulate.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/stats/descriptive/descriptive.hpp>
#include <nk/value/error.hpp>
#include <nk/value/value.hpp>
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace nk::stats {

namespace detail {

void tabulate_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("tabulate: requires (x)",
                    0, 0, "tabulate", "", "nk:tabulate:nargin");
    if (args[0].isChar() || args[0].isString())
        throw Error("tabulate: string/cell inputs not yet supported",
                    0, 0, "tabulate", "", "nk:tabulate:NotSupported");
    outs[0] = tabulate(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace nk::stats
