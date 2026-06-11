// toolboxes/signal/src/language/bitwise/int_math_reg.cpp
//
// CallContext register half of language/bitwise/int_math.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/lang/bitwise/int_math.hpp>
#include <numkit/lang/types/types.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/value_type.hpp>
#include <numkit/ops/helpers.hpp>
#include "bitwise/int_math_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::builtin {
using namespace numkit::lang;  // C4c localized (umbrella removed)

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
