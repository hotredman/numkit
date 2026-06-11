// toolboxes/signal/include/numkit/signal/filter_implementation/conversions.hpp
//
// Convert between filter representations: zp2sos / tf2sos. The cascade
// applicator sosfilt lives in digital_filtering/sosfilt.hpp.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit::signal {

/// Convert zeros / poles / gain to a second-order-sections (SOS) matrix.
///
/// Pairs complex-conjugate roots into biquadratic sections, then arranges
/// them into an `L × 6` matrix where each row is
/// `[b0 b1 b2 1 a1 a2]` (numerator + denominator coefficients of one
/// biquad).
///
/// 1-output form: the global gain is distributed across the sections so
/// the cascade `prod_k Hk(z)` reproduces the original transfer function.
///
/// @param zeros  Zeros of the filter (complex column vector).
/// @param poles  Poles of the filter (complex column vector).
/// @param gain   Overall scalar gain.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `L × 6` DOUBLE SOS matrix where
///               `L = ceil(max(numel(zeros), numel(poles)) / 2)`.
///
/// @code
/// auto [z, p, k] = ellipap(6, 1.0, 40.0);
/// Value sos = zp2sos(z, p, k);
/// Value y   = sosfilt(sos, x);
/// @endcode
///
/// @see zp2sosWithGain, sosfilt, tf2sos
Value zp2sos(const Value &                zeros,
             const Value &                poles,
             double                       gain,
             std::pmr::memory_resource *  mr = nullptr);

/// Convert zeros / poles / gain to (SOS, gain) — the two-output form.
///
/// Same biquad structure as `zp2sos` but the gain is returned separately;
/// the SOS matrix itself has no leading scale (each first-numerator
/// coefficient `b0` ≈ 1). This is the `[sos, g] = zp2sos(…)` form.
///
/// @param zeros  Zeros of the filter.
/// @param poles  Poles of the filter.
/// @param gain   Overall scalar gain.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Tuple `(sos, g)`. Apply via `g * sosfilt(sos, x)`.
///
/// @see zp2sos
/// @param surplusAtOrigin  When #zeros < #poles, place the surplus zeros
///   at the ORIGIN (true, MATLAB's zp2sos convention — empty biquad
///   sections become [0 0 g]) or leave them at infinity (false — empty
///   sections stay [g 0 0], used by tf2sos to reproduce the input b).
std::tuple<Value, double>
zp2sosWithGain(const Value &                zeros,
               const Value &                poles,
               double                       gain,
               std::pmr::memory_resource *  mr = nullptr,
               bool                         surplusAtOrigin = true);

/// Convert a transfer-function pair (b, a) to an SOS matrix.
///
/// Internally: `roots(b)` → zeros, `roots(a)` → poles,
/// `gain = b[0] / a[0]`, then dispatches to `zp2sos`.
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @param mr  Memory resource (nullptr → process default).
/// @return    SOS matrix (see `zp2sos`).
///
/// @see tf2sosWithGain, zp2sos
Value tf2sos(const Value &                b,
             const Value &                a,
             std::pmr::memory_resource *  mr = nullptr);

/// @brief Two-output form of @ref tf2sos: returns `(sos, g)`.
///
/// Equivalent to @ref tf2sos but returns the residual gain `g` as the
/// second tuple element instead of folding it into the SOS biquads'
/// numerators.
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(sos, g)` — SOS matrix (without gain) and the
///            residual scalar gain.
/// @see tf2sos, zp2sosWithGain
std::tuple<Value, double>
tf2sosWithGain(const Value &b, const Value &a,
               std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
