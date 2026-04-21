#include <numkit/m/dsp/MDspLibrary.hpp>

#include <numkit/m/core/MTypes.hpp>  // ExternalFunc, CallContext, Span, MValue

namespace numkit::m::dsp::detail {
// Forward declarations for adapters implemented in dedicated files
// (public C++ API functions live in numkit::m::dsp; these are their
// Engine-registration bridges).
void fft_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void ifft_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void conv_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void deconv_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void xcorr_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void filter_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void filtfilt_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void butter_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void fir1_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void freqz_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
} // namespace numkit::m::dsp::detail

namespace numkit::m {

void DspLibrary::install(Engine &engine)
{
    registerSignalCoreFunctions(engine);
    registerWindowFunctions(engine);
    registerSpectralFunctions(engine);
    registerResampleFunctions(engine);
    registerTransformFunctions(engine);

    // ── Public-API-backed built-ins (Phase 5 pilot and onwards) ─────
    engine.registerFunction("fft",      &dsp::detail::fft_reg);
    engine.registerFunction("ifft",     &dsp::detail::ifft_reg);
    engine.registerFunction("conv",     &dsp::detail::conv_reg);
    engine.registerFunction("deconv",   &dsp::detail::deconv_reg);
    engine.registerFunction("xcorr",    &dsp::detail::xcorr_reg);
    engine.registerFunction("filter",   &dsp::detail::filter_reg);
    engine.registerFunction("filtfilt", &dsp::detail::filtfilt_reg);
    engine.registerFunction("butter",   &dsp::detail::butter_reg);
    engine.registerFunction("fir1",     &dsp::detail::fir1_reg);
    engine.registerFunction("freqz",    &dsp::detail::freqz_reg);
}

} // namespace numkit::m
