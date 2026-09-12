// toolboxes/signal/src/nan_aware/nan_aware_reg.cpp
//
// CallContext register half of nan_aware/nan_aware.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/stats/nan_aware/nan_aware.hpp>
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/value.hpp>
#include <nk/ops/nan_reductions.hpp>
#include <nk/ops/helpers.hpp>
#include "nan_aware/nan_aware_detail.hpp"
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

#define NK_NAN_REDUCTION_ADAPTER(name, fn)                                      \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires at least 1 argument",                 \
                         0, 0, #name, "", "nk:" #name ":nargin");                 \
        int dim = 0;                                                             \
        if (args.size() >= 2 && !args[1].isEmpty())                              \
            dim = static_cast<int>(args[1].toScalar());                          \
        outs[0] = fn(args[0], dim, ctx.engine->resource());                     \
    }

NK_NAN_REDUCTION_ADAPTER(nansum,    nansum)
NK_NAN_REDUCTION_ADAPTER(nanmean,   nanmean)
NK_NAN_REDUCTION_ADAPTER(nanmedian, nanmedian)

#undef NK_NAN_REDUCTION_ADAPTER

// nanmax / nanmin accept both signatures:
//   nanmax(A)         — reduce over first non-singleton
//   nanmax(A, dim)    — legacy/nk form (dim in arg 1)
//   nanmax(A, [], d)  — MATLAB-style 3-arg form (dim in arg 2; arg 1 = [])
#define NK_NAN_MAXMIN_ADAPTER(name, fn)                                          \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                 \
                    Span<Value> outs, CallContext &ctx)                         \
    {                                                                             \
        if (args.empty())                                                         \
            throw Error(#name ": requires at least 1 argument",                  \
                         0, 0, #name, "", "nk:" #name ":nargin");                  \
        int dim = 0;                                                              \
        if (args.size() == 2 && !args[1].isEmpty())                               \
            dim = static_cast<int>(args[1].toScalar());                           \
        else if (args.size() >= 3 && !args[2].isEmpty())                          \
            dim = static_cast<int>(args[2].toScalar());                           \
        outs[0] = fn(args[0], dim, ctx.engine->resource());                      \
    }

NK_NAN_MAXMIN_ADAPTER(nanmax, nanmax)
NK_NAN_MAXMIN_ADAPTER(nanmin, nanmin)

#undef NK_NAN_MAXMIN_ADAPTER

void nanvar_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("nanvar: requires at least 1 argument",
                     0, 0, "nanvar", "", "nk:nanvar:nargin");
    int w = 0, dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        w = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = nanvar(args[0], w, dim, ctx.engine->resource());
}

void nanstd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("nanstd: requires at least 1 argument",
                     0, 0, "nanstd", "", "nk:nanstd:nargin");
    int w = 0, dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        w = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = nanstdev(args[0], w, dim, ctx.engine->resource());
}

// nancov has two MATLAB call patterns:
//   nancov(X)                   — covariance matrix of X
//   nancov(X, normFlag)         — normalization 0 (n-1) or 1 (n)
//   nancov(X, Y)                — between two vectors → 2×2
//   nancov(X, Y, normFlag)      — same + normalization
// We disambiguate by arg-2 type: scalar → normFlag; non-scalar → Y.
void nancov_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("nancov: requires at least 1 argument",
                     0, 0, "nancov", "", "nk:nancov:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = nancov(args[0], 0, mr);
        return;
    }
    // args.size() >= 2
    const bool secondIsScalar = (args[1].numel() == 1) && !args[1].isEmpty();
    if (secondIsScalar) {
        const int w = static_cast<int>(args[1].toScalar());
        outs[0] = nancov(args[0], w, mr);
    } else {
        int w = 0;
        if (args.size() >= 3 && !args[2].isEmpty())
            w = static_cast<int>(args[2].toScalar());
        outs[0] = nancov(args[0], args[1], w, mr);
    }
}

} // namespace detail

} // namespace nk::stats
