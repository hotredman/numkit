// toolboxes/signal/src/moments/moments_reg.cpp
//
// CallContext register half of moments/moments.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/moments/moments.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/ops/helpers.hpp>
#include "reduction_helpers.hpp"
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

void skewness_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("skewness: requires at least 1 argument",
                     0, 0, "skewness", "", "numkit:skewness:nargin");
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
                     0, 0, "kurtosis", "", "numkit:kurtosis:nargin");
    int normFlag = 1;
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        normFlag = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = kurtosis(args[0], normFlag, dim, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
