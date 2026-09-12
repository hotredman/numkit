// toolboxes/signal/src/descriptive/cholcov_reg.cpp
//
// CallContext register half of descriptive/cholcov.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/builtin/elmat.hpp>
#include <nk/linalg/decompositions.hpp>   // chol (migrated)
#include <nk/linalg/eig.hpp>              // eig_symmetric (migrated)
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

void cholcov_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cholcov: requires (SIGMA)",
                    0, 0, "cholcov", "", "nk:cholcov:nargin");
    auto *mr = ctx.engine->resource();
    auto [T, p] = cholcov(args[0], mr);
    outs[0] = std::move(T);
    if (nargout > 1) outs[1] = std::move(p);
}

} // namespace detail

} // namespace nk::stats
