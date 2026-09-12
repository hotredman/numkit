// toolboxes/signal/src/measurements/sigroi_reg.cpp
//
// CallContext register half of measurements/sigroi.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/signal/measurements/sigroi.hpp>
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

namespace nk::signal {

namespace detail {

void binmask2sigroi_reg(Span<const Value> args, size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("binmask2sigroi: requires (mask)",
                    0, 0, "binmask2sigroi", "", "nk:binmask2sigroi:nargin");
    outs[0] = binmask2sigroi(args[0], ctx.engine->resource());
}

void sigroi2binmask_reg(Span<const Value> args, size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sigroi2binmask: requires (roi [, len])",
                    0, 0, "sigroi2binmask", "", "nk:sigroi2binmask:nargin");
    int64_t len = -1;
    if (args.size() >= 2) len = static_cast<int64_t>(args[1].toScalar());
    outs[0] = sigroi2binmask(args[0], len, ctx.engine->resource());
}

void extendsigroi_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("extendsigroi: requires (roi, Lpre, Lpost)",
                    0, 0, "extendsigroi", "", "nk:extendsigroi:nargin");
    outs[0] = extendsigroi(args[0],
                           static_cast<int64_t>(args[1].toScalar()),
                           static_cast<int64_t>(args[2].toScalar()),
                           ctx.engine->resource());
}

void shortensigroi_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("shortensigroi: requires (roi, Lpre, Lpost)",
                    0, 0, "shortensigroi", "", "nk:shortensigroi:nargin");
    outs[0] = shortensigroi(args[0],
                            static_cast<int64_t>(args[1].toScalar()),
                            static_cast<int64_t>(args[2].toScalar()),
                            ctx.engine->resource());
}

void mergesigroi_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mergesigroi: requires (roi, sep)",
                    0, 0, "mergesigroi", "", "nk:mergesigroi:nargin");
    outs[0] = mergesigroi(args[0],
                          static_cast<int64_t>(args[1].toScalar()),
                          ctx.engine->resource());
}

void removesigroi_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("removesigroi: requires (roi, maxLen)",
                    0, 0, "removesigroi", "", "nk:removesigroi:nargin");
    outs[0] = removesigroi(args[0],
                           static_cast<int64_t>(args[1].toScalar()),
                           ctx.engine->resource());
}

void extractsigroi_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("extractsigroi: requires (x, roi [, concat])",
                    0, 0, "extractsigroi", "", "nk:extractsigroi:nargin");
    bool concat = false;
    if (args.size() >= 3) concat = (args[2].toScalar() != 0.0);
    outs[0] = extractsigroi(args[0], args[1], concat, ctx.engine->resource());
}

void sigrangebinmask_reg(Span<const Value> args, size_t /*nargout*/,
                         Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("sigrangebinmask: requires (x, bound) where bound is "
                    "scalar (above) or 2-vec [vmin vmax] (inside)",
                    0, 0, "sigrangebinmask", "", "nk:sigrangebinmask:nargin");
    auto *mr = ctx.engine->resource();
    const Value &bound = args[1];
    if (bound.numel() == 1)
        outs[0] = sigrangebinmask(args[0], bound.toScalar(), mr);
    else if (bound.numel() == 2)
        outs[0] = sigrangebinmask(args[0], bound.elemAsDouble(0),
                                  bound.elemAsDouble(1), mr);
    else
        throw Error("sigrangebinmask: bound must be scalar or 2-element vector",
                    0, 0, "sigrangebinmask", "", "nk:sigrangebinmask:BadBound");
}

} // namespace detail

} // namespace nk::signal
