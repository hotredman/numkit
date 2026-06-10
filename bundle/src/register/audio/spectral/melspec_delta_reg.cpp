// toolboxes/audio/src/spectral/melspec_delta_reg.cpp
//
// Register half of the melSpectrogram / audioDelta builtins: the
// CallContext wrappers that delegate to the engine-free compute in
// melspec_delta.cpp. library.cpp forward-declares + registers these.
//
// melSpectrogram compute reconciled to the canonical mr-last API (matching
// the header) as part of this split. Phase 2b — see project_layering_refactor.

#include <numkit/audio/spectral/melspec_delta.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

namespace numkit::audio {
namespace detail {

void melSpectrogram_reg(Span<const Value> args, size_t nargout,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("melSpectrogram: requires (x, fs [, NumBands])",
                    0, 0, "melSpectrogram", "", "numkit:melSpectrogram:nargin");
    int numBands = 32;
    if (args.size() >= 3) numBands = static_cast<int>(args[2].toScalar());
    auto [S, F, T] = melSpectrogram(args[0], args[1].toScalar(), numBands,
                                     ctx.engine->resource());
    outs[0] = S;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = F;
    if (nargout >= 3 && outs.size() >= 3) outs[2] = T;
}

void audioDelta_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("audioDelta: requires (x [, windowLength])",
                    0, 0, "audioDelta", "", "numkit:audioDelta:nargin");
    int wl = 9;
    if (args.size() >= 2) wl = static_cast<int>(args[1].toScalar());
    outs[0] = audioDelta(args[0], wl, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::audio
