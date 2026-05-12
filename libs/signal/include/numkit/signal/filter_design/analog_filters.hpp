// libs/signal/include/numkit/signal/filter_design/analog_filters.hpp
//
// Analog filter prototypes + lowpass-to-X frequency transformations +
// bilinear z-transform + analog freq response. Together these form the
// machinery underneath cheby1 / cheby2 / ellip / besself; the
// top-level filters compose prototype → lp2X → bilinear.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

// ── Analog lowpass prototypes (cutoff Ω = 1 rad/s) ─────────────────
// Each returns (z, p, k) — zeros, poles, gain — of the prototype.
// Zeros come back as a possibly-empty COMPLEX column vector; poles
// are always COMPLEX column; gain is a real DOUBLE scalar.

/// buttap(N) — Butterworth analog prototype.
std::tuple<Value, Value, Value>
buttap(int N, std::pmr::memory_resource *mr = nullptr);

/// cheb1ap(N, Rp) — Chebyshev type I (passband ripple Rp dB) prototype.
std::tuple<Value, Value, Value>
cheb1ap(int N, double Rp, std::pmr::memory_resource *mr = nullptr);

/// cheb2ap(N, Rs) — Chebyshev type II (stopband attenuation Rs dB)
/// prototype. Has finite zeros on the imaginary axis.
std::tuple<Value, Value, Value>
cheb2ap(int N, double Rs, std::pmr::memory_resource *mr = nullptr);

/// besselap(N) — Bessel/Thompson prototype. Returns the standard
/// "no normalisation" form (poles of a Bessel polynomial). MATLAB's
/// besselap normalises so the magnitude equals 1/√2 at Ω = 1 rad/s
/// (group-delay, not magnitude); we match that convention.
std::tuple<Value, Value, Value>
besselap(int N, std::pmr::memory_resource *mr = nullptr);

/// ellipap(N, Rp, Rs) — Cauer (elliptic) analog prototype. Order N,
/// passband ripple Rp (dB), stopband attenuation Rs (dB). Has finite
/// zeros on the imaginary axis. Algorithm: Sophocleous/Orfanidis
/// formulas built on Jacobi elliptic functions and the degree
/// equation K(k')/K(k) = (1/N) * K(k1')/K(k1).
std::tuple<Value, Value, Value>
ellipap(int N, double Rp, double Rs, std::pmr::memory_resource *mr = nullptr);

// ── Lowpass → X transformations on (z, p, k) ───────────────────────

/// lp2lp(z, p, k, Wo) — scale a lowpass prototype to cutoff Wo.
std::tuple<Value, Value, Value>
lp2lp(const Value &z, const Value &p, double k, double Wo, std::pmr::memory_resource *mr = nullptr);

/// lp2hp(z, p, k, Wo) — lowpass → highpass at cutoff Wo.
std::tuple<Value, Value, Value>
lp2hp(const Value &z, const Value &p, double k, double Wo, std::pmr::memory_resource *mr = nullptr);

/// lp2bp(z, p, k, Wo, Bw) — lowpass → bandpass centred at Wo with
/// bandwidth Bw.
std::tuple<Value, Value, Value>
lp2bp(const Value &z, const Value &p, double k, double Wo, double Bw, std::pmr::memory_resource *mr = nullptr);

/// lp2bs(z, p, k, Wo, Bw) — lowpass → bandstop centred at Wo, bw Bw.
std::tuple<Value, Value, Value>
lp2bs(const Value &z, const Value &p, double k, double Wo, double Bw, std::pmr::memory_resource *mr = nullptr);

// ── Analog → digital ───────────────────────────────────────────────

/// bilinear(b, a, fs[, fp]) — bilinear transform of an analog filter
/// (b, a) to digital. With prewarp frequency `fp` non-zero, scales fs
/// to preserve the response at fp.
std::tuple<Value, Value>
bilinear(const Value &b, const Value &a, double fs, double fp = 0.0, std::pmr::memory_resource *mr = nullptr);

/// impinvar(b, a, fs[, tol]) — impulse-invariance design: digital
/// filter with the same impulse response samples as the analog filter
/// (b/a) sampled at rate fs. Partial-fraction-based.
std::tuple<Value, Value>
impinvar(const Value &b, const Value &a, double fs, double tol = 1e-3, std::pmr::memory_resource *mr = nullptr);

/// freqs(b, a, w) — magnitude/complex response of the analog filter
/// b(s)/a(s) at angular frequencies w (rad/s). Returns the complex H(jw).
Value freqs(const Value &b, const Value &a, const Value &w, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
