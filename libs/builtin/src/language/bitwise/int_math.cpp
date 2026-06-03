// libs/builtin/src/datatypes/numeric/int_math.cpp
//
// Phase 7: integer-flavored numeric utilities — gcd / lcm and the
// bit* family. All inputs flow through the existing elementwise
// double pipeline; the operation cast to int64_t internally and
// the result casts back to double for storage. Bit ops on values
// outside the [-2^53, 2^53] safe integer range degrade like MATLAB:
// the round-trip through double rounds.

#include <numkit/builtin/language/bitwise/int_math.hpp>

#include <numkit/builtin/language/types/types.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value_type.hpp>

#include "helpers.hpp"

#include <cmath>
#include <cstdint>

namespace numkit::builtin {

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

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════
namespace detail {

namespace {

// Pick the integer class that bitand/bitor/bitxor results follow.
// MATLAB rules (matching idivide's cousin): both int → same class
// (mixed int classes error), one int + scalar double → int's class,
// double-double → DOUBLE (bit ops accept double as a historical
// convenience). See BUGS.md #13.
ValueType pickBitwiseResultType(const Value &a, const Value &b, const char *fn)
{
    const ValueType t0 = a.type();
    const ValueType t1 = b.type();
    const bool int0 = isIntegerType(t0);
    const bool int1 = isIntegerType(t1);
    const bool dbl0 = (t0 == ValueType::DOUBLE);
    const bool dbl1 = (t1 == ValueType::DOUBLE);

    if (!int0 && !int1) return ValueType::DOUBLE;
    if (int0 && int1) {
        if (t0 != t1)
            throw Error(std::string(fn) + ": integer inputs must be the same class",
                         0, 0, fn, "",
                         std::string("numkit:") + fn + ":mixedInt");
        return t0;
    }
    // One int, one non-int.
    if (int0) {
        if (!dbl1 || !b.isScalar())
            throw Error(std::string(fn) + ": integer + non-scalar-double mix",
                         0, 0, fn, "",
                         std::string("numkit:") + fn + ":badMix");
        return t0;
    }
    if (!dbl0 || !a.isScalar())
        throw Error(std::string(fn) + ": integer + non-scalar-double mix",
                     0, 0, fn, "",
                     std::string("numkit:") + fn + ":badMix");
    return t1;
}

// Run binary bitwise op `fn` (takes mr + two DOUBLE Values) over
// possibly-int inputs. Casts both inputs to DOUBLE; restores the
// integer class on the result if MATLAB would.
template <typename Fn>
Value runBitwiseBinary(const Value &a, const Value &b, const char *fnName, Fn fn, std::pmr::memory_resource *mr)
{
    const ValueType rt = pickBitwiseResultType(a, b, fnName);
    Value ad = (a.type() == ValueType::DOUBLE) ? a : toDouble(a, mr);
    Value bd = (b.type() == ValueType::DOUBLE) ? b : toDouble(b, mr);
    Value r = fn(ad, bd, mr);
    if (rt != ValueType::DOUBLE)
        r = cast(r, mtypeName(rt), mr);
    return r;
}

} // namespace

// Extended Euclidean algorithm. Returns g = gcd >= 0 and sets the Bezout
// coefficients u,v such that a*u + b*v = g, matching MATLAB R2025b's
// [g,u,v]=gcd(a,b) convention: standard iterative algorithm with C++
// truncating division, then normalize g >= 0 (negate u,v with it); the
// special case gcd(0,0) returns [0,0,0].
inline int64_t extGcd(int64_t a, int64_t b, int64_t &u, int64_t &v)
{
    if (a == 0 && b == 0) { u = 0; v = 0; return 0; }
    int64_t old_r = a, r = b;
    int64_t old_s = 1, s = 0;
    int64_t old_t = 0, t = 1;
    while (r != 0) {
        const int64_t q = old_r / r;
        int64_t tmp;
        tmp = old_r - q * r; old_r = r; r = tmp;
        tmp = old_s - q * s; old_s = s; s = tmp;
        tmp = old_t - q * t; old_t = t; t = tmp;
    }
    int64_t g = old_r;
    u = old_s;
    v = old_t;
    if (g < 0) { g = -g; u = -u; v = -v; }
    return g;
}

#define NK_BIN_REG_BIT(name, fn)                                                       \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                       \
                    Span<Value> outs, CallContext &ctx)                               \
    {                                                                                  \
        if (args.size() < 2)                                                           \
            throw Error(#name ": requires 2 arguments",                               \
                         0, 0, #name, "", "numkit:" #name ":nargin");                       \
        outs[0] = runBitwiseBinary(args[0], args[1], \
                                   #name, fn, ctx.engine->resource());                                         \
    }

#define NK_BIN_REG(name, fn)                                                   \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,               \
                    Span<Value> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.size() < 2)                                                   \
            throw Error(#name ": requires 2 arguments",                       \
                         0, 0, #name, "", "numkit:" #name ":nargin");               \
        outs[0] = fn(args[0], args[1], ctx.engine->resource());               \
    }

NK_BIN_REG(lcm,      lcm)
NK_BIN_REG_BIT(bitand,   bitand_)
NK_BIN_REG_BIT(bitor,    bitor_)
NK_BIN_REG_BIT(bitxor,   bitxor_)
NK_BIN_REG_BIT(bitshift, bitshift)

#undef NK_BIN_REG
#undef NK_BIN_REG_BIT

// gcd: 1-output is the gcd; [g,u,v]=gcd(a,b) also returns the Bezout
// coefficients (extended Euclid), elementwise. lcm is unaffected.
void gcd_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("gcd: requires 2 arguments",
                     0, 0, "gcd", "", "numkit:gcd:nargin");
    auto *mr = ctx.engine->resource();
    outs[0] = gcd(args[0], args[1], mr);
    if (nargout >= 2)
        outs[1] = elementwiseDouble(args[0], args[1], [](double xv, double yv) {
            int64_t u, v;
            extGcd(toInt64(xv), toInt64(yv), u, v);
            return static_cast<double>(u);
        }, mr);
    if (nargout >= 3)
        outs[2] = elementwiseDouble(args[0], args[1], [](double xv, double yv) {
            int64_t u, v;
            extGcd(toInt64(xv), toInt64(yv), u, v);
            return static_cast<double>(v);
        }, mr);
}

void bitcmp_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("bitcmp: requires at least 1 argument",
                     0, 0, "bitcmp", "", "numkit:bitcmp:nargin");
    auto *mr = ctx.engine->resource();

    int width = 0;
    std::string explicitType;       // empty = inferred or default

    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].isChar() || args[1].isString()) {
            const auto t = args[1].toString();
            if      (t == "uint8"  || t == "int8")  { width = 8;  explicitType = t; }
            else if (t == "uint16" || t == "int16") { width = 16; explicitType = t; }
            else if (t == "uint32" || t == "int32") { width = 32; explicitType = t; }
            else if (t == "uint64" || t == "int64") { width = 64; explicitType = t; }
            else
                throw Error("bitcmp: unknown type name",
                             0, 0, "bitcmp", "", "numkit:bitcmp:badType");
        } else {
            width = static_cast<int>(args[1].toScalar());
        }
    } else {
        // 1-arg form: width / class is inferred from the input type.
        // MATLAB requires an integer-typed input here. See BUGS.md #13.
        const ValueType t = args[0].type();
        if (!isIntegerType(t))
            throw Error(
                "bitcmp: 1-arg form requires an integer input "
                "(use bitcmp(A, 'uintN') for double inputs).",
                0, 0, "bitcmp", "", "numkit:bitcmp:doubleNeedsClass");
        explicitType = mtypeName(t);
        switch (t) {
        case ValueType::INT8: case ValueType::UINT8:  width = 8;  break;
        case ValueType::INT16: case ValueType::UINT16: width = 16; break;
        case ValueType::INT32: case ValueType::UINT32: width = 32; break;
        case ValueType::INT64: case ValueType::UINT64: width = 64; break;
        default: width = 64; break;
        }
    }

    // Run the bitwise complement in DOUBLE space, then cast back to
    // the integer class if specified.
    Value input = isIntegerType(args[0].type()) ? toDouble(args[0], mr) : args[0];
    Value r = bitcmp(input, width, mr);
    if (!explicitType.empty())
        r = cast(r, explicitType, mr);
    outs[0] = std::move(r);
}

void bitset_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bitset: requires (A, n) or (A, n, val)",
                     0, 0, "bitset", "", "numkit:bitset:nargin");
    outs[0] = bitset(args[0], args[1],
                     (args.size() >= 3) ? args[2] : Value::Empty,
                     ctx.engine->resource());
}

void bitget_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bitget: requires (A, n)",
                     0, 0, "bitget", "", "numkit:bitget:nargin");
    outs[0] = bitget(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
