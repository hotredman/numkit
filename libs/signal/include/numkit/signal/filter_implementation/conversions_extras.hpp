// libs/signal/include/numkit/signal/filter_implementation/conversions_extras.hpp
//
// Additional filter-form conversions to round out D3:
//   sos ↔ tf, sos ↔ zpk, tf ↔ ss, sos ↔ ss, zpk ↔ ss, plus tf2zpk
//   (the gain-explicit alias of tf2zp).
//
// State-space form is the 4-tuple (A, B, C, D):
//   A : N×N state-transition matrix
//   B : N×1 input vector
//   C : 1×N output vector
//   D : 1×1 direct feed-through scalar
// Single-input single-output controllable canonical form is used.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

/// sos2tf(sos[, g]) — convolve every section row [b0 b1 b2 a0 a1 a2]
/// across the cascade to recover (b, a). Optional `g` multiplies b.
std::tuple<Value, Value>
sos2tf(const Value &sos, double g = 1.0, std::pmr::memory_resource *mr = nullptr);

/// sos2zp(sos[, g]) — zeros / poles / gain reconstruction by rooting
/// each section. Returns (zeros, poles, gain).
std::tuple<Value, Value, double>
sos2zp(const Value &sos, double g = 1.0, std::pmr::memory_resource *mr = nullptr);

/// tf2zpk(b, a) — alias of tf2zp returning (zeros, poles, gain). MATLAB
/// keeps both names; tf2zpk is the more common modern spelling.
std::tuple<Value, Value, double>
tf2zpk(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// ctf2zp(NUM, DEN[, SV]) — cascaded transfer function → zero/pole/gain.
/// NUM (K × Q+1) and DEN (K × R+1) define K cascaded biquad-like sections.
/// SV (scalar or K+1 vector) optional scale values; defaults to 1.
/// Returns (Z, P, K_gain).
std::tuple<Value, Value, double>
ctf2zp(const Value &NUM, const Value &DEN, const Value &SV = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// scaleFilterSections(CTFNum, SV) — scale numerator coefficients of a
/// cascaded-transfer-function NUM matrix by per-section scale values.
/// SV is scalar or K+1 vector. Distributes |sv|^(1/K) across all sections,
/// keeps sign info on the last section.
Value scaleFilterSections(const Value &CTFNum, const Value &SV, std::pmr::memory_resource *mr = nullptr);

/// tf2ss(b, a) — transfer function to state-space (controllable
/// canonical form). Returns (A, B, C, D).
std::tuple<Value, Value, Value, Value>
tf2ss(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// ss2tf(A, B, C, D) — state space to transfer function. SISO only.
/// Returns (b, a).
std::tuple<Value, Value>
ss2tf(const Value &A, const Value &B, const Value &C, const Value &D, std::pmr::memory_resource *mr = nullptr);

/// ss2zp(A, B, C, D) — state space → (zeros, poles, gain) via tf2zp on
/// the ss2tf result.
std::tuple<Value, Value, double>
ss2zp(const Value &A, const Value &B, const Value &C, const Value &D, std::pmr::memory_resource *mr = nullptr);

/// zp2ss(z, p, k) — zeros/poles/gain → (A, B, C, D) via zp2tf+tf2ss.
std::tuple<Value, Value, Value, Value>
zp2ss(const Value &z, const Value &p, double k, std::pmr::memory_resource *mr = nullptr);

/// sos2ss(sos[, g]) — sos → (A, B, C, D) via sos2tf+tf2ss.
std::tuple<Value, Value, Value, Value>
sos2ss(const Value &sos, double g = 1.0, std::pmr::memory_resource *mr = nullptr);

/// ss2sos(A, B, C, D) — state space → SOS via ss2tf+tf2sos.
Value ss2sos(const Value &A, const Value &B, const Value &C, const Value &D, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
