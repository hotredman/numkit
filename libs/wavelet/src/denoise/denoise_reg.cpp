// libs/wavelet/src/denoise/denoise_reg.cpp
//
// Register half of the thresholding / denoising primitives: the CallContext
// builtins wthresh / wnoisest / wdenoise that delegate to the engine-free
// compute in denoise.cpp. library.cpp forward-declares + registers these.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/wavelet/denoise/denoise.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <string>

namespace numkit::wavelet {
namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("wavelet: expected string argument",
                    0, 0, "", "", "numkit:wavelet:type");
    return v.toString();
}

void wthresh_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("wthresh: requires (X, sorh, T)",
                    0, 0, "wthresh", "", "numkit:wthresh:nargin");
    outs[0] = wthresh(args[0], argString(args[1]), args[2].toScalar(), ctx.engine->resource());
}

void wnoisest_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("wnoisest: requires (C, L, S)",
                    0, 0, "wnoisest", "", "numkit:wnoisest:nargin");
    outs[0] = wnoisest(args[0], args[1], args[2], ctx.engine->resource());
}

void wdenoise_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("wdenoise: requires the signal",
                    0, 0, "wdenoise", "", "numkit:wdenoise:nargin");
    int level = -1;
    if (args.size() >= 2 && !args[1].isEmpty())
        level = static_cast<int>(args[1].toScalar());
    std::string wname;
    if (args.size() >= 3 && !args[2].isEmpty())
        wname = argString(args[2]);
    outs[0] = wdenoise(args[0], level, wname, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::wavelet
