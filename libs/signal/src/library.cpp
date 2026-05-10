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
void fft2_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void ifft2_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void interpft_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void conv_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void deconv_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void xcorr_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void conv2_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void filter2_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void convn_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void xcov_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
// convolution/extras.cpp (E1)
void cconv_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void convmtx_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void xcorr2_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void finddelay_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void alignsignals_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void filter_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void filtfilt_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void buffer_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void uencode_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void udecode_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void polyscale_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void polystab_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void shiftdata_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void unshiftdata_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
// digital_filtering/spec_driven.cpp (D2)
void lowpass_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void highpass_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void bandpass_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void bandstop_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void butter_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void fir1_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void firls_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
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
void filtord_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void firtype_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void filternorm_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void downsample_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void upsample_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void decimate_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void resample_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
// multirate/extras.cpp (F1)
void upfirdn_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void interp_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void intfilt_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void fftfilt_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void periodogram_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void pwelch_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void cpsd_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void mscohere_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void tfestimate_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void pyulear_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void pburg_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
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
// transforms/extras.cpp (E2)
void dftmtx_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void bitrevorder_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void dst_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void idst_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void rceps_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void cceps_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void icceps_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);

// SOS family (libs/signal/src/digital_filtering/sosfilt.cpp)
void sosfilt_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void sosfiltfilt_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void zp2sos_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void tf2sos_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
// filter_implementation/conversions_extras.cpp (D3)
void sos2tf_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void sos2zp_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void tf2zpk_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void tf2ss_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void ss2tf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void ss2zp_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void zp2ss_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void sos2ss_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ss2sos_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

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

// Spectral metrics (libs/signal/src/spectral_analysis/spectral_metrics.cpp)
void bandpower_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void meanfreq_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void medfreq_reg         (Span<const Value>, size_t, Span<Value>, CallContext &);
void enbw_reg            (Span<const Value>, size_t, Span<Value>, CallContext &);
void obw_reg             (Span<const Value>, size_t, Span<Value>, CallContext &);
void powerbw_reg         (Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralcrest_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralflatness_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralentropy_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralkurtosis_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralskewness_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void snr_reg             (Span<const Value>, size_t, Span<Value>, CallContext &);
void sinad_reg           (Span<const Value>, size_t, Span<Value>, CallContext &);
void thd_reg             (Span<const Value>, size_t, Span<Value>, CallContext &);
void sfdr_reg            (Span<const Value>, size_t, Span<Value>, CallContext &);
void instfreq_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void instbw_reg          (Span<const Value>, size_t, Span<Value>, CallContext &);

// Analog filter design (libs/signal/src/filter_design/analog_filters.cpp)
void buttap_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void cheb1ap_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void cheb2ap_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void ellipap_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void besselap_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void lp2lp_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void lp2hp_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void lp2bp_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void lp2bs_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void bilinear_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void impinvar_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void freqs_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);

// Top-level IIR designs (libs/signal/src/filter_design/iir_designs.cpp)
void cheby1_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void cheby2_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void ellip_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void besself_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void buttord_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void cheb1ord_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void cheb2ord_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void kaiserord_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void ellipord_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void firpmord_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// Signal modelling (libs/signal/src/spectral_analysis/signal_modeling.cpp)
void levinson_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void rlevinson_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void aryule_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void arburg_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void lpc_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void invfreqs_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void invfreqz_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ac2poly_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void poly2ac_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void ac2rc_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void schurrc_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void rc2ac_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void poly2rc_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void rc2poly_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void is2rc_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void rc2is_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void lar2rc_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void rc2lar_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void arcov_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void armcov_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void prony_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void corrmtx_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void poly2lsf_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void lsf2poly_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// Vibration analysis (libs/signal/src/measurements/vibration.cpp)
void envspectrum_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void tachorpm_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void rainflow_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void tsa_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);

// Pulse / transition metrics (libs/signal/src/measurements/pulse_metrics.cpp)
void statelevels_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void midcross_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void risetime_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void falltime_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void slewrate_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void overshoot_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void undershoot_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void settlingtime_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void pulsewidth_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void pulseperiod_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void pulsesep_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void dutycycle_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);

// Signal ROI utilities (libs/signal/src/measurements/sigroi.cpp)
void binmask2sigroi_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void sigroi2binmask_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void extendsigroi_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void shortensigroi_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void mergesigroi_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void removesigroi_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void extractsigroi_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void sigrangebinmask_reg(Span<const Value>, size_t, Span<Value>, CallContext &);

// Signal small utilities (libs/signal/src/measurements/sig_utils.cpp)
void seqperiod_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void zerocrossrate_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void cusum_reg         (Span<const Value>, size_t, Span<Value>, CallContext &);
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
    reg("transforms", "fft2",      &signal::detail::fft2_reg);
    reg("transforms", "ifft2",     &signal::detail::ifft2_reg);
    reg("transforms", "fftshift",  &signal::detail::fftshift_reg);
    reg("transforms", "ifftshift", &signal::detail::ifftshift_reg);
    reg("transforms", "interpft",  &signal::detail::interpft_reg);
    reg("transforms", "dct",       &signal::detail::dct_reg);
    reg("transforms", "idct",      &signal::detail::idct_reg);
    reg("transforms", "hilbert",   &signal::detail::hilbert_reg);
    reg("transforms", "envelope",  &signal::detail::envelope_reg);
    reg("transforms", "goertzel",  &signal::detail::goertzel_reg);
    reg("transforms", "nextpow2",  &signal::detail::nextpow2_reg);
    reg("transforms", "unwrap",    &signal::detail::unwrap_reg);
    reg("transforms", "dftmtx",      &signal::detail::dftmtx_reg);
    reg("transforms", "bitrevorder", &signal::detail::bitrevorder_reg);
    reg("transforms", "dst",         &signal::detail::dst_reg);
    reg("transforms", "idst",        &signal::detail::idst_reg);
    reg("transforms", "rceps",       &signal::detail::rceps_reg);
    reg("transforms", "cceps",       &signal::detail::cceps_reg);
    reg("transforms", "icceps",      &signal::detail::icceps_reg);

    // ── Convolution / correlation ──────────────────────────────────────
    reg("convolution", "conv",         &signal::detail::conv_reg);
    reg("convolution", "deconv",       &signal::detail::deconv_reg);
    reg("convolution", "xcorr",        &signal::detail::xcorr_reg);
    reg("convolution", "conv2",        &signal::detail::conv2_reg);
    reg("convolution", "filter2",      &signal::detail::filter2_reg);
    reg("convolution", "convn",        &signal::detail::convn_reg);
    reg("convolution", "xcov",         &signal::detail::xcov_reg);
    reg("convolution", "cconv",        &signal::detail::cconv_reg);
    reg("convolution", "convmtx",      &signal::detail::convmtx_reg);
    reg("convolution", "xcorr2",       &signal::detail::xcorr2_reg);
    reg("convolution", "finddelay",    &signal::detail::finddelay_reg);
    reg("convolution", "alignsignals", &signal::detail::alignsignals_reg);

    // ── Digital filtering (filter / filtfilt / SOS family / median + D2) ─
    reg("digital_filtering", "filter",   &signal::detail::filter_reg);
    reg("digital_filtering", "filtfilt", &signal::detail::filtfilt_reg);
    reg("digital_filtering", "buffer",   &signal::detail::buffer_reg);
    reg("digital_filtering", "uencode",  &signal::detail::uencode_reg);
    reg("digital_filtering", "udecode",  &signal::detail::udecode_reg);
    reg("digital_filtering", "polyscale", &signal::detail::polyscale_reg);
    reg("digital_filtering", "polystab",  &signal::detail::polystab_reg);
    reg("digital_filtering", "shiftdata", &signal::detail::shiftdata_reg);
    reg("digital_filtering", "unshiftdata", &signal::detail::unshiftdata_reg);
    reg("digital_filtering", "sosfilt",     &signal::detail::sosfilt_reg);
    reg("digital_filtering", "sosfiltfilt", &signal::detail::sosfiltfilt_reg);
    reg("digital_filtering", "medfilt1", &signal::detail::medfilt1_reg);
    reg("digital_filtering", "lowpass",  &signal::detail::lowpass_reg);
    reg("digital_filtering", "highpass", &signal::detail::highpass_reg);
    reg("digital_filtering", "bandpass", &signal::detail::bandpass_reg);
    reg("digital_filtering", "bandstop", &signal::detail::bandstop_reg);

    // ── Filter design (FIR/IIR coefficient generators) ─────────────────
    reg("filter_design", "butter",     &signal::detail::butter_reg);
    reg("filter_design", "fir1",       &signal::detail::fir1_reg);
    reg("filter_design", "firls",      &signal::detail::firls_reg);
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
    reg("filter_analysis", "filtord",    &signal::detail::filtord_reg);
    reg("filter_analysis", "firtype",    &signal::detail::firtype_reg);
    reg("filter_analysis", "filternorm", &signal::detail::filternorm_reg);

    // ── Filter implementation (form conversions: TF/SOS/ZPK/SS) ────────
    reg("filter_implementation", "tf2sos", &signal::detail::tf2sos_reg);
    reg("filter_implementation", "zp2sos", &signal::detail::zp2sos_reg);
    reg("filter_implementation", "sos2tf", &signal::detail::sos2tf_reg);
    reg("filter_implementation", "sos2zp", &signal::detail::sos2zp_reg);
    reg("filter_implementation", "tf2zpk", &signal::detail::tf2zpk_reg);
    reg("filter_implementation", "tf2ss",  &signal::detail::tf2ss_reg);
    reg("filter_implementation", "ss2tf",  &signal::detail::ss2tf_reg);
    reg("filter_implementation", "ss2zp",  &signal::detail::ss2zp_reg);
    reg("filter_implementation", "zp2ss",  &signal::detail::zp2ss_reg);
    reg("filter_implementation", "sos2ss", &signal::detail::sos2ss_reg);
    reg("filter_implementation", "ss2sos", &signal::detail::ss2sos_reg);

    // ── Multirate (decimate / interp / resample / + F1 extras) ─────────
    reg("multirate", "downsample", &signal::detail::downsample_reg);
    reg("multirate", "upsample",   &signal::detail::upsample_reg);
    reg("multirate", "decimate",   &signal::detail::decimate_reg);
    reg("multirate", "resample",   &signal::detail::resample_reg);
    reg("multirate", "upfirdn",    &signal::detail::upfirdn_reg);
    reg("multirate", "interp",     &signal::detail::interp_reg);
    reg("multirate", "intfilt",    &signal::detail::intfilt_reg);
    reg("multirate", "fftfilt",    &signal::detail::fftfilt_reg);

    // ── Spectral analysis (pwelch / periodogram) ───────────────────────
    reg("spectral_analysis", "periodogram", &signal::detail::periodogram_reg);
    reg("spectral_analysis", "pwelch",      &signal::detail::pwelch_reg);
    reg("spectral_analysis", "cpsd",        &signal::detail::cpsd_reg);
    reg("spectral_analysis", "mscohere",    &signal::detail::mscohere_reg);
    reg("spectral_analysis", "tfestimate",  &signal::detail::tfestimate_reg);
    reg("spectral_analysis", "pyulear",     &signal::detail::pyulear_reg);
    reg("spectral_analysis", "pburg",       &signal::detail::pburg_reg);

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
    reg("spectral_analysis", "bandpower",        &signal::detail::bandpower_reg);
    reg("spectral_analysis", "meanfreq",         &signal::detail::meanfreq_reg);
    reg("spectral_analysis", "medfreq",          &signal::detail::medfreq_reg);
    reg("spectral_analysis", "enbw",             &signal::detail::enbw_reg);
    reg("spectral_analysis", "obw",              &signal::detail::obw_reg);
    reg("spectral_analysis", "powerbw",          &signal::detail::powerbw_reg);
    reg("spectral_analysis", "spectralcrest",    &signal::detail::spectralcrest_reg);
    reg("spectral_analysis", "spectralflatness", &signal::detail::spectralflatness_reg);
    reg("spectral_analysis", "spectralentropy",  &signal::detail::spectralentropy_reg);
    reg("spectral_analysis", "spectralkurtosis", &signal::detail::spectralkurtosis_reg);
    reg("spectral_analysis", "spectralskewness", &signal::detail::spectralskewness_reg);
    // NOTE: MATLAB camelCase variants spectralCrest/Entropy/Flatness/Kurtosis/
    // Skewness now live in libs/audio (per-frame STFT semantics matching
    // MATLAB R2025b Signal Toolbox; see libs/audio/src/spectral/shape_descriptors.cpp).
    // The lowercase forms above remain as legacy single-segment scalar versions.
    reg("spectral_analysis", "snr",              &signal::detail::snr_reg);
    reg("spectral_analysis", "sinad",            &signal::detail::sinad_reg);
    reg("spectral_analysis", "thd",              &signal::detail::thd_reg);
    reg("spectral_analysis", "sfdr",             &signal::detail::sfdr_reg);
    reg("spectral_analysis", "instfreq",         &signal::detail::instfreq_reg);
    reg("spectral_analysis", "instbw",           &signal::detail::instbw_reg);
    reg("filter_design", "buttap",   &signal::detail::buttap_reg);
    reg("filter_design", "cheb1ap",  &signal::detail::cheb1ap_reg);
    reg("filter_design", "cheb2ap",  &signal::detail::cheb2ap_reg);
    reg("filter_design", "ellipap",  &signal::detail::ellipap_reg);
    reg("filter_design", "besselap", &signal::detail::besselap_reg);
    reg("filter_design", "lp2lp",    &signal::detail::lp2lp_reg);
    reg("filter_design", "lp2hp",    &signal::detail::lp2hp_reg);
    reg("filter_design", "lp2bp",    &signal::detail::lp2bp_reg);
    reg("filter_design", "lp2bs",    &signal::detail::lp2bs_reg);
    reg("filter_design", "bilinear", &signal::detail::bilinear_reg);
    reg("filter_design", "impinvar", &signal::detail::impinvar_reg);
    reg("filter_design", "freqs",    &signal::detail::freqs_reg);
    reg("filter_design", "cheby1",   &signal::detail::cheby1_reg);
    reg("filter_design", "cheby2",   &signal::detail::cheby2_reg);
    reg("filter_design", "ellip",    &signal::detail::ellip_reg);
    reg("filter_design", "besself",  &signal::detail::besself_reg);
    reg("filter_design", "buttord",  &signal::detail::buttord_reg);
    reg("filter_design", "cheb1ord", &signal::detail::cheb1ord_reg);
    reg("filter_design", "cheb2ord", &signal::detail::cheb2ord_reg);
    reg("filter_design", "kaiserord", &signal::detail::kaiserord_reg);
    reg("filter_design", "ellipord",  &signal::detail::ellipord_reg);
    reg("filter_design", "firpmord",  &signal::detail::firpmord_reg);
    reg("parametric", "levinson",  &signal::detail::levinson_reg);
    reg("parametric", "rlevinson", &signal::detail::rlevinson_reg);
    reg("parametric", "aryule",    &signal::detail::aryule_reg);
    reg("parametric", "arburg",    &signal::detail::arburg_reg);
    reg("parametric", "lpc",       &signal::detail::lpc_reg);
    reg("parametric", "ac2poly",   &signal::detail::ac2poly_reg);
    reg("parametric", "poly2ac",   &signal::detail::poly2ac_reg);
    reg("parametric", "ac2rc",     &signal::detail::ac2rc_reg);
    reg("parametric", "schurrc",   &signal::detail::schurrc_reg);
    reg("parametric", "rc2ac",     &signal::detail::rc2ac_reg);
    reg("parametric", "poly2rc",   &signal::detail::poly2rc_reg);
    reg("parametric", "rc2poly",   &signal::detail::rc2poly_reg);
    reg("parametric", "is2rc",     &signal::detail::is2rc_reg);
    reg("parametric", "rc2is",     &signal::detail::rc2is_reg);
    reg("parametric", "lar2rc",    &signal::detail::lar2rc_reg);
    reg("parametric", "rc2lar",    &signal::detail::rc2lar_reg);
    reg("parametric", "arcov",     &signal::detail::arcov_reg);
    reg("parametric", "armcov",    &signal::detail::armcov_reg);
    reg("parametric", "prony",     &signal::detail::prony_reg);
    reg("parametric", "corrmtx",   &signal::detail::corrmtx_reg);
    reg("parametric", "poly2lsf",  &signal::detail::poly2lsf_reg);
    reg("parametric", "lsf2poly",  &signal::detail::lsf2poly_reg);
    reg("parametric", "invfreqs",  &signal::detail::invfreqs_reg);
    reg("parametric", "invfreqz",  &signal::detail::invfreqz_reg);
    reg("vibration",    "envspectrum",  &signal::detail::envspectrum_reg);
    reg("vibration",    "tachorpm",     &signal::detail::tachorpm_reg);
    reg("vibration",    "rainflow",     &signal::detail::rainflow_reg);
    reg("vibration",    "tsa",          &signal::detail::tsa_reg);
    reg("measurements", "statelevels",  &signal::detail::statelevels_reg);
    reg("measurements", "midcross",     &signal::detail::midcross_reg);
    reg("measurements", "risetime",     &signal::detail::risetime_reg);
    reg("measurements", "falltime",     &signal::detail::falltime_reg);
    reg("measurements", "slewrate",     &signal::detail::slewrate_reg);
    reg("measurements", "overshoot",    &signal::detail::overshoot_reg);
    reg("measurements", "undershoot",   &signal::detail::undershoot_reg);
    reg("measurements", "settlingtime", &signal::detail::settlingtime_reg);
    reg("measurements", "pulsewidth",   &signal::detail::pulsewidth_reg);
    reg("measurements", "pulseperiod",  &signal::detail::pulseperiod_reg);
    reg("measurements", "pulsesep",     &signal::detail::pulsesep_reg);
    reg("measurements", "dutycycle",    &signal::detail::dutycycle_reg);

    // Signal ROI utilities
    reg("measurements", "binmask2sigroi", &signal::detail::binmask2sigroi_reg);
    reg("measurements", "sigroi2binmask", &signal::detail::sigroi2binmask_reg);
    reg("measurements", "extendsigroi",   &signal::detail::extendsigroi_reg);
    reg("measurements", "shortensigroi",  &signal::detail::shortensigroi_reg);
    reg("measurements", "mergesigroi",    &signal::detail::mergesigroi_reg);
    reg("measurements", "removesigroi",   &signal::detail::removesigroi_reg);
    reg("measurements", "extractsigroi",  &signal::detail::extractsigroi_reg);
    reg("measurements", "sigrangebinmask",&signal::detail::sigrangebinmask_reg);

    reg("measurements", "seqperiod",     &signal::detail::seqperiod_reg);
    reg("measurements", "zerocrossrate", &signal::detail::zerocrossrate_reg);
    reg("measurements", "cusum",         &signal::detail::cusum_reg);

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
    engine.registerFunction("", "fft2",      &signal::detail::fft2_reg);
    engine.registerFunction("", "ifft2",     &signal::detail::ifft2_reg);
    engine.registerFunction("", "fftshift",  &signal::detail::fftshift_reg);
    engine.registerFunction("", "ifftshift", &signal::detail::ifftshift_reg);
    engine.registerFunction("", "conv",      &signal::detail::conv_reg);
    engine.registerFunction("", "conv2",     &signal::detail::conv2_reg);
    engine.registerFunction("", "filter2",   &signal::detail::filter2_reg);
    engine.registerFunction("", "convn",     &signal::detail::convn_reg);
    engine.registerFunction("", "xcorr",     &signal::detail::xcorr_reg);
    engine.registerFunction("", "xcov",      &signal::detail::xcov_reg);
    engine.registerFunction("", "interpft",  &signal::detail::interpft_reg);
}

} // namespace numkit
