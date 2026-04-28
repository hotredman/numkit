#include <numkit/m/signal/MSignalLibrary.hpp>

#include <numkit/m/core/MTypes.hpp>  // ExternalFunc, CallContext, Span, MValue

namespace numkit::m::signal::detail {
// Forward declarations for adapters implemented in dedicated files
// (public C++ API functions live in numkit::m::signal; these are their
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
void phasez_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void grpdelay_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
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
void rectpuls_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void tripuls_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void gauspuls_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void pulstran_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);

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

// Savitzky-Golay (libs/dsp/src/MDspSgolay.cpp)
void sgolay_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void sgolayfilt_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
} // namespace numkit::m::signal::detail

namespace numkit::m {

void SignalLibrary::install(Engine &engine)
{
    // ── Public-API-backed built-ins (Phase 5 pilot and onwards) ─────
    engine.registerFunction("fft",         &signal::detail::fft_reg);
    engine.registerFunction("ifft",        &signal::detail::ifft_reg);
    engine.registerFunction("conv",        &signal::detail::conv_reg);
    engine.registerFunction("deconv",      &signal::detail::deconv_reg);
    engine.registerFunction("xcorr",       &signal::detail::xcorr_reg);
    engine.registerFunction("filter",      &signal::detail::filter_reg);
    engine.registerFunction("filtfilt",    &signal::detail::filtfilt_reg);
    engine.registerFunction("butter",      &signal::detail::butter_reg);
    engine.registerFunction("fir1",        &signal::detail::fir1_reg);
    engine.registerFunction("freqz",       &signal::detail::freqz_reg);
    engine.registerFunction("phasez",      &signal::detail::phasez_reg);
    engine.registerFunction("grpdelay",    &signal::detail::grpdelay_reg);
    engine.registerFunction("downsample",  &signal::detail::downsample_reg);
    engine.registerFunction("upsample",    &signal::detail::upsample_reg);
    engine.registerFunction("decimate",    &signal::detail::decimate_reg);
    engine.registerFunction("resample",    &signal::detail::resample_reg);
    engine.registerFunction("periodogram", &signal::detail::periodogram_reg);
    engine.registerFunction("pwelch",      &signal::detail::pwelch_reg);
    engine.registerFunction("spectrogram", &signal::detail::spectrogram_reg);
    engine.registerFunction("hamming",     &signal::detail::hamming_reg);
    engine.registerFunction("hann",        &signal::detail::hann_reg);
    engine.registerFunction("hanning",     &signal::detail::hann_reg);   // MATLAB alias
    engine.registerFunction("blackman",    &signal::detail::blackman_reg);
    engine.registerFunction("kaiser",      &signal::detail::kaiser_reg);
    engine.registerFunction("rectwin",     &signal::detail::rectwin_reg);
    engine.registerFunction("bartlett",    &signal::detail::bartlett_reg);
    engine.registerFunction("unwrap",      &signal::detail::unwrap_reg);
    engine.registerFunction("hilbert",     &signal::detail::hilbert_reg);
    engine.registerFunction("envelope",    &signal::detail::envelope_reg);
    engine.registerFunction("nextpow2",    &signal::detail::nextpow2_reg);
    engine.registerFunction("fftshift",    &signal::detail::fftshift_reg);
    engine.registerFunction("ifftshift",   &signal::detail::ifftshift_reg);
    engine.registerFunction("chirp",       &signal::detail::chirp_reg);
    engine.registerFunction("rectpuls",    &signal::detail::rectpuls_reg);
    engine.registerFunction("tripuls",     &signal::detail::tripuls_reg);
    engine.registerFunction("gauspuls",    &signal::detail::gauspuls_reg);
    engine.registerFunction("pulstran",    &signal::detail::pulstran_reg);

    // ── Phase 9 DSP gaps ──────────────────────────────────────────
    engine.registerFunction("medfilt1",  &signal::detail::medfilt1_reg);
    engine.registerFunction("findpeaks", &signal::detail::findpeaks_reg);
    engine.registerFunction("goertzel",  &signal::detail::goertzel_reg);
    engine.registerFunction("dct",       &signal::detail::dct_reg);
    engine.registerFunction("idct",      &signal::detail::idct_reg);

    // ── SOS filter family ─────────────────────────────────────────
    engine.registerFunction("sosfilt",   &signal::detail::sosfilt_reg);
    engine.registerFunction("zp2sos",    &signal::detail::zp2sos_reg);
    engine.registerFunction("tf2sos",    &signal::detail::tf2sos_reg);

    // ── Savitzky-Golay ─────────────────────────────────────────────
    engine.registerFunction("sgolay",     &signal::detail::sgolay_reg);
    engine.registerFunction("sgolayfilt", &signal::detail::sgolayfilt_reg);
}

} // namespace numkit::m
