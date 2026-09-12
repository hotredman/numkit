// toolboxes/signal/src/descriptive/partial_canon_reg.cpp
//
// CallContext register half of descriptive/partial_canon.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/linalg/decompositions.hpp>     // qr_decompose, svd_decompose
#include <nk/stats/descriptive/descriptive.hpp>
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
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

void partialcorri_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("partialcorri: requires (Y, X [, Z])",
                    0, 0, "partialcorri", "", "nk:partialcorri:nargin");
    const Value Z = (args.size() >= 3) ? args[2] : Value();
    outs[0] = partialcorri(args[0], args[1], Z, ctx.engine->resource());
}

void canoncorr_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("canoncorr: requires (X, Y)",
                    0, 0, "canoncorr", "", "nk:canoncorr:nargin");
    auto res = canoncorr(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(res.A);
    if (nargout > 1) outs[1] = std::move(res.B);
    if (nargout > 2) outs[2] = std::move(res.r);
}

} // namespace detail

} // namespace nk::stats
