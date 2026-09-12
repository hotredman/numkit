// toolboxes/signal/src/descriptive/tiedrank_reg.cpp
//
// CallContext register half of descriptive/tiedrank.cpp (Phase 2b compute/register split).
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

void tiedrank_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("tiedrank: requires at least one argument",
                    0, 0, "tiedrank", "", "nk:tiedrank:nargin");
    auto *mr = ctx.engine->resource();
    auto [r, ta] = tiedrank(args[0], mr);
    outs[0] = std::move(r);
    if (nargout > 1) outs[1] = std::move(ta);
}

} // namespace detail

} // namespace nk::stats
