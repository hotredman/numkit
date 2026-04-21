// libs/dsp/src/MDspSignalCore.cpp

#include <numkit/m/dsp/MDspSignalCore.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MDspHelpers.hpp"   // Complex typedef
#include "MStdHelpers.hpp"   // createLike

#include <cmath>

namespace numkit::m::dsp {

namespace {

// Cyclic shift core used by both fftshift and ifftshift — only the shift
// amount differs between them.
MValue cyclicShift(const MValue &x, size_t shift, Allocator &alloc)
{
    const size_t N = x.numel();
    if (x.isComplex()) {
        auto r = createLike(x, MType::COMPLEX, &alloc);
        const Complex *src = x.complexData();
        Complex *dst = r.complexDataMut();
        for (size_t i = 0; i < N; ++i)
            dst[i] = src[(i + shift) % N];
        return r;
    }
    auto r = createLike(x, MType::DOUBLE, &alloc);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    for (size_t i = 0; i < N; ++i)
        dst[i] = src[(i + shift) % N];
    return r;
}

} // anonymous namespace

// ── nextpow2 ──────────────────────────────────────────────────────────
MValue nextpow2(Allocator &alloc, double n)
{
    if (n <= 0)
        return MValue::scalar(0.0, &alloc);
    return MValue::scalar(std::ceil(std::log2(n)), &alloc);
}

// ── fftshift ──────────────────────────────────────────────────────────
MValue fftshift(Allocator &alloc, const MValue &x)
{
    return cyclicShift(x, x.numel() / 2, alloc);
}

// ── ifftshift ─────────────────────────────────────────────────────────
MValue ifftshift(Allocator &alloc, const MValue &x)
{
    return cyclicShift(x, (x.numel() + 1) / 2, alloc);
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void nextpow2_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("nextpow2: requires 1 argument",
                     0, 0, "nextpow2", "", "MATLAB:nextpow2:nargin");
    outs[0] = nextpow2(ctx.engine->allocator(), args[0].toScalar());
}

void fftshift_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("fftshift: requires 1 argument",
                     0, 0, "fftshift", "", "MATLAB:fftshift:nargin");
    outs[0] = fftshift(ctx.engine->allocator(), args[0]);
}

void ifftshift_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("ifftshift: requires 1 argument",
                     0, 0, "ifftshift", "", "MATLAB:ifftshift:nargin");
    outs[0] = ifftshift(ctx.engine->allocator(), args[0]);
}

} // namespace detail

} // namespace numkit::m::dsp
