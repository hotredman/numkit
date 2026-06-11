// toolboxes/signal/include/numkit/signal/filter_implementation/conversions_extras.hpp
//
// Additional filter-form conversions: sos ↔ tf, sos ↔ zpk, tf ↔ ss,
// sos ↔ ss, zpk ↔ ss, plus tf2zpk (gain-explicit alias of tf2zp).
//
// State-space form is the 4-tuple (A, B, C, D):
//   A : N×N state-transition matrix
//   B : N×1 input vector
//   C : 1×N output vector
//   D : 1×1 direct feed-through scalar
// Single-input single-output controllable canonical form is used.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit::signal {

/// Second-order sections to transfer function.
///
/// Convolves every section row `[b0 b1 b2 1 a1 a2]` across the cascade
/// to recover the global numerator and denominator polynomials.
///
/// @param sos  L × 6 second-order-sections matrix.
/// @param g    Optional global gain (multiplies `b` at the end). Default 1.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Tuple `(b, a)` — global numerator and denominator.
///
/// @see zp2sos, tf2sos
std::tuple<Value, Value>
sos2tf(const Value &                sos,
       double                       g  = 1.0,
       std::pmr::memory_resource *  mr = nullptr);

/// Second-order sections to zero / pole / gain.
///
/// Roots each section and assembles the zero / pole vectors. The
/// per-section gains are accumulated into the returned scalar gain.
///
/// @param sos  L × 6 SOS matrix.
/// @param g    Optional global gain prefactor. Default 1.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Tuple `(zeros, poles, gain)`.
///
/// @see sos2tf, zp2sos
std::tuple<Value, Value, double>
sos2zp(const Value &                sos,
       double                       g  = 1.0,
       std::pmr::memory_resource *  mr = nullptr);

/// Transfer function to zero / pole / gain.
///
/// Alias of `tf2zp`. Roots `b` for zeros, `a` for
/// poles, and computes `gain = b[0] / a[0]`.
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(zeros, poles, gain)`.
std::tuple<Value, Value, double>
tf2zpk(const Value &                b,
       const Value &                a,
       std::pmr::memory_resource *  mr = nullptr);

/// Cascaded transfer function to zero / pole / gain.
///
/// `NUM` (K × Q+1) and `DEN` (K × R+1) define K cascaded biquad-like
/// sections. The optional `SV` (scalar or K+1 vector) provides per-section
/// scale values; default 1.
///
/// @param NUM   K × (Q+1) numerator-of-section matrix.
/// @param DEN   K × (R+1) denominator-of-section matrix.
/// @param SV    Section scale values. `Value::Empty` → all 1.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Tuple `(Z, P, K_gain)`.
///
/// @see scaleFilterSections
std::tuple<Value, Value, double>
ctf2zp(const Value &                NUM,
       const Value &                DEN,
       const Value &                SV = Value::Empty,
       std::pmr::memory_resource *  mr = nullptr);

/// Scale numerator coefficients of a cascaded transfer function.
///
/// Distributes the scale values `SV` (scalar or K+1 vector) across all
/// sections of the cascade NUM matrix. Magnitude is distributed as
/// `|sv|^(1/K)` per section; sign is concentrated on the last section.
///
/// @param CTFNum  K × (Q+1) numerator matrix.
/// @param SV      Scale values.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Scaled K × (Q+1) numerator matrix.
Value scaleFilterSections(const Value &                CTFNum,
                          const Value &                SV,
                          std::pmr::memory_resource *  mr = nullptr);

/// Transfer function to state space (controllable canonical form).
///
/// Returns the 4-tuple `(A, B, C, D)` defining a SISO state-space realisation
/// equivalent to `H(z) = B(z) / A(z)`. The controllable canonical form is
/// always used.
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(A, B, C, D)`.
///
/// @see ss2tf, tf2zpk
std::tuple<Value, Value, Value, Value>
tf2ss(const Value &                b,
      const Value &                a,
      std::pmr::memory_resource *  mr = nullptr);

/// State space to transfer function (SISO only).
///
/// @param A   N × N state-transition matrix.
/// @param B   N × 1 input vector.
/// @param C   1 × N output vector.
/// @param D   Direct feed-through scalar (default 0).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(b, a)`.
/// @see tf2ss
std::tuple<Value, Value>
ss2tf(const Value &                A,
      const Value &                B,
      const Value &                C,
      double                       D  = 0.0,
      std::pmr::memory_resource *  mr = nullptr);

/// @brief State space → zero / pole / gain
/// (`[z, p, k] = ss2zp(A, B, C, D)`).
///
/// Composes @ref ss2tf with `tf2zpk` internally. SISO only.
///
/// @param A   N × N state-transition matrix.
/// @param B   N × 1 input vector.
/// @param C   1 × N output vector.
/// @param D   Direct feed-through scalar (default 0).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(z, p, k)` — zeros, poles, gain.
/// @see ss2tf, zp2ss
std::tuple<Value, Value, double>
ss2zp(const Value &A, const Value &B, const Value &C, double D = 0.0,
      std::pmr::memory_resource *mr = nullptr);

/// Zero / pole / gain to state space (via `zp2tf` then `tf2ss`).
///
/// @param z   Zeros (complex column vector).
/// @param p   Poles (complex column vector).
/// @param k   Scalar gain.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(A, B, C, D)`.
std::tuple<Value, Value, Value, Value>
zp2ss(const Value &                z,
      const Value &                p,
      double                       k,
      std::pmr::memory_resource *  mr = nullptr);

/// Second-order sections to state space (via `sos2tf` then `tf2ss`).
///
/// @param sos  L × 6 SOS matrix.
/// @param g    Optional global gain. Default 1.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Tuple `(A, B, C, D)`.
std::tuple<Value, Value, Value, Value>
sos2ss(const Value &                sos,
       double                       g  = 1.0,
       std::pmr::memory_resource *  mr = nullptr);

/// @brief State space → second-order sections
/// (`sos = ss2sos(A, B, C, D)`).
///
/// Composes @ref ss2tf with @ref tf2sos. SISO only.
///
/// @param A   N × N state-transition matrix.
/// @param B   N × 1 input vector.
/// @param C   1 × N output vector.
/// @param D   Direct feed-through scalar (default 0).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `L × 6` SOS matrix.
/// @see ss2tf, sos2ss
Value ss2sos(const Value &A, const Value &B, const Value &C, double D = 0.0,
             std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
