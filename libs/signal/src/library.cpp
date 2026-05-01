// libs/signal/src/library.cpp
//
// Registration hub for the Signal Processing Toolbox builtins.
// Namespace layout — sub-namespaces mirror libs/signal/src/<sub>/
// directories (NAMESPACE_DESIGN.md §5, §9.2).
//
// Convention: every signal function gets a *dual* registration —
//   1. signal.<sub>.<name>      (e.g. signal.transforms.fft)
//   2. compat.<name>            (so `import compat.*` flattens it)
// 6 functions additionally get a third registration in core (whitelist
// of cross-domain general-purpose ops): fft, ifft, fftshift, ifftshift,
// conv, xcorr.

#include <numkit/signal/library.hpp>

#include <numkit/core/types.hpp>  // ExternalFunc, CallContext, Span, Value

namespace numkit::signal::detail {
// Forward declarations for adapters implemented in dedicated files
// (public C++ API functions live in numkit::signal; these are their
// Engine-registration bridges).
void fft_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void ifft_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void conv_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void deconv_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void xcorr_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void filter_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void filtfilt_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void butter_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void fir1_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void freqz_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void phasez_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void grpdelay_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
// filter_analysis/responses.cpp (D1)
void impz_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void impzlength_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void stepz_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void phasedelay_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void zerophase_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
// filter_analysis/predicates.cpp (D1)
void isfir_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void isstable_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void isminphase_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ismaxphase_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void islinphase_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void isallpass_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void downsample_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void upsample_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void decimate_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void resample_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void periodogram_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void pwelch_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void spectrogram_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void hamming_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void hann_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void blackman_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void kaiser_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void rectwin_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void bartlett_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void triang_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void tukeywin_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void flattopwin_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void gausswin_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void chebwin_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void parzenwin_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void nuttallwin_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void taylorwin_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void blackmanharris_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void bohmanwin_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void barthannwin_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void unwrap_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void hilbert_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void envelope_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void nextpow2_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void fftshift_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void ifftshift_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void chirp_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void rectpuls_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void tripuls_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void gauspuls_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void pulstran_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void square_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void sawtooth_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void sinc_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void gmonopuls_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void diric_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);

// Phase 9 — DSP gaps
void medfilt1_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void findpeaks_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void goertzel_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void dct_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void idct_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);

// SOS family (libs/signal/src/digital_filtering/sosfilt.cpp)
void sosfilt_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void zp2sos_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void tf2sos_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);

// Savitzky-Golay (libs/signal/src/smoothing/sgolay.cpp)
void sgolay_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void sgolayfilt_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);

// dB conversions (libs/signal/src/measurements/dbconv.cpp)
void db_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void db2mag_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void mag2db_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void db2pow_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void pow2db_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);

// Signal stats (libs/signal/src/measurements/signal_stats.cpp)
void rms_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void rssq_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void peak2peak_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void peak2rms_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
} // namespace numkit::signal::detail

namespace numkit {

void SignalLibrary::install(Engine &engine)
{
    // Local helper — signal is MATLAB-mirror, every fn registered in
    // signal.<sub>.<name> AND aliased into compat.<name>.
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("signal.") + sub, name, fn);
        engine.registerFunction("compat", name, fn);
    };

    // ── Transforms (FFT family, DCT, Hilbert, Goertzel, envelope, ...) ──
    reg("transforms", "fft",       &signal::detail::fft_reg);
    reg("transforms", "ifft",      &signal::detail::ifft_reg);
    reg("transforms", "fftshift",  &signal::detail::fftshift_reg);
    reg("transforms", "ifftshift", &signal::detail::ifftshift_reg);
    reg("transforms", "dct",       &signal::detail::dct_reg);
    reg("transforms", "idct",      &signal::detail::idct_reg);
    reg("transforms", "hilbert",   &signal::detail::hilbert_reg);
    reg("transforms", "envelope",  &signal::detail::envelope_reg);
    reg("transforms", "goertzel",  &signal::detail::goertzel_reg);
    reg("transforms", "nextpow2",  &signal::detail::nextpow2_reg);
    reg("transforms", "unwrap",    &signal::detail::unwrap_reg);

    // ── Convolution / correlation ──────────────────────────────────────
    reg("convolution", "conv",   &signal::detail::conv_reg);
    reg("convolution", "deconv", &signal::detail::deconv_reg);
    reg("convolution", "xcorr",  &signal::detail::xcorr_reg);

    // ── Digital filtering (filter / filtfilt / SOS family / median) ────
    reg("digital_filtering", "filter",   &signal::detail::filter_reg);
    reg("digital_filtering", "filtfilt", &signal::detail::filtfilt_reg);
    reg("digital_filtering", "sosfilt",  &signal::detail::sosfilt_reg);
    reg("digital_filtering", "medfilt1", &signal::detail::medfilt1_reg);

    // ── Filter design (FIR/IIR coefficient generators) ─────────────────
    reg("filter_design", "butter",     &signal::detail::butter_reg);
    reg("filter_design", "fir1",       &signal::detail::fir1_reg);
    reg("filter_design", "sgolay",     &signal::detail::sgolay_reg);
    reg("filter_design", "sgolayfilt", &signal::detail::sgolayfilt_reg);

    // ── Filter analysis (freqz / phasez / grpdelay + responses + preds) ─
    reg("filter_analysis", "freqz",      &signal::detail::freqz_reg);
    reg("filter_analysis", "phasez",     &signal::detail::phasez_reg);
    reg("filter_analysis", "grpdelay",   &signal::detail::grpdelay_reg);
    reg("filter_analysis", "impz",       &signal::detail::impz_reg);
    reg("filter_analysis", "impzlength", &signal::detail::impzlength_reg);
    reg("filter_analysis", "stepz",      &signal::detail::stepz_reg);
    reg("filter_analysis", "phasedelay", &signal::detail::phasedelay_reg);
    reg("filter_analysis", "zerophase",  &signal::detail::zerophase_reg);
    reg("filter_analysis", "isfir",      &signal::detail::isfir_reg);
    reg("filter_analysis", "isstable",   &signal::detail::isstable_reg);
    reg("filter_analysis", "isminphase", &signal::detail::isminphase_reg);
    reg("filter_analysis", "ismaxphase", &signal::detail::ismaxphase_reg);
    reg("filter_analysis", "islinphase", &signal::detail::islinphase_reg);
    reg("filter_analysis", "isallpass",  &signal::detail::isallpass_reg);

    // ── Filter implementation (form conversions: TF/SOS/ZPK) ───────────
    reg("filter_implementation", "tf2sos", &signal::detail::tf2sos_reg);
    reg("filter_implementation", "zp2sos", &signal::detail::zp2sos_reg);

    // ── Multirate (decimate / interp / resample) ───────────────────────
    reg("multirate", "downsample", &signal::detail::downsample_reg);
    reg("multirate", "upsample",   &signal::detail::upsample_reg);
    reg("multirate", "decimate",   &signal::detail::decimate_reg);
    reg("multirate", "resample",   &signal::detail::resample_reg);

    // ── Spectral analysis (pwelch / periodogram) ───────────────────────
    reg("spectral_analysis", "periodogram", &signal::detail::periodogram_reg);
    reg("spectral_analysis", "pwelch",      &signal::detail::pwelch_reg);

    // ── Time-frequency (spectrogram / STFT family) ─────────────────────
    reg("time_frequency", "spectrogram", &signal::detail::spectrogram_reg);

    // ── Windows (hamming / hann / blackman / kaiser / rectwin / bartlett) ─
    reg("windows", "hamming",  &signal::detail::hamming_reg);
    reg("windows", "hann",     &signal::detail::hann_reg);
    reg("windows", "blackman", &signal::detail::blackman_reg);
    reg("windows", "kaiser",   &signal::detail::kaiser_reg);
    reg("windows", "rectwin",  &signal::detail::rectwin_reg);
    reg("windows", "bartlett", &signal::detail::bartlett_reg);
    reg("windows", "triang",         &signal::detail::triang_reg);
    reg("windows", "tukeywin",       &signal::detail::tukeywin_reg);
    reg("windows", "flattopwin",     &signal::detail::flattopwin_reg);
    reg("windows", "gausswin",       &signal::detail::gausswin_reg);
    reg("windows", "chebwin",        &signal::detail::chebwin_reg);
    reg("windows", "parzenwin",      &signal::detail::parzenwin_reg);
    reg("windows", "nuttallwin",     &signal::detail::nuttallwin_reg);
    reg("windows", "taylorwin",      &signal::detail::taylorwin_reg);
    reg("windows", "blackmanharris", &signal::detail::blackmanharris_reg);
    reg("windows", "bohmanwin",      &signal::detail::bohmanwin_reg);
    reg("windows", "barthannwin",    &signal::detail::barthannwin_reg);
    // MATLAB legacy alias `hanning` → also points at hann_reg. compat
    // already has 'hann'; this aliases the same impl into compat as
    // 'hanning' too (separate compat entry — different short-name).
    reg("windows", "hanning",  &signal::detail::hann_reg);

    // ── Waveform generation (chirp / pulses) ───────────────────────────
    reg("waveform_generation", "chirp",     &signal::detail::chirp_reg);
    reg("waveform_generation", "rectpuls",  &signal::detail::rectpuls_reg);
    reg("waveform_generation", "tripuls",   &signal::detail::tripuls_reg);
    reg("waveform_generation", "gauspuls",  &signal::detail::gauspuls_reg);
    reg("waveform_generation", "pulstran",  &signal::detail::pulstran_reg);
    reg("waveform_generation", "square",    &signal::detail::square_reg);
    reg("waveform_generation", "sawtooth",  &signal::detail::sawtooth_reg);
    reg("waveform_generation", "sinc",      &signal::detail::sinc_reg);
    reg("waveform_generation", "gmonopuls", &signal::detail::gmonopuls_reg);
    reg("waveform_generation", "diric",     &signal::detail::diric_reg);

    // ── Measurements (findpeaks, dB conv, signal stats) ────────────────
    reg("measurements", "findpeaks", &signal::detail::findpeaks_reg);
    reg("measurements", "db",        &signal::detail::db_reg);
    reg("measurements", "db2mag",    &signal::detail::db2mag_reg);
    reg("measurements", "mag2db",    &signal::detail::mag2db_reg);
    reg("measurements", "db2pow",    &signal::detail::db2pow_reg);
    reg("measurements", "pow2db",    &signal::detail::pow2db_reg);
    reg("measurements", "rms",       &signal::detail::rms_reg);
    reg("measurements", "rssq",      &signal::detail::rssq_reg);
    reg("measurements", "peak2peak", &signal::detail::peak2peak_reg);
    reg("measurements", "peak2rms",  &signal::detail::peak2rms_reg);

    // ── Core promotions (NAMESPACE_DESIGN.md §7, closed whitelist) ─────
    // These 6 functions are general-purpose and reachable by short name
    // even WITHOUT `import compat.*`. They get a third registration in
    // core (namespace = ""). The same ExternalFunc pointer is shared
    // across all three registrations.
    engine.registerFunction("", "fft",       &signal::detail::fft_reg);
    engine.registerFunction("", "ifft",      &signal::detail::ifft_reg);
    engine.registerFunction("", "fftshift",  &signal::detail::fftshift_reg);
    engine.registerFunction("", "ifftshift", &signal::detail::ifftshift_reg);
    engine.registerFunction("", "conv",      &signal::detail::conv_reg);
    engine.registerFunction("", "xcorr",     &signal::detail::xcorr_reg);
}

} // namespace numkit
