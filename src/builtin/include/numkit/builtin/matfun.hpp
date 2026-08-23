// src/builtin/include/numkit/builtin/matfun.hpp
//
// Pure C++ Matrix functions and integer division (MATLAB parity).
#pragma once

#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @file
/// @brief Matrix functions, integer division with rounding modes, and matrix algebra.
///
/// Provides a clean, engine-free C++ API for integer division with rounding modes
/// and matrix arithmetic utility functions.

// ── Integer Division ────────────────────────────────────────────────────────

/// @brief Integer division with specified rounding mode (`idivide(A, B, opt)`).
///
/// Divides @p a by @p b with integer rounding according to @p mode.
/// At least one operand must be an integer class (`int8`..`int64`, `uint8`..`uint64`).
/// The other operand can be a same-class integer or a scalar double.
///
/// @param a Numerator matrix or scalar.
/// @param b Denominator matrix or scalar.
/// @param mode Rounding mode: `"fix"` (towards zero, default), `"floor"` (towards -inf),
///             `"ceil"` (towards +inf), or `"round"` (nearest integer).
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Quotient after integer division, maintaining integer type of operands.
/// @throws std::runtime_error If neither argument is an integer class or if invalid mode is passed.
/// @see numkit::builtin::rdivide, numkit::builtin::ldivide
Value idivide(const Value &a, const Value &b, const std::string &mode = "fix", std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
