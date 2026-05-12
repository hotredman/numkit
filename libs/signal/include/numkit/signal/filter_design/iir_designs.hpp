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

#include <string>
#include <string_view>
#include <tuple>

namespace numkit::signal {

enum class FilterType { Lowpass, Highpass, Bandpass, Bandstop };

/// cheby1 — Chebyshev type I IIR design.
std::tuple<Value, Value>
cheby1(int N, double Rp, const Value &Wn, FilterType ftype = FilterType::Lowpass, bool analog = false, std::pmr::memory_resource *mr = nullptr);

/// cheby2 — Chebyshev type II IIR design.
std::tuple<Value, Value>
cheby2(int N, double Rs, const Value &Wn, FilterType ftype = FilterType::Lowpass, bool analog = false, std::pmr::memory_resource *mr = nullptr);

/// besself — Bessel/Thompson IIR design (analog only by default in MATLAB,
/// but we support the digital path via bilinear if `analog == false`).
std::tuple<Value, Value>
besself(int N, const Value &Wn, FilterType ftype = FilterType::Lowpass, bool analog = false, std::pmr::memory_resource *mr = nullptr);

/// ellip — Cauer (elliptic) IIR design. Passband ripple Rp dB,
/// stopband attenuation Rs dB. Same call shape as cheby1/cheby2.
std::tuple<Value, Value>
ellip(int N, double Rp, double Rs, const Value &Wn, FilterType ftype = FilterType::Lowpass, bool analog = false, std::pmr::memory_resource *mr = nullptr);

// ── Order estimators (digital, normalised Wn ∈ (0, 1)) ─────────────

/// buttord(Wp, Ws, Rp, Rs[, 's']) — minimum order Butterworth filter
/// meeting passband ripple ≤ Rp and stopband attenuation ≥ Rs.
/// Returns (N, Wn) where Wn is the natural / cutoff frequency.
/// Wp, Ws are scalars (low/highpass) or 2-vectors (band{pass,stop}).
std::tuple<int, Value>
buttord(const Value &Wp, const Value &Ws, double Rp, double Rs, bool analog = false, std::pmr::memory_resource *mr = nullptr);

/// cheb1ord(Wp, Ws, Rp, Rs[, 's']) — minimum order Chebyshev type I.
/// Returns (N, Wn) where Wn is the passband edge.
std::tuple<int, Value>
cheb1ord(const Value &Wp, const Value &Ws, double Rp, double Rs, bool analog = false, std::pmr::memory_resource *mr = nullptr);

/// cheb2ord(Wp, Ws, Rp, Rs[, 's']) — minimum order Chebyshev type II.
/// Returns (N, Wn) where Wn is the stopband edge.
std::tuple<int, Value>
cheb2ord(const Value &Wp, const Value &Ws, double Rp, double Rs, bool analog = false, std::pmr::memory_resource *mr = nullptr);

/// ellipord(Wp, Ws, Rp, Rs[, 's']) — minimum order Elliptic (Cauer) filter
/// meeting passband ripple ≤ Rp dB and stopband attenuation ≥ Rs dB.
/// Returns (N, Wn). Bandstop (Wp 2-vec, Wp(1) > Ws(1)) is deferred —
/// KNOWN GAP. Lowpass / highpass / bandpass cases supported.
std::tuple<int, Value>
ellipord(const Value &Wp, const Value &Ws, double Rp, double Rs, bool analog = false, std::pmr::memory_resource *mr = nullptr);

/// firpmord(F, A, dev[, fs]) — Parks-McClellan FIR order estimator.
/// Returns (N, ff, aa, wts) where N is filter order suitable for firpm,
/// ff/aa/wts are the band-edge / amplitude / weight vectors for firpm.
/// F: vector of band edges (length 2·numel(A) - 2).
/// A: vector of binary amplitudes per band.
/// dev: max linear deviation per band.
/// fs: optional sample rate (default 2 → normalized).
std::tuple<int, Value, Value, Value>
firpmord(const Value &F, const Value &A, const Value &dev, double fs = 2.0, std::pmr::memory_resource *mr = nullptr);

/// kaiserord(F, A, dev[, fs]) — Kaiser-window FIR order estimator.
/// Returns (N, Wn, beta, ftype). N is the FIR filter order suitable for
/// fir1(N, Wn, ftype, kaiser(N+1, beta), 'noscale') to meet the
/// specifications given by F (transition-band edges in Hz), A (binary
/// amplitudes per band), dev (max linear deviations per band),
/// fs (sampling frequency, default 2 → normalized).
/// ftype is one of "low", "high", "stop", "bandpass", "DC-0", "DC-1".
std::tuple<int, Value, double, std::string>
kaiserord(const Value &F, const Value &A, const Value &dev, double fs = 2.0, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
