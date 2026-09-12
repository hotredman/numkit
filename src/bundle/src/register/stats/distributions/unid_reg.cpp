// toolboxes/signal/src/distributions/unid_reg.cpp
//
// CallContext register half of distributions/unid.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/builtin/datafun.hpp>
#include <nk/stats/distributions/unid.hpp>
#include <nk/value/error.hpp>
#include <nk/value/value.hpp>
#include "distributions/dist_helpers.hpp"
#include "distributions/unid_detail.hpp"
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

void unidpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("unidpdf: requires (k, N)", 0, 0, "unidpdf", "", "nk:unidpdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &N = args[1];
    if (N.isScalar())
        outs[0] = unidpdf(args[0], N.toScalar(), mr);
    else
        outs[0] = broadcast_dist2(args[0], N, mr, "unidpdf", unidpdfK);
}

void unidcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    if (a.size() < 2)
        throw Error("unidcdf: requires (k, N[, 'upper'])", 0, 0, "unidcdf", "", "nk:unidcdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &N = a[1];
    Value v = N.isScalar() ? unidcdf(a[0], N.toScalar(), mr)
                           : broadcast_dist2(a[0], N, mr, "unidcdf", unidcdfK);
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void unidinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("unidinv: requires (p, N)", 0, 0, "unidinv", "", "nk:unidinv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &N = args[1];
    if (N.isScalar())
        outs[0] = unidinv(args[0], N.toScalar(), mr);
    else
        outs[0] = broadcast_dist2(args[0], N, mr, "unidinv", unidinvK);
}

void unidrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("unidrnd: requires N[, sz...]", 0, 0, "unidrnd", "", "nk:unidrnd:nargin");
    const double N = args[0].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 1, rows, cols);
    outs[0] = unidrnd(ctx.engine->rng(), N, rows, cols, ctx.engine->resource());
}

void unidstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx.engine->resource(), "unidstat",
                       [](double N) { return unidstat(N); });
}

} // namespace detail

} // namespace nk::stats
