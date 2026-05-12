// libs/signal/include/numkit/signal/waveform_generation/waveform_generation.hpp
//
// Pulse and chirp waveforms.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit { class Engine; }

namespace numkit::signal {

using ::numkit::Engine;

/// rectpuls(t[, w]) — unit-amplitude rectangular pulse on (-w/2, w/2).
/// Default w = 1. y = 1 inside the support, 0 outside; the boundary
/// |t| == w/2 returns 0 (matches MATLAB's open interval).
Value rectpuls(const Value &t, double w = 1.0, std::pmr::memory_resource *mr = nullptr);

/// tripuls(t[, w]) — unit-amplitude triangular pulse on |t| ≤ w/2.
/// y = max(1 - 2·|t|/w, 0). Default w = 1.
Value tripuls(const Value &t, double w = 1.0, std::pmr::memory_resource *mr = nullptr);

/// gauspuls(t, fc[, bw]) — Gaussian-modulated sinusoid.
/// y = exp(-α·t²) · cos(2π·fc·t) where α is set by the fractional
/// bandwidth bw at the -6dB envelope level (MATLAB convention,
/// bwr = -6 dB hard-coded). Default bw = 0.5.
Value gauspuls(const Value &t, double fc, double bw = 0.5, std::pmr::memory_resource *mr = nullptr);

/// pulstran(t, d, fnName[, args...]) — pulse train: ∑_i fn(t - d_i, args).
/// fnName is one of "rectpuls" / "tripuls" / "gauspuls". Custom function
/// handles are invoked via Engine::callFunctionHandle when an Engine is
/// available; without one they throw m:pulstran:fnUnsupported.
Value pulstran(const Value &t, const Value &d, const std::string &fnName, double fcOrW = 1.0, double bw = 0.5, std::pmr::memory_resource *mr = nullptr);

/// pulstran with a function-handle pulse generator. The handle is
/// invoked once per delay as `fn(t - d_i)` (extra trailing args from
/// the adapter aren't forwarded — keep the handle a 1-input function).
/// Engine pointer must be valid.
Value pulstranHandle(const Value &t, const Value &d, const Value &fnHandle, Engine *engine, std::pmr::memory_resource *mr = nullptr);

/// chirp(t, f0, t1, f1[, method]) — frequency-modulated cosine.
/// Output has the same shape as `t` and is always DOUBLE.
///   method = "linear":      y = cos(2π·(f0·t + ((f1-f0)/(2·t1))·t²))
///   method = "quadratic":   y = cos(2π·(f0·t + ((f1-f0)/(3·t1²))·t³))
///   method = "logarithmic": y = cos(2π·f0·((β^t - 1)/log(β))),
///                            β = (f1/f0)^(1/t1). Requires f0 > 0,
///                            f1 > 0, f1 != f0.
Value chirp(const Value &t, double f0, double t1, double f1, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

/// square(t[, duty]) — square wave with period 2π and duty cycle in
/// percent (default 50). Output is +1 during the high portion and -1
/// during the low portion of each cycle.
Value square(const Value &t, double duty = 50.0, std::pmr::memory_resource *mr = nullptr);

/// sawtooth(t[, width]) — periodic sawtooth on period 2π. `width` in
/// [0, 1] controls the rising-portion fraction; width=1 (default) gives
/// the canonical sawtooth (linear ramp from -1 to 1). width=0.5 yields
/// a triangle wave.
Value sawtooth(const Value &t, double width = 1.0, std::pmr::memory_resource *mr = nullptr);

/// sinc(t) — normalised cardinal sine: sinc(0) = 1, otherwise sin(πt)/(πt).
Value sinc(const Value &t, std::pmr::memory_resource *mr = nullptr);

/// gmonopuls(t, fc) — Gaussian monopulse at centre frequency fc.
/// y = (2π·fc·t / sqrt(K)) · exp(-2·(π·fc·t)²) where K is set so the
/// pulse peak equals 1 at t = 1/(2π·fc).
Value gmonopuls(const Value &t, double fc, std::pmr::memory_resource *mr = nullptr);

/// diric(x, n) — Dirichlet (periodic-sinc) function of order n.
///   y = sin(n·x/2) / (n·sin(x/2))           if x is not a multiple of 2π
///   y = (-1)^(k·(n-1))                       if x = 2π·k
/// `n` must be a positive integer.
Value diric(const Value &x, int n, std::pmr::memory_resource *mr = nullptr);

/// modulate(x, Fc, Fs, method[, opt]) — analog modulation methods.
/// Supported `method` (case-insensitive): "am", "amdsb-sc" (= "am"),
/// "amdsb-tc", "fm", "pm". opt: amdsb-tc → DC offset (default min(x));
/// fm → freq deviation factor kf (default Fc/Fs·2π/max(|x|));
/// pm → phase deviation factor kp (default π/max(|x|)).
/// KNOWN GAPs: amssb, pwm, ptm/ppm, qam deferred — all use Hilbert/FFT
/// or specialised pulse waveforms. Common modes (am/fm/pm/amdsb-tc)
/// shipped, all bit-equal MATLAB R2025b.
Value modulate(const Value &x, double Fc, double Fs, const std::string &method, const Value &opt = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// demod(y, Fc, Fs, method[, opt]) — analog demodulation (AM family).
/// Supports "am", "amdsb-sc" (= "am"), "amdsb-tc". For amdsb-tc, opt
/// is the DC offset to subtract (default 0). KNOWN GAPs: fm/pm modes
/// (use hilbert, blocked on libs/signal::fft sign-convention bug),
/// amssb / pwm / ptm/ppm / qam deferred. Pipeline: y * cos(2π Fc t) →
/// 5th-order Butterworth lowpass (cutoff 2*Fc/Fs) via filtfilt.
Value demod(const Value &y, double Fc, double Fs, const std::string &method, const Value &opt = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// vco(x, range, fs) — voltage-controlled (frequency-modulated) oscillator.
/// x ∈ [-1, 1] modulates the instantaneous frequency. Returns y = cos(...)
/// of the same shape as x. range may be:
///   - scalar Fc   : -1 → 0 Hz, 0 → Fc Hz, +1 → 2·Fc Hz
///   - [Fmin Fmax] : -1 → Fmin Hz, +1 → Fmax Hz
/// Frequency modulation via rectangular cumsum integral approximation
/// (matches MATLAB modulate(...,'fm')).
Value vco(const Value &x, const Value &range, double fs, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
