// libs/signal/src/dim/pca_reg.cpp
//
// CallContext register half of dim/pca.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/dim/pca.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "pca_detail.hpp"
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

void pca_reg(Span<const Value> args, size_t nargout,
             Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pca: requires X", 0, 0, "pca", "", "numkit:pca:nargin");
    auto [coeff, score, latent, tsq, explained, mu] =
        pca(args[0], ctx.engine->resource());
    outs[0] = std::move(coeff);
    if (nargout > 1) outs[1] = std::move(score);
    if (nargout > 2) outs[2] = std::move(latent);
    if (nargout > 3) outs[3] = std::move(tsq);
    if (nargout > 4) outs[4] = std::move(explained);
    if (nargout > 5) outs[5] = std::move(mu);
}

void pcacov_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pcacov: requires C", 0, 0, "pcacov", "",
                    "numkit:pcacov:nargin");
    auto [coeff, latent, explained] = pcacov(args[0], ctx.engine->resource());
    outs[0] = std::move(coeff);
    if (nargout > 1) outs[1] = std::move(latent);
    if (nargout > 2) outs[2] = std::move(explained);
}

void pcares_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pcares: requires (X, ndim)", 0, 0, "pcares", "",
                    "numkit:pcares:nargin");
    auto [res, recon] = pcares_full(args[0], (int)args[1].toScalar(), ctx.engine->resource());
    outs[0] = std::move(res);
    if (nargout > 1) outs[1] = std::move(recon);
}

} // namespace detail

} // namespace numkit::stats
