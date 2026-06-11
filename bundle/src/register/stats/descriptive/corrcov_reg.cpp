// toolboxes/signal/src/descriptive/corrcov_reg.cpp
//
// CallContext register half of descriptive/corrcov.cpp (Phase 2b compute/register split).
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

void corrcov_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("corrcov: requires (C)",
                    0, 0, "corrcov", "", "numkit:corrcov:nargin");
    auto *mr = ctx.engine->resource();
    auto [Rv, Sv] = corrcov(args[0], mr);
    outs[0] = std::move(Rv);
    if (nargout > 1) outs[1] = std::move(Sv);
}

} // namespace detail

} // namespace numkit::stats
