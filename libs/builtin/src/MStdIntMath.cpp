// libs/builtin/src/MStdIntMath.cpp
//
// Phase 7: integer-flavored numeric utilities — gcd / lcm and the
// bit* family. All inputs flow through the existing elementwise
// double pipeline; the operation cast to int64_t internally and
// the result casts back to double for storage. Bit ops on values
// outside the [-2^53, 2^53] safe integer range degrade like MATLAB:
// the round-trip through double rounds.

#include <numkit/m/builtin/MStdIntMath.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <cmath>
#include <cstdint>

namespace numkit::m::builtin {

namespace {

inline int64_t toInt64(double v)
{
    if (std::isnan(v) || std::isinf(v)) return 0;
    return static_cast<int64_t>(v);
}

inline int64_t gcdInt(int64_t a, int64_t b)
{
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        const int64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

} // namespace

// ────────────────────────────────────────────────────────────────────
// gcd / lcm
// ────────────────────────────────────────────────────────────────────

MValue gcd(Allocator &alloc, const MValue &a, const MValue &b)
{
    return elementwiseDouble(a, b, [](double xv, double yv) {
        return static_cast<double>(gcdInt(toInt64(xv), toInt64(yv)));
    }, &alloc);
}

MValue lcm(Allocator &alloc, const MValue &a, const MValue &b)
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
    }, &alloc);
}

// ────────────────────────────────────────────────────────────────────
// bitwise
// ────────────────────────────────────────────────────────────────────

MValue bitand_(Allocator &alloc, const MValue &a, const MValue &b)
{
    return elementwiseDouble(a, b, [](double xv, double yv) {
        return static_cast<double>(toInt64(xv) & toInt64(yv));
    }, &alloc);
}

MValue bitor_(Allocator &alloc, const MValue &a, const MValue &b)
{
    return elementwiseDouble(a, b, [](double xv, double yv) {
        return static_cast<double>(toInt64(xv) | toInt64(yv));
    }, &alloc);
}

MValue bitxor_(Allocator &alloc, const MValue &a, const MValue &b)
{
    return elementwiseDouble(a, b, [](double xv, double yv) {
        return static_cast<double>(toInt64(xv) ^ toInt64(yv));
    }, &alloc);
}

MValue bitshift(Allocator &alloc, const MValue &a, const MValue &k)
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
    }, &alloc);
}

MValue bitcmp(Allocator &alloc, const MValue &a, int width)
{
    if (width != 8 && width != 16 && width != 32 && width != 64)
        throw MError("bitcmp: width must be 8, 16, 32, or 64",
                     0, 0, "bitcmp", "", "m:bitcmp:badWidth");
    const uint64_t mask = (width == 64) ? ~uint64_t{0}
                                        : ((uint64_t{1} << width) - 1);
    return unaryDouble(a, [mask](double xv) {
        const uint64_t ux = static_cast<uint64_t>(toInt64(xv));
        return static_cast<double>((~ux) & mask);
    }, &alloc);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════
namespace detail {

#define NK_BIN_REG(name, fn)                                                   \
    void name##_reg(Span<const MValue> args, size_t /*nargout*/,               \
                    Span<MValue> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.size() < 2)                                                   \
            throw MError(#name ": requires 2 arguments",                       \
                         0, 0, #name, "", "m:" #name ":nargin");               \
        outs[0] = fn(ctx.engine->allocator(), args[0], args[1]);               \
    }

NK_BIN_REG(gcd,      gcd)
NK_BIN_REG(lcm,      lcm)
NK_BIN_REG(bitand,   bitand_)
NK_BIN_REG(bitor,    bitor_)
NK_BIN_REG(bitxor,   bitxor_)
NK_BIN_REG(bitshift, bitshift)

#undef NK_BIN_REG

void bitcmp_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw MError("bitcmp: requires at least 1 argument",
                     0, 0, "bitcmp", "", "m:bitcmp:nargin");
    int width = 64;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].isChar() || args[1].isString()) {
            const auto t = args[1].toString();
            if (t == "uint8"  || t == "int8")  width = 8;
            else if (t == "uint16" || t == "int16") width = 16;
            else if (t == "uint32" || t == "int32") width = 32;
            else if (t == "uint64" || t == "int64") width = 64;
            else
                throw MError("bitcmp: unknown type name",
                             0, 0, "bitcmp", "", "m:bitcmp:badType");
        } else {
            width = static_cast<int>(args[1].toScalar());
        }
    }
    outs[0] = bitcmp(ctx.engine->allocator(), args[0], width);
}

} // namespace detail

} // namespace numkit::m::builtin
