// toolboxes/builtin/src/datatypes/numeric/int_math.cpp
//
// Phase 7: integer-flavored numeric utilities — gcd / lcm and the
// bit* family. All inputs flow through the existing elementwise
// double pipeline; the operation cast to int64_t internally and
// the result casts back to double for storage. Bit ops on values
// outside the [-2^53, 2^53] safe integer range degrade like MATLAB:
// the round-trip through double rounds.

#include <numkit/builtin/ops.hpp>

#include <numkit/builtin/datatypes.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value_type.hpp>

#include <numkit/ops/helpers.hpp>

#include <cmath>
#include <cstdint>

#include "int_math_detail.hpp"

namespace numkit::builtin {


// ────────────────────────────────────────────────────────────────────
// gcd / lcm
// ────────────────────────────────────────────────────────────────────

Value gcd(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(a, b, [](double xv, double yv) {
        return static_cast<double>(gcdInt(toInt64(xv), toInt64(yv)));
    }, mr);
}

Value lcm(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(a, b, [](double xv, double yv) {
        const int64_t x = toInt64(xv);
        const int64_t y = toInt64(yv);
        if (x == 0 || y == 0) return 0.0;
        const int64_t g = gcdInt(x, y);
        // |a*b| / g, computed as (|a|/g)*|b| to reduce overflow risk.
        const int64_t ax = (x < 0) ? -x : x;
        const int64_t ay = (y < 0) ? -y : y;
        return static_cast<double>((ax / g) * ay);
    }, mr);
}

// ────────────────────────────────────────────────────────────────────
// bitwise
// ────────────────────────────────────────────────────────────────────

Value bitand_(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(a, b, [](double xv, double yv) {
        return static_cast<double>(toInt64(xv) & toInt64(yv));
    }, mr);
}

Value bitor_(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(a, b, [](double xv, double yv) {
        return static_cast<double>(toInt64(xv) | toInt64(yv));
    }, mr);
}

Value bitxor_(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(a, b, [](double xv, double yv) {
        return static_cast<double>(toInt64(xv) ^ toInt64(yv));
    }, mr);
}

Value bitshift(const Value &a, const Value &k, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(a, k, [](double xv, double kv) {
        const int64_t x = toInt64(xv);
        const int64_t shift = toInt64(kv);
        if (shift > 0) {
            // Left shift. Cap at 63 to avoid undefined behavior.
            if (shift >= 64) return 0.0;
            return static_cast<double>(static_cast<int64_t>(
                static_cast<uint64_t>(x) << shift));
        }
        if (shift < 0) {
            const int64_t s = -shift;
            if (s >= 64) {
                // Signed-shift far-right: 0 for non-negative, -1 for negative.
                return (x < 0) ? -1.0 : 0.0;
            }
            // Arithmetic right shift: implementation-defined in C++ for
            // negative values pre-C++20. We do it explicitly to be safe.
            if (x >= 0) return static_cast<double>(x >> s);
            const uint64_t ux = static_cast<uint64_t>(x);
            const uint64_t mask = ~uint64_t{0} << (64 - s);
            return static_cast<double>(static_cast<int64_t>((ux >> s) | mask));
        }
        return static_cast<double>(x);
    }, mr);
}

Value bitcmp(const Value &a, int width, std::pmr::memory_resource *mr)
{
    if (width != 8 && width != 16 && width != 32 && width != 64)
        throw Error("bitcmp: width must be 8, 16, 32, or 64",
                     0, 0, "bitcmp", "", "numkit:bitcmp:badWidth");
    const uint64_t mask = (width == 64) ? ~uint64_t{0}
                                        : ((uint64_t{1} << width) - 1);
    return unaryDouble(a, [mask](double xv) {
        const uint64_t ux = static_cast<uint64_t>(toInt64(xv));
        return static_cast<double>((~ux) & mask);
    }, mr);
}

// bitset(A, n)        — set bit n (1-based) of each A_i to 1.
// bitset(A, n, val)   — set bit n to `val` (0 or 1).
// MATLAB convention: bit 1 is the LSB.
Value bitset(const Value &a, const Value &n, const Value &val, std::pmr::memory_resource *mr)
{
    const int v = val.isEmpty() ? 1 : static_cast<int>(val.toScalar());
    if (v != 0 && v != 1)
        throw Error("bitset: third argument must be 0 or 1",
                     0, 0, "bitset", "", "numkit:bitset:badVal");
    return elementwiseDouble(a, n, [v](double xv, double nv) {
        const int64_t x = toInt64(xv);
        const int bit = static_cast<int>(nv);
        if (bit < 1 || bit > 64) return xv;
        const uint64_t mask = uint64_t{1} << (bit - 1);
        const uint64_t ux = static_cast<uint64_t>(x);
        const uint64_t r  = (v == 1) ? (ux | mask) : (ux & ~mask);
        return static_cast<double>(static_cast<int64_t>(r));
    }, mr);
}

// bitget(A, n)        — return the n-th bit (0 or 1) of each A_i.
Value bitget(const Value &a, const Value &n, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(a, n, [](double xv, double nv) {
        const int64_t x = toInt64(xv);
        const int bit = static_cast<int>(nv);
        if (bit < 1 || bit > 64) return 0.0;
        const uint64_t ux = static_cast<uint64_t>(x);
        return static_cast<double>((ux >> (bit - 1)) & 1u);
    }, mr);
}

} // namespace numkit::builtin
