// libs/signal/include/numkit/signal/filter_design/iir_designs.hpp
//
// Top-level digital/analog IIR filter designs that compose the analog
// prototypes (cheb1ap/cheb2ap/besselap) with lp2X transforms,
// zp2tf, and bilinear:
//
//   cheby1(N, Rp,    Wn[, ftype][, 's'])
//   cheby2(N, Rs,    Wn[, ftype][, 's'])
//   besself(N,       Wn[, ftype][, 's'])
//
// Wn is normalised to Nyquist (0..1) for digital, rad/s for analog.
// Wn can be scalar (low/high) or 2-vector (bandpass/bandstop).
// ftype: "low" | "high" | "bandpass" | "stop". Default = lowpass when
// scalar Wn, bandpass when 2-vector.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string_view>
#include <tuple>

namespace numkit::signal {

enum class FilterType { Lowpass, Highpass, Bandpass, Bandstop };

/// cheby1 — Chebyshev type I IIR design.
std::tuple<Value, Value>
cheby1(std::pmr::memory_resource *mr, int N, double Rp,
       const Value &Wn, FilterType ftype = FilterType::Lowpass, bool analog = false);

/// cheby2 — Chebyshev type II IIR design.
std::tuple<Value, Value>
cheby2(std::pmr::memory_resource *mr, int N, double Rs,
       const Value &Wn, FilterType ftype = FilterType::Lowpass, bool analog = false);

/// besself — Bessel/Thompson IIR design (analog only by default in MATLAB,
/// but we support the digital path via bilinear if `analog == false`).
std::tuple<Value, Value>
besself(std::pmr::memory_resource *mr, int N,
        const Value &Wn, FilterType ftype = FilterType::Lowpass, bool analog = false);

/// ellip — Cauer (elliptic) IIR design. Passband ripple Rp dB,
/// stopband attenuation Rs dB. Same call shape as cheby1/cheby2.
std::tuple<Value, Value>
ellip(std::pmr::memory_resource *mr, int N, double Rp, double Rs,
      const Value &Wn, FilterType ftype = FilterType::Lowpass, bool analog = false);

// ── Order estimators (digital, normalised Wn ∈ (0, 1)) ─────────────

/// buttord(Wp, Ws, Rp, Rs[, 's']) — minimum order Butterworth filter
/// meeting passband ripple ≤ Rp and stopband attenuation ≥ Rs.
/// Returns (N, Wn) where Wn is the natural / cutoff frequency.
/// Wp, Ws are scalars (low/highpass) or 2-vectors (band{pass,stop}).
std::tuple<int, Value>
buttord(std::pmr::memory_resource *mr, const Value &Wp, const Value &Ws,
        double Rp, double Rs, bool analog = false);

/// cheb1ord(Wp, Ws, Rp, Rs[, 's']) — minimum order Chebyshev type I.
/// Returns (N, Wn) where Wn is the passband edge.
std::tuple<int, Value>
cheb1ord(std::pmr::memory_resource *mr, const Value &Wp, const Value &Ws,
         double Rp, double Rs, bool analog = false);

/// cheb2ord(Wp, Ws, Rp, Rs[, 's']) — minimum order Chebyshev type II.
/// Returns (N, Wn) where Wn is the stopband edge.
std::tuple<int, Value>
cheb2ord(std::pmr::memory_resource *mr, const Value &Wp, const Value &Ws,
         double Rp, double Rs, bool analog = false);

} // namespace numkit::signal
