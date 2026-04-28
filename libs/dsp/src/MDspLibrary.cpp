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
void hamming_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void hann_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void blackman_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void kaiser_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void rectwin_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void bartlett_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void unwrap_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void hilbert_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void envelope_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void nextpow2_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void fftshift_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void ifftshift_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void chirp_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);

// Phase 9 — DSP gaps (libs/dsp/src/MDspGaps.cpp)
void medfilt1_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void findpeaks_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void goertzel_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void dct_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void idct_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);

// SOS family (libs/dsp/src/MDspSos.cpp)
void sosfilt_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void zp2sos_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void tf2sos_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
} // namespace numkit::m::dsp::detail

namespace numkit::m {

void DspLibrary::install(Engine &engine)
{
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
    engine.registerFunction("hamming",     &dsp::detail::hamming_reg);
    engine.registerFunction("hann",        &dsp::detail::hann_reg);
    engine.registerFunction("hanning",     &dsp::detail::hann_reg);   // MATLAB alias
    engine.registerFunction("blackman",    &dsp::detail::blackman_reg);
    engine.registerFunction("kaiser",      &dsp::detail::kaiser_reg);
    engine.registerFunction("rectwin",     &dsp::detail::rectwin_reg);
    engine.registerFunction("bartlett",    &dsp::detail::bartlett_reg);
    engine.registerFunction("unwrap",      &dsp::detail::unwrap_reg);
    engine.registerFunction("hilbert",     &dsp::detail::hilbert_reg);
    engine.registerFunction("envelope",    &dsp::detail::envelope_reg);
    engine.registerFunction("nextpow2",    &dsp::detail::nextpow2_reg);
    engine.registerFunction("fftshift",    &dsp::detail::fftshift_reg);
    engine.registerFunction("ifftshift",   &dsp::detail::ifftshift_reg);
    engine.registerFunction("chirp",       &dsp::detail::chirp_reg);

    // ── Phase 9 DSP gaps ──────────────────────────────────────────
    engine.registerFunction("medfilt1",  &dsp::detail::medfilt1_reg);
    engine.registerFunction("findpeaks", &dsp::detail::findpeaks_reg);
    engine.registerFunction("goertzel",  &dsp::detail::goertzel_reg);
    engine.registerFunction("dct",       &dsp::detail::dct_reg);
    engine.registerFunction("idct",      &dsp::detail::idct_reg);

    // ── SOS filter family ─────────────────────────────────────────
    engine.registerFunction("sosfilt",   &dsp::detail::sosfilt_reg);
    engine.registerFunction("zp2sos",    &dsp::detail::zp2sos_reg);
    engine.registerFunction("tf2sos",    &dsp::detail::tf2sos_reg);
}

} // namespace numkit::m
