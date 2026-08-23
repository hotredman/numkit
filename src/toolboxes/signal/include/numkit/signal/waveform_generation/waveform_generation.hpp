/// @file waveform_generation.hpp
/// @ingroup group_signal
// toolboxes/signal/include/numkit/signal/waveform_generation/waveform_generation.hpp
//
// Pulse and chirp waveforms.

#pragma once

#include <memory_resource>
#include <numkit/value/fn_handle.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <string>

namespace numkit::signal {

/// @addtogroup group_signal
/// @{


/// Rectangular pulse of unit amplitude on the open interval `(-w/2, w/2)`.
///
/// \f$ y(t) = 1 \f$ inside the support, `0` outside; the boundary
/// `|t| == w/2` returns `0` (open-interval convention).
///
/// @param t   Time samples.
/// @param w   Pulse width. Default 1.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape DOUBLE array.
Value rectpuls(const Value &                t,
               double                       w  = 1.0,
               std::pmr::memory_resource *  mr = nullptr);

/// Triangular pulse of unit amplitude on `|t| ≤ w/2`.
///
/// \f$ y(t) = \max(1 - 2|t|/w,\ 0) \f$.
///
/// @param t   Time samples.
/// @param w   Pulse base width. Default 1.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape DOUBLE array.
Value tripuls(const Value &                t,
              double                       w  = 1.0,
              std::pmr::memory_resource *  mr = nullptr);

/// Gaussian-modulated sinusoid.
///
/// \f$ y(t) = \exp(-\alpha t^2) \cos(2\pi f_c t) \f$
/// where α is set so the envelope reaches `bw` fractional bandwidth at
/// the -6 dB level (`bwr = -6 dB` hard-coded).
///
/// @param t   Time samples.
/// @param fc  Centre frequency in Hz.
/// @param bw  Fractional bandwidth. Default 0.5.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape DOUBLE array.
Value gauspuls(const Value &                t,
               double                       fc,
               double                       bw = 0.5,
               std::pmr::memory_resource *  mr = nullptr);

/// Pulse train: `Σ_i fn(t - d_i)` for a named pulse type.
///
/// @param t       Output time grid.
/// @param d       Delay vector (one entry per pulse).
/// @param fnName  Pulse-generator name: `"rectpuls"`, `"tripuls"`,
///                or `"gauspuls"`.
/// @param fcOrW   For rectpuls / tripuls: pulse width `w`. For gauspuls:
///                centre frequency `fc`. Default 1.
/// @param bw      Gaussian fractional bandwidth (gauspuls only). Default 0.5.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Same-shape DOUBLE array.
///
/// @see pulstranHandle, rectpuls, tripuls, gauspuls
Value pulstran(const Value &                t,
               const Value &                d,
               const std::string &          fnName,
               double                       fcOrW = 1.0,
               double                       bw    = 0.5,
               std::pmr::memory_resource *  mr    = nullptr);

/// Pulse train with a callback-based pulse generator.
///
/// The callback is invoked once per delay with a 1-element `args`
/// holding the shifted time vector `t - d_i` (as a `1 × n` DOUBLE
/// row Value) and writes the corresponding pulse vector into
/// `outs[0]` (length `n`).
///
/// @param t   Output time grid (length n).
/// @param d   Delay vector (length k).
/// @param fn  Callback (1 vector in, 1 vector out).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `n × 1` column-vector DOUBLE Value containing
///            `Σ_i fn(t - d_i)`.
/// @see pulstran
Value pulstranHandle(Span<const double>           t,
                     Span<const double>           d,
                     FnHandle                     fn,
                     std::pmr::memory_resource *  mr = nullptr);

/// Frequency-modulated cosine (chirp).
///
/// Three sweep laws:
///   * `"linear"`      — \f$ y = \cos(2\pi (f_0 t + ((f_1-f_0)/(2 t_1)) t^2)) \f$
///   * `"quadratic"`   — \f$ y = \cos(2\pi (f_0 t + ((f_1-f_0)/(3 t_1^2)) t^3)) \f$
///   * `"logarithmic"` — \f$ y = \cos(2\pi f_0 ((\beta^t - 1)/\ln\beta)) \f$,
///                       \f$ \beta = (f_1/f_0)^{1/t_1} \f$. Requires
///                       `f0 > 0`, `f1 > 0`, `f1 != f0`.
///
/// @param t       Time samples.
/// @param f0      Initial frequency in Hz.
/// @param t1      Reference time.
/// @param f1      Frequency at time `t1` (Hz).
/// @param method  `"linear"` (default), `"quadratic"`, or `"logarithmic"`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Same-shape DOUBLE array.
Value chirp(const Value &                t,
            double                       f0,
            double                       t1,
            double                       f1,
            const std::string &          method = "linear",
            std::pmr::memory_resource *  mr     = nullptr);

/// Square wave with period 2π.
///
/// Output is `+1` during the high portion and `-1` during the low
/// portion of each cycle. The high-portion fraction is `duty / 100`.
///
/// @param t     Time samples.
/// @param duty  Duty cycle in percent. Default 50 (symmetric).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Same-shape DOUBLE array.
Value square(const Value &                t,
             double                       duty = 50.0,
             std::pmr::memory_resource *  mr   = nullptr);

/// Sawtooth wave on period 2π.
///
/// `width` (in `[0, 1]`) controls the rising-portion fraction:
///   * `width = 1` (default) → canonical sawtooth (linear ramp -1 → +1).
///   * `width = 0.5`         → symmetric triangle wave.
///   * `width = 0`           → reversed sawtooth.
///
/// @param t      Time samples.
/// @param width  Rising-portion fraction in `[0, 1]`. Default 1.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Same-shape DOUBLE array.
Value sawtooth(const Value &                t,
               double                       width = 1.0,
               std::pmr::memory_resource *  mr    = nullptr);

/// Normalised cardinal sine: `sinc(0) = 1`, otherwise `sin(πt)/(πt)`.
///
/// @param t   Real array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape DOUBLE array.
Value sinc(const Value &                t,
           std::pmr::memory_resource *  mr = nullptr);

/// Gaussian monopulse at centre frequency `fc`.
///
/// \f$ y(t) = \frac{2\pi f_c t}{\sqrt{K}} \exp(-2 (\pi f_c t)^2) \f$
/// where `K` is chosen so the pulse peak equals 1 at `t = 1/(2π fc)`.
///
/// @param t   Time samples.
/// @param fc  Centre frequency in Hz.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape DOUBLE array.
Value gmonopuls(const Value &                t,
                double                       fc,
                std::pmr::memory_resource *  mr = nullptr);

/// Dirichlet (periodic-sinc) function of order `n`.
///
/// \f$ y(x) = \frac{\sin(nx/2)}{n \sin(x/2)} \f$ for `x` not a multiple
/// of 2π; at `x = 2π k`: `y = (-1)^{k(n-1)}`.
///
/// @param x   Real array.
/// @param n   Order, positive integer.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape DOUBLE array.
Value diric(const Value &                x,
            int                          n,
            std::pmr::memory_resource *  mr = nullptr);

/// Analog modulation: AM / FM / PM family.
///
/// Supported (case-insensitive) `method`:
///   * `"am"`        / `"amdsb-sc"` — double-sideband suppressed carrier.
///   * `"amdsb-tc"`                  — DSB with transmitted carrier; opt is
///                                     the DC offset (default `min(x)`).
///   * `"fm"`                        — frequency modulation;
///                                     opt is deviation factor `kf`
///                                     (default `Fc/Fs · 2π / max(|x|)`).
///   * `"pm"`                        — phase modulation;
///                                     opt is deviation `kp` (default
///                                     `π / max(|x|)`).
///
/// @param x       Message signal.
/// @param Fc      Carrier frequency in Hz.
/// @param Fs      Sample rate in Hz.
/// @param method  Modulation type (case-insensitive).
/// @param opt     Method-specific scalar; `Value::Empty` → defaults above.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Modulated signal.
///
/// @note KNOWN GAPs: `"amssb"`, `"pwm"`, `"ptm"`, `"ppm"`, `"qam"` deferred.
/// @see demod, vco
Value modulate(const Value &                x,
               double                       Fc,
               double                       Fs,
               const std::string &          method,
               const Value &                opt = Value::Empty,
               std::pmr::memory_resource *  mr  = nullptr);

/// Analog demodulation (AM family).
///
/// Supported `method`:
///   * `"am"` / `"amdsb-sc"` — multiply by `cos(2π Fc t)`, then lowpass.
///   * `"amdsb-tc"`           — same, but opt is the DC offset to subtract.
///
/// Pipeline: `y · cos(2π Fc t)` → 5th-order Butterworth lowpass at
/// cutoff `2·Fc/Fs` via `filtfilt`.
///
/// @param y       Received signal.
/// @param Fc      Carrier frequency in Hz.
/// @param Fs      Sample rate in Hz.
/// @param method  Demodulation type.
/// @param opt     Method-specific scalar (DC offset for amdsb-tc).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Demodulated message signal.
///
/// @note KNOWN GAPs: `"fm"` / `"pm"` (use Hilbert, blocked on toolboxes/signal
///       FFT sign-convention bug), `"amssb"`, `"pwm"`, `"ptm"`, `"ppm"`,
///       `"qam"`.
/// @see modulate
Value demod(const Value &                y,
            double                       Fc,
            double                       Fs,
            const std::string &          method,
            const Value &                opt = Value::Empty,
            std::pmr::memory_resource *  mr  = nullptr);

/// Voltage-controlled (frequency-modulated) oscillator.
///
/// `x ∈ [-1, 1]` modulates the instantaneous frequency. Centre-form:
/// `-1 → 0 Hz`, `0 → Fc Hz`, `+1 → 2·Fc Hz`. Frequency modulation
/// via rectangular `cumsum` integral approximation.
///
/// @param x   Control signal in `[-1, 1]`.
/// @param fc  Centre frequency in Hz (`x = 0` maps to this).
/// @param fs  Sample rate in Hz.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `y = cos(...)` of the same shape as `x`.
/// @see modulate
Value vco(const Value &x, double fc, double fs,
          std::pmr::memory_resource *mr = nullptr);

/// @brief Voltage-controlled oscillator, explicit `[Fmin, Fmax]`
/// range form (`y = vco(x, [fmin fmax], fs)`).
///
/// `x ∈ [-1, 1]`: `-1 → fmin`, `+1 → fmax`.
///
/// @param x     Control signal in `[-1, 1]`.
/// @param fmin  Frequency at `x == -1`, Hz.
/// @param fmax  Frequency at `x == +1`, Hz.
/// @param fs    Sample rate in Hz.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `y = cos(...)` of the same shape as `x`.
/// @see modulate
Value vco(const Value &x, double fmin, double fmax, double fs,
          std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::signal
