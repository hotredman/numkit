// libs/signal/src/transforms/transform_helpers.cpp
//
// nextpow2 / fftshift / ifftshift. Split from MDspSignalCore.

#include <numkit/signal/transforms/transform_helpers.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "../dsp_helpers.hpp"  // Complex typedef
#include "helpers.hpp"         // createLike

#include <cmath>
#include <complex>

namespace numkit::signal {

namespace {

// Cyclic shift core used by both fftshift and ifftshift — only the shift
// amount differs between them.
Value cyclicShift(const Value &x, size_t shift, Allocator &alloc)
{
    const size_t N = x.numel();
    if (x.isComplex()) {
        auto r = createLike(x, ValueType::COMPLEX, &alloc);
        const Complex *src = x.complexData();
        Complex *dst = r.complexDataMut();
        for (size_t i = 0; i < N; ++i)
            dst[i] = src[(i + shift) % N];
        return r;
    }
    auto r = createLike(x, ValueType::DOUBLE, &alloc);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    for (size_t i = 0; i < N; ++i)
        dst[i] = src[(i + shift) % N];
    return r;
}

} // anonymous namespace

Value nextpow2(Allocator &alloc, double n)
{
    if (n <= 0)
        return Value::scalar(0.0, &alloc);
    return Value::scalar(std::ceil(std::log2(n)), &alloc);
}

Value fftshift(Allocator &alloc, const Value &x)
{
    return cyclicShift(x, x.numel() / 2, alloc);
}

Value ifftshift(Allocator &alloc, const Value &x)
{
    return cyclicShift(x, (x.numel() + 1) / 2, alloc);
}

namespace detail {

void nextpow2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nextpow2: requires 1 argument",
                     0, 0, "nextpow2", "", "m:nextpow2:nargin");
    outs[0] = nextpow2(ctx.engine->allocator(), args[0].toScalar());
}

void fftshift_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fftshift: requires 1 argument",
                     0, 0, "fftshift", "", "m:fftshift:nargin");
    outs[0] = fftshift(ctx.engine->allocator(), args[0]);
}

void ifftshift_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ifftshift: requires 1 argument",
                     0, 0, "ifftshift", "", "m:ifftshift:nargin");
    outs[0] = ifftshift(ctx.engine->allocator(), args[0]);
}

} // namespace detail

} // namespace numkit::signal
