// libs/comm/include/numkit/comm/eq/pulse.hpp
//
// Pulse-shaping filter design (raised-cosine, root-raised-cosine,
// Gaussian) and rectangular pulse / integrate-and-dump operators.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>

namespace numkit::comm {

/// @brief Design a raised-cosine or root-raised-cosine FIR filter
/// (`h = rcosdesign(beta, span, sps, shape)`).
///
/// Closed-form coefficients:
/// - RC:  `p(t) = sinc(t/T) · cos(πβt/T) / (1 − (2βt/T)²)`
/// - RRC: `h(t) = (4β/π√T) · [cos((1+β)πt/T) + sin((1−β)πt/T)/(4βt/T)]
///        / (1 − (4βt/T)²)`
///
/// Standard l'Hôpital limits applied at `t = 0` and `t = ±T/(4β)`.
/// Returns a row vector of length `span·sps + 1`. Both RC and RRC
/// outputs are unit-energy normalised (`‖h‖² = 1`).
///
/// @param beta   Roll-off factor, `0 ≤ β ≤ 1`.
/// @param span   Filter span in symbol periods (positive integer).
/// @param sps    Samples per symbol (positive integer).
/// @param shape  `"normal"` → RC (default), `"sqrt"` → RRC.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Row vector of length `span·sps + 1`.
/// @throws Error On invalid β, span, sps, or unknown shape.
/// @see gaussdesign
Value rcosdesign(double beta, int span, int sps,
                 const std::string &shape = "normal",
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Design a Gaussian FIR pulse-shaping filter
/// (`h = gaussdesign(BT, span, sps)`).
///
/// Formula:
/// - `α = √(log 2 / 2) / BT`
/// - `h(t) = (√π / α) · exp(−(πt/α)²)`, evaluated on
///   `t = ((1:N) − mean(1:N)) / sps` with `N = span·sps + 1`.
/// - Output is sum-normalised (`Σh = 1`).
///
/// @param BT    3-dB bandwidth × symbol period (typical 0.1..0.5).
/// @param span  Filter span in symbol periods (positive integer).
/// @param sps   Samples per symbol (positive integer).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Row vector of length `span·sps + 1`, sum-normalised.
/// @throws Error On invalid BT, span, or sps.
/// @see rcosdesign
Value gaussdesign(double BT, int span, int sps,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief Rectangular pulse shaping (`y = rectpulse(x, n)`).
///
/// Repeats each sample of `x` `n` times along the leading
/// non-singleton dimension. For an L×1 column input → (L·n)×1 output;
/// for 1×L row → 1×(L·n); for matrices each row is repeated `n` times
/// (column count unchanged).
///
/// @param x   Input signal (vector or matrix, DOUBLE).
/// @param n   Repetition factor (positive integer).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Expanded signal with same orientation as `x`.
/// @throws Error On non-positive `n`.
/// @see intdump
Value rectpulse(const Value &x, int n,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Integrate-and-dump (`y = intdump(x, n)`).
///
/// Averages each `n` consecutive samples of `x` along the leading
/// non-singleton dimension. Algebraic inverse of @ref rectpulse for
/// length-divisible inputs.
///
/// @param x   Input signal (vector or matrix, DOUBLE).
/// @param n   Block size (positive integer).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Averaged signal with same orientation as `x`.
/// @throws Error If the relevant dimension is not divisible by `n`,
///               or `n` is non-positive.
/// @see rectpulse
Value intdump(const Value &x, int n,
              std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
