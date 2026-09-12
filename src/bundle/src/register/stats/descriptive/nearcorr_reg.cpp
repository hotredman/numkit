// toolboxes/signal/src/descriptive/nearcorr_reg.cpp
//
// CallContext register half of descriptive/nearcorr.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/builtin/elmat.hpp>
#include <nk/linalg/eig.hpp>             // eig_symmetric (migrated)
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

void nearcorr_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nearcorr: requires (A)",
                    0, 0, "nearcorr", "", "nk:nearcorr:nargin");
    outs[0] = nearcorr(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace nk::stats
