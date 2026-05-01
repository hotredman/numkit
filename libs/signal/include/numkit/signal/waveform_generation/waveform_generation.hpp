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
Value rectpuls(std::pmr::memory_resource *mr, const Value &t, double w = 1.0);

/// tripuls(t[, w]) — unit-amplitude triangular pulse on |t| ≤ w/2.
/// y = max(1 - 2·|t|/w, 0). Default w = 1.
Value tripuls(std::pmr::memory_resource *mr, const Value &t, double w = 1.0);

/// gauspuls(t, fc[, bw]) — Gaussian-modulated sinusoid.
/// y = exp(-α·t²) · cos(2π·fc·t) where α is set by the fractional
/// bandwidth bw at the -6dB envelope level (MATLAB convention,
/// bwr = -6 dB hard-coded). Default bw = 0.5.
Value gauspuls(std::pmr::memory_resource *mr, const Value &t, double fc, double bw = 0.5);

/// pulstran(t, d, fnName[, args...]) — pulse train: ∑_i fn(t - d_i, args).
/// fnName is one of "rectpuls" / "tripuls" / "gauspuls". Custom function
/// handles are invoked via Engine::callFunctionHandle when an Engine is
/// available; without one they throw m:pulstran:fnUnsupported.
Value pulstran(std::pmr::memory_resource *mr, const Value &t, const Value &d,
                const std::string &fnName, double fcOrW = 1.0,
                double bw = 0.5);

/// pulstran with a function-handle pulse generator. The handle is
/// invoked once per delay as `fn(t - d_i)` (extra trailing args from
/// the adapter aren't forwarded — keep the handle a 1-input function).
/// Engine pointer must be valid.
Value pulstranHandle(std::pmr::memory_resource *mr, const Value &t, const Value &d,
                      const Value &fnHandle, Engine *engine);

/// chirp(t, f0, t1, f1[, method]) — frequency-modulated cosine.
/// Output has the same shape as `t` and is always DOUBLE.
///   method = "linear":      y = cos(2π·(f0·t + ((f1-f0)/(2·t1))·t²))
///   method = "quadratic":   y = cos(2π·(f0·t + ((f1-f0)/(3·t1²))·t³))
///   method = "logarithmic": y = cos(2π·f0·((β^t - 1)/log(β))),
///                            β = (f1/f0)^(1/t1). Requires f0 > 0,
///                            f1 > 0, f1 != f0.
Value chirp(std::pmr::memory_resource *mr, const Value &t,
             double f0, double t1, double f1,
             const std::string &method = "linear");

/// square(t[, duty]) — square wave with period 2π and duty cycle in
/// percent (default 50). Output is +1 during the high portion and -1
/// during the low portion of each cycle.
Value square(std::pmr::memory_resource *mr, const Value &t, double duty = 50.0);

/// sawtooth(t[, width]) — periodic sawtooth on period 2π. `width` in
/// [0, 1] controls the rising-portion fraction; width=1 (default) gives
/// the canonical sawtooth (linear ramp from -1 to 1). width=0.5 yields
/// a triangle wave.
Value sawtooth(std::pmr::memory_resource *mr, const Value &t, double width = 1.0);

/// sinc(t) — normalised cardinal sine: sinc(0) = 1, otherwise sin(πt)/(πt).
Value sinc(std::pmr::memory_resource *mr, const Value &t);

/// gmonopuls(t, fc) — Gaussian monopulse at centre frequency fc.
/// y = (2π·fc·t / sqrt(K)) · exp(-2·(π·fc·t)²) where K is set so the
/// pulse peak equals 1 at t = 1/(2π·fc).
Value gmonopuls(std::pmr::memory_resource *mr, const Value &t, double fc);

/// diric(x, n) — Dirichlet (periodic-sinc) function of order n.
///   y = sin(n·x/2) / (n·sin(x/2))           if x is not a multiple of 2π
///   y = (-1)^(k·(n-1))                       if x = 2π·k
/// `n` must be a positive integer.
Value diric(std::pmr::memory_resource *mr, const Value &x, int n);

} // namespace numkit::signal
