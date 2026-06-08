// toolboxes/signal/src/descriptive/cholcov_reg.cpp
//
// CallContext register half of descriptive/cholcov.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/language/arrays/matrix.hpp>
#include <numkit/linalg/decompositions.hpp>   // chol (migrated)
#include <numkit/linalg/eig.hpp>              // eig_symmetric (migrated)
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

void cholcov_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cholcov: requires (SIGMA)",
                    0, 0, "cholcov", "", "numkit:cholcov:nargin");
    auto *mr = ctx.engine->resource();
    auto [T, p] = cholcov(args[0], mr);
    outs[0] = std::move(T);
    if (nargout > 1) outs[1] = std::move(p);
}

} // namespace detail

} // namespace numkit::stats
