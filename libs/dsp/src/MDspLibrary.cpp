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
void downsample_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void upsample_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void decimate_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void resample_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void periodogram_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void pwelch_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void spectrogram_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
} // namespace numkit::m::dsp::detail

namespace numkit::m {

void DspLibrary::install(Engine &engine)
{
    registerSignalCoreFunctions(engine);
    registerWindowFunctions(engine);
    registerTransformFunctions(engine);

    // ── Public-API-backed built-ins (Phase 5 pilot and onwards) ─────
    engine.registerFunction("fft",         &dsp::detail::fft_reg);
    engine.registerFunction("ifft",        &dsp::detail::ifft_reg);
    engine.registerFunction("conv",        &dsp::detail::conv_reg);
    engine.registerFunction("deconv",      &dsp::detail::deconv_reg);
    engine.registerFunction("xcorr",       &dsp::detail::xcorr_reg);
    engine.registerFunction("filter",      &dsp::detail::filter_reg);
    engine.registerFunction("filtfilt",    &dsp::detail::filtfilt_reg);
    engine.registerFunction("butter",      &dsp::detail::butter_reg);
    engine.registerFunction("fir1",        &dsp::detail::fir1_reg);
    engine.registerFunction("freqz",       &dsp::detail::freqz_reg);
    engine.registerFunction("downsample",  &dsp::detail::downsample_reg);
    engine.registerFunction("upsample",    &dsp::detail::upsample_reg);
    engine.registerFunction("decimate",    &dsp::detail::decimate_reg);
    engine.registerFunction("resample",    &dsp::detail::resample_reg);
    engine.registerFunction("periodogram", &dsp::detail::periodogram_reg);
    engine.registerFunction("pwelch",      &dsp::detail::pwelch_reg);
    engine.registerFunction("spectrogram", &dsp::detail::spectrogram_reg);
}

} // namespace numkit::m
