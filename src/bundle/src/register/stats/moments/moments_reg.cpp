// toolboxes/signal/src/moments/moments_reg.cpp
//
// CallContext register half of moments/moments.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/stats/moments/moments.hpp>
#include <nk/value/error.hpp>
#include <nk/value/value.hpp>
#include <nk/ops/helpers.hpp>
#include <nk/ops/reductions.hpp>
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

void skewness_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("skewness: requires at least 1 argument",
                     0, 0, "skewness", "", "nk:skewness:nargin");
    int normFlag = 1;  // MATLAB default
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        normFlag = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = skewness(args[0], normFlag, dim, ctx.engine->resource());
}

void kurtosis_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("kurtosis: requires at least 1 argument",
                     0, 0, "kurtosis", "", "nk:kurtosis:nargin");
    int normFlag = 1;
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        normFlag = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = kurtosis(args[0], normFlag, dim, ctx.engine->resource());
}

} // namespace detail

} // namespace nk::stats
