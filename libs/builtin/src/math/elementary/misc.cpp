// libs/builtin/src/math/elementary/misc.cpp
//
// Miscellaneous elementary-math builtins: deg2rad, rad2deg, mod, rem,
// hypot, nthroot.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/math/elementary/misc.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>

namespace numkit::builtin {

Value deg2rad(std::pmr::memory_resource *mr, const Value &x)
{
    constexpr double k = 3.14159265358979323846 / 180.0;
    return unaryDouble(x, [k](double v) { return v * k; }, mr);
}

Value rad2deg(std::pmr::memory_resource *mr, const Value &x)
{
    constexpr double k = 180.0 / 3.14159265358979323846;
    return unaryDouble(x, [k](double v) { return v * k; }, mr);
}

Value mod(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    return elementwiseDouble(a, b,
                             [](double aa, double bb) {
                                 return bb != 0 ? aa - std::floor(aa / bb) * bb : aa;
                             },
                             mr);
}

Value rem(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    return elementwiseDouble(a, b, [](double aa, double bb) { return std::fmod(aa, bb); }, mr);
}

Value hypot(std::pmr::memory_resource *mr, const Value &x, const Value &y)
{
    return elementwiseDouble(x, y,
        [](double a, double b) { return std::hypot(a, b); }, mr);
}

// nthroot(x, n): real n-th root. For negative x with odd integer n,
// returns the negative real root (sign(x) * |x|^(1/n)). For negative x
// with non-odd n, returns NaN.
Value nthroot(std::pmr::memory_resource *mr, const Value &x, const Value &n)
{
    return elementwiseDouble(x, n, [](double xv, double nv) {
        if (nv == 0.0) return std::nan("");
        if (xv >= 0.0) return std::pow(xv, 1.0 / nv);
        const double rounded = std::round(nv);
        if (rounded != nv) return std::nan("");
        const long long ni = static_cast<long long>(rounded);
        if (ni % 2 == 0) return std::nan("");
        return -std::pow(-xv, 1.0 / nv);
    }, mr);
}

// ── Engine adapters ──────────────────────────────────────────────────
namespace detail {

#define NK_UNARY_ADAPTER(name, fn)                                              \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "m:" #name ":nargin");                 \
        outs[0] = fn(ctx.engine->resource(), args[0]);                          \
    }

NK_UNARY_ADAPTER(deg2rad, deg2rad)
NK_UNARY_ADAPTER(rad2deg, rad2deg)

#undef NK_UNARY_ADAPTER

void mod_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mod: requires 2 arguments",
                     0, 0, "mod", "", "m:mod:nargin");
    outs[0] = mod(ctx.engine->resource(), args[0], args[1]);
}

void rem_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rem: requires 2 arguments",
                     0, 0, "rem", "", "m:rem:nargin");
    outs[0] = rem(ctx.engine->resource(), args[0], args[1]);
}

void hypot_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("hypot: requires 2 arguments",
                     0, 0, "hypot", "", "m:hypot:nargin");
    outs[0] = hypot(ctx.engine->resource(), args[0], args[1]);
}

void nthroot_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("nthroot: requires 2 arguments",
                     0, 0, "nthroot", "", "m:nthroot:nargin");
    outs[0] = nthroot(ctx.engine->resource(), args[0], args[1]);
}

} // namespace detail

} // namespace numkit::builtin
