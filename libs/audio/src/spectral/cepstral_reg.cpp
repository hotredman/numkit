// libs/audio/src/spectral/cepstral_reg.cpp
//
// Register half of the cepstral builtins: the CallContext wrappers
// cepstralCoefficients / mfcc / gtcc (arg parsing + multi-output delta
// packing) that delegate to the engine-free compute in cepstral.cpp.
// library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/audio/spectral/cepstral.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

namespace numkit::audio {
namespace detail {

void cepstralCoefficients_reg(Span<const Value> args, size_t /*nargout*/,
                               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cepstralCoefficients: requires (S [, NumCoeffs])",
                    0, 0, "cepstralCoefficients", "",
                    "numkit:cepstralCoefficients:nargin");
    int nc = 13;
    if (args.size() >= 2) nc = static_cast<int>(args[1].toScalar());
    outs[0] = cepstralCoefficients(args[0], nc, ctx.engine->resource());
}

void mfcc_reg(Span<const Value> args, size_t nargout,
              Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mfcc: requires (x, fs [, NumCoeffs])",
                    0, 0, "mfcc", "", "numkit:mfcc:nargin");
    int nc = 13;
    if (args.size() >= 3) nc = static_cast<int>(args[2].toScalar());
    auto [c, d, dd] = mfcc(args[0], args[1].toScalar(), nc,
                            ctx.engine->resource());
    outs[0] = c;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = d;
    if (nargout >= 3 && outs.size() >= 3) outs[2] = dd;
}

void gtcc_reg(Span<const Value> args, size_t nargout,
              Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("gtcc: requires (x, fs [, NumCoeffs])",
                    0, 0, "gtcc", "", "numkit:gtcc:nargin");
    int nc = 13;
    if (args.size() >= 3) nc = static_cast<int>(args[2].toScalar());
    auto [c, d, dd] = gtcc(args[0], args[1].toScalar(), nc,
                            ctx.engine->resource());
    outs[0] = c;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = d;
    if (nargout >= 3 && outs.size() >= 3) outs[2] = dd;
}

} // namespace detail
} // namespace numkit::audio
