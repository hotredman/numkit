// libs/audio/src/spectral/shape_descriptors_reg.cpp
//
// Register half of the spectral-shape descriptors: the CallContext builtins
// spectralCentroid / Spread / RolloffPoint / Decrease / Slope / Crest /
// Entropy / Flatness / Kurtosis / Skewness / Flux that delegate to the
// engine-free compute in shape_descriptors.cpp. library.cpp forward-declares
// + registers these by name.
//
// Compute reconciled to the canonical mr-last API (matching the header) as
// part of this split. Phase 2b — see project_layering_refactor memory.

#include <numkit/audio/spectral/shape_descriptors.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

namespace numkit::audio {
namespace detail {

// Per-fn registration adapters share the (x, f) argument shape.
#define NK_SPEC_REG(FN)                                                          \
    void FN##_reg(Span<const Value> args, size_t /*nargout*/,                    \
                  Span<Value> outs, CallContext &ctx)                            \
    {                                                                            \
        if (args.size() < 2)                                                     \
            throw Error(#FN ": requires (x, fs) or (X, F)",                      \
                        0, 0, #FN, "", "numkit:" #FN ":nargin");                      \
        outs[0] = FN(args[0], args[1], ctx.engine->resource());                  \
    }

NK_SPEC_REG(spectralCentroid)
NK_SPEC_REG(spectralSpread)
NK_SPEC_REG(spectralDecrease)
NK_SPEC_REG(spectralSlope)
NK_SPEC_REG(spectralCrest)
NK_SPEC_REG(spectralEntropy)
NK_SPEC_REG(spectralFlatness)
NK_SPEC_REG(spectralKurtosis)
NK_SPEC_REG(spectralSkewness)

#undef NK_SPEC_REG

void spectralRolloffPoint_reg(Span<const Value> args, size_t /*nargout*/,
                              Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("spectralRolloffPoint: requires (x, fs [, threshold])",
                    0, 0, "spectralRolloffPoint", "",
                    "numkit:spectralRolloffPoint:nargin");
    double pct = 0.95;
    if (args.size() >= 3) pct = args[2].toScalar();
    outs[0] = spectralRolloffPoint(args[0], args[1], pct,
                                    ctx.engine->resource());
}

void spectralFlux_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("spectralFlux: requires (x, fs [, p])",
                    0, 0, "spectralFlux", "", "numkit:spectralFlux:nargin");
    double p = 2.0;
    if (args.size() >= 3) p = args[2].toScalar();
    outs[0] = spectralFlux(args[0], args[1], p, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::audio
