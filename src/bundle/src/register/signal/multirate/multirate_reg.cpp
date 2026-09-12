// toolboxes/signal/src/multirate/multirate_reg.cpp
//
// Register half of the signal multirate builtins: the CallContext wrappers
// delegating to the engine-free compute in multirate.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <nk/signal/multirate/multirate.hpp>

#include <nk/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <nk/value/value.hpp>
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace nk::signal {

namespace detail {

void downsample_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("downsample: requires 2 arguments",
                     0, 0, "downsample", "", "nk:downsample:nargin");
    const size_t n = static_cast<size_t>(args[1].toScalar());
    size_t phase = 0;
    if (args.size() >= 3) {
        phase = static_cast<size_t>(args[2].toScalar());
        if (phase >= n)
            throw Error("downsample: phase must be an integer in [0, n-1]",
                         0, 0, "downsample", "", "nk:downsample:badPhase");
    }
    outs[0] = downsample(args[0], n, ctx.engine->resource(), phase);
}

void upsample_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("upsample: requires 2 arguments",
                     0, 0, "upsample", "", "nk:upsample:nargin");
    const size_t n = static_cast<size_t>(args[1].toScalar());
    size_t phase = 0;
    if (args.size() >= 3) {
        phase = static_cast<size_t>(args[2].toScalar());
        if (phase >= n)
            throw Error("upsample: phase must be an integer in [0, n-1]",
                         0, 0, "upsample", "", "nk:upsample:badPhase");
    }
    outs[0] = upsample(args[0], n, ctx.engine->resource(), phase);
}

void decimate_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("decimate: requires 2 arguments",
                     0, 0, "decimate", "", "nk:decimate:nargin");
    outs[0] = decimate(args[0], static_cast<size_t>(args[1].toScalar()), ctx.engine->resource());
}

void resample_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("resample: requires 3 arguments",
                     0, 0, "resample", "", "nk:resample:nargin");
    outs[0] = resample(args[0], static_cast<size_t>(args[1].toScalar()), static_cast<size_t>(args[2].toScalar()), ctx.engine->resource());
}

} // namespace detail

} // namespace nk::signal
