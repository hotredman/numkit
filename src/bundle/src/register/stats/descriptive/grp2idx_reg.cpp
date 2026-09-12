// toolboxes/signal/src/descriptive/grp2idx_reg.cpp
//
// CallContext register half of descriptive/grp2idx.cpp (Phase 2b compute/register split).
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

void grp2idx_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.empty())
        throw Error("grp2idx: requires (s)",
                    0, 0, "grp2idx", "", "nk:grp2idx:nargin");
    Grp2idxResult r = grp2idx(args[0], ctx.engine->resource());
    outs[0] = std::move(r.G);
    if (nargout >= 2) outs[1] = std::move(r.GN);
    if (nargout >= 3) outs[2] = std::move(r.GL);
}

} // namespace detail

} // namespace nk::stats
