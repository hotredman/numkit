/// @file base_conversions.hpp
/// @ingroup group_comm
// toolboxes/comm/include/numkit/comm/source/base_conversions.hpp
//
// Communications Toolbox base-conversion utilities: bit <-> integer
// packing (bit2int / int2bit) and the legacy binary <-> decimal digit
// converters (bi2de / de2bi), plus the vec2mat reshape-with-padding helper.

#pragma once

#include <memory_resource>
#include <utility>
#include <numkit/value/value.hpp>

namespace numkit::comm {

/// @addtogroup group_comm
/// @{


/// @brief Pack a bit vector into integers — `y = bit2int(b, n)`.
///
/// Groups the `numel(b)` bits of `b` (which must be divisible by `n`) into
/// `numel(b) / n` integers, `n` bits each, MSB-first by default. Returns a
/// column vector of the packed integer values.
///
/// @param b         Bit values (numel divisible by `n`).
/// @param n         Bits per integer, in `1..64`.
/// @param msbfirst  `true` (default) packs MSB-first; `false` LSB-first.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Column vector of `numel(b) / n` packed integers.
/// @throws Error if `n ∉ 1..64` or `numel(b)` is not divisible by `n`.
/// @see int2bit
Value bit2int(const Value &b, int n, bool msbfirst = true,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Unpack integers into a bit matrix — `b = int2bit(d, n)`.
///
/// Inverse of @ref bit2int: each of the `numel(d)` integers becomes one
/// column of `n` bits, giving an `n × numel(d)` matrix (MSB-first by
/// default).
///
/// @param d         Integer values.
/// @param n         Bits per integer, in `1..64`.
/// @param msbfirst  `true` (default) emits MSB-first; `false` LSB-first.
/// @param mr        Memory resource (nullptr → process default).
/// @return          `n × numel(d)` bit matrix.
/// @throws Error if `n ∉ 1..64`.
/// @see bit2int
Value int2bit(const Value &d, int n, bool msbfirst = true,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Convert digit-rows to decimal — `d = bi2de(b)` (legacy).
///
/// Each row of the 2-D matrix `b` is a group of base-`base` digits; returns
/// a column vector with the decimal value of each row. Defaults to base 2,
/// LSB-first (`'right-msb'`). MATLAB documents `bi2de` as a legacy synonym
/// (superseded by @ref bit2int) but it still ships in R2025b.
///
/// @param b         2-D matrix of digit rows.
/// @param base      Digit base, `>= 2` (default 2).
/// @param msbfirst  `false` (default, `'right-msb'`) treats the first column
///                  as least significant; `true` (`'left-msb'`) reverses.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Column vector of per-row decimal values.
/// @throws Error if `b` is not 2-D or `base < 2`.
/// @see de2bi, bit2int
Value bi2de(const Value &b, int base = 2, bool msbfirst = false,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Convert integers to digit-rows — `b = de2bi(d)` (legacy).
///
/// Inverse of @ref bi2de: each input integer becomes one row of base-`base`
/// digits. Width `n` is the minimum number of digits; when `n <= 0` it is
/// computed from the largest input. Defaults to base 2, LSB-first
/// (`'right-msb'`). Legacy synonym of @ref int2bit (still in R2025b).
///
/// @param d         Integer values.
/// @param n         Digit width; `<= 0` (default `-1`) auto-sizes from `max(d)`.
/// @param base      Digit base, `>= 2` (default 2).
/// @param msbfirst  `false` (default, `'right-msb'`) puts the least
///                  significant digit first; `true` (`'left-msb'`) reverses.
/// @param mr        Memory resource (nullptr → process default).
/// @return          `numel(d) × n` matrix of digit rows.
/// @throws Error if `base < 2`.
/// @see bi2de, int2bit
Value de2bi(const Value &d, int n = -1, int base = 2, bool msbfirst = false,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Reshape a vector into an N-column matrix with padding —
/// `[m, pad] = vec2mat(v, n)`.
///
/// Row-major fill of `v` into a `ceil(numel(v)/n) × n` matrix, padding the
/// last row to width `n` with `padval`. Returns the matrix and the count of
/// pad entries added. MATLAB documents `vec2mat` as legacy (superseded by
/// `reshape`), but it still ships in R2025b.
///
/// @param v       Input vector.
/// @param n       Number of columns (positive).
/// @param padval  Pad value for the last row (default 0).
/// @param mr      Memory resource (nullptr → process default).
/// @return        `{ matrix, padCount }` — the reshaped matrix and the
///                number of pad entries added to fill the final row.
/// @throws Error if `n <= 0`.
std::pair<Value, int>
vec2mat(const Value &v, int n, double padval = 0.0,
        std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::comm
