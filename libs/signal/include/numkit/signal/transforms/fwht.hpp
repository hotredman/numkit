// libs/signal/include/numkit/signal/transforms/fwht.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>

namespace numkit::signal {

/// @brief Fast Walsh-Hadamard transform (`y = fwht(x[, n[, ordering]])`).
///
/// Computes `y = (1/N) · H_natural · x` (the discrete Walsh-Hadamard
/// transform), normalised so that `y(1) = mean(x)`. For matrix `x` the
/// transform is applied column-wise.
///
/// Length handling:
/// - If `n == 0` (omitted), the transform length defaults to the next
///   power of 2 ≥ `length(x)` (zero-padding if needed).
/// - If `n` is given it must be a power of 2: `x` is truncated or
///   zero-padded to length `n`.
///
/// Orderings:
/// - `"sequency"` (default) — Walsh-function order (rows sorted by
///   zero-crossing count). Equivalent to `dyadic` then gray-code
///   permutation `g(i) = i XOR (i>>1)`.
/// - `"hadamard"` — natural Hadamard order (recursive Sylvester construction).
/// - `"dyadic"` — bit-reversed natural order (Paley order).
///
/// @param x         Real input vector / matrix.
/// @param n         Transform length (0 → auto-promote to next pow-2).
/// @param ordering  `"sequency"` (default), `"hadamard"`, or `"dyadic"`.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Walsh-Hadamard coefficients.
/// @see ifwht
Value fwht(const Value &              x,
           std::size_t                n,
           const std::string &        ordering,
           std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse fast Walsh-Hadamard transform
/// (`x = ifwht(y[, n[, ordering]])`).
///
/// Inverse of `fwht`. Because `H_natural · H_natural = N · I`, the
/// inverse multiplies the natural-order coefficients by `H_natural`
/// without the `1/N` scaling — round-trip is exact in integer
/// arithmetic. Non-natural orderings are first permuted back to natural
/// before the inverse butterfly.
///
/// @param y         Walsh-Hadamard coefficient vector / matrix.
/// @param n         Transform length (0 → auto-promote to next pow-2).
/// @param ordering  Matching ordering string used in `fwht`.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Reconstructed signal.
/// @see fwht
Value ifwht(const Value &              y,
            std::size_t                n,
            const std::string &        ordering,
            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
