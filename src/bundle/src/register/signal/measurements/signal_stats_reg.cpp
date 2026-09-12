// toolboxes/signal/src/measurements/signal_stats_reg.cpp
//
// CallContext register half of measurements/signal_stats.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/signal/measurements/signal_stats.hpp>
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

namespace nk::signal {

namespace detail {

static int dimFromArg(Span<const Value> args)
{
    return (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
}

void rms_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rms: requires at least 1 argument",
                     0, 0, "rms", "", "nk:rms:nargin");
    outs[0] = rms(args[0], dimFromArg(args), ctx.engine->resource());
}

void rssq_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rssq: requires at least 1 argument",
                     0, 0, "rssq", "", "nk:rssq:nargin");
    outs[0] = rssq(args[0], dimFromArg(args), ctx.engine->resource());
}

void peak2peak_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("peak2peak: requires at least 1 argument",
                     0, 0, "peak2peak", "", "nk:peak2peak:nargin");
    outs[0] = peak2peak(args[0], dimFromArg(args), ctx.engine->resource());
}

void peak2rms_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("peak2rms: requires at least 1 argument",
                     0, 0, "peak2rms", "", "nk:peak2rms:nargin");
    outs[0] = peak2rms(args[0], dimFromArg(args), ctx.engine->resource());
}

} // namespace detail

} // namespace nk::signal
