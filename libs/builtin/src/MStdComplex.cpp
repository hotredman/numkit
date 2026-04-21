// libs/builtin/src/MStdComplex.cpp

#include <numkit/m/builtin/MStdComplex.hpp>
#include <numkit/m/builtin/MStdLibrary.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

MValue real(Allocator &alloc, const MValue &x)
{
    Allocator *p = &alloc;
    if (!x.isComplex())
        return x;
    if (x.isScalar())
        return MValue::scalar(x.toComplex().real(), p);
    auto r = createLike(x, MType::DOUBLE, p);
    for (size_t i = 0; i < x.numel(); ++i)
        r.doubleDataMut()[i] = x.complexData()[i].real();
    return r;
}

MValue imag(Allocator &alloc, const MValue &x)
{
    Allocator *p = &alloc;
    if (!x.isComplex())
        return MValue::scalar(0.0, p);
    if (x.isScalar())
        return MValue::scalar(x.toComplex().imag(), p);
    auto r = createLike(x, MType::DOUBLE, p);
    for (size_t i = 0; i < x.numel(); ++i)
        r.doubleDataMut()[i] = x.complexData()[i].imag();
    return r;
}

MValue conj(Allocator &alloc, const MValue &x)
{
    Allocator *p = &alloc;
    if (!x.isComplex())
        return x;
    return unaryComplex(x, [](const Complex &c) { return std::conj(c); }, p);
}

MValue complex(Allocator &alloc, const MValue &re)
{
    Allocator *p = &alloc;
    if (re.isScalar())
        return MValue::complexScalar(re.toScalar(), 0.0, p);
    auto r = createLike(re, MType::COMPLEX, p);
    Complex *dst = r.complexDataMut();
    for (size_t i = 0; i < re.numel(); ++i)
        dst[i] = Complex(re.elemAsDouble(i), 0.0);
    return r;
}

MValue complex(Allocator &alloc, const MValue &re, const MValue &im)
{
    Allocator *p = &alloc;
    if (re.isScalar() && im.isScalar())
        return MValue::complexScalar(re.toScalar(), im.toScalar(), p);
    const MValue &shape = re.isScalar() ? im : re;
    if (!re.isScalar() && !im.isScalar() && re.dims() != im.dims())
        throw MError("complex: real and imaginary parts must have matching dimensions",
                     0, 0, "complex", "", "m:complex:dimagree");
    auto r = createLike(shape, MType::COMPLEX, p);
    Complex *dst = r.complexDataMut();
    const size_t n = shape.numel();
    const double reScalar = re.isScalar() ? re.toScalar() : 0.0;
    const double imScalar = im.isScalar() ? im.toScalar() : 0.0;
    for (size_t i = 0; i < n; ++i) {
        double r0 = re.isScalar() ? reScalar : re.elemAsDouble(i);
        double i0 = im.isScalar() ? imScalar : im.elemAsDouble(i);
        dst[i] = Complex(r0, i0);
    }
    return r;
}

MValue angle(Allocator &alloc, const MValue &x)
{
    Allocator *p = &alloc;
    if (x.isComplex()) {
        if (x.isScalar())
            return MValue::scalar(std::arg(x.toComplex()), p);
        auto r = createLike(x, MType::DOUBLE, p);
        for (size_t i = 0; i < x.numel(); ++i)
            r.doubleDataMut()[i] = std::arg(x.complexData()[i]);
        return r;
    }
    return unaryDouble(x, [](double v) { return std::atan2(0.0, v); }, p);
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void real_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("real: requires 1 argument", 0, 0, "real", "", "m:real:nargin");
    outs[0] = real(ctx.engine->allocator(), args[0]);
}

void imag_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("imag: requires 1 argument", 0, 0, "imag", "", "m:imag:nargin");
    outs[0] = imag(ctx.engine->allocator(), args[0]);
}

void conj_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("conj: requires 1 argument", 0, 0, "conj", "", "m:conj:nargin");
    outs[0] = conj(ctx.engine->allocator(), args[0]);
}

void complex_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("complex: requires 1 or 2 arguments", 0, 0, "complex", "",
                     "m:complex:nargin");
    if (args.size() == 1)
        outs[0] = complex(ctx.engine->allocator(), args[0]);
    else
        outs[0] = complex(ctx.engine->allocator(), args[0], args[1]);
}

void angle_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("angle: requires 1 argument", 0, 0, "angle", "", "m:angle:nargin");
    outs[0] = angle(ctx.engine->allocator(), args[0]);
}

} // namespace detail

} // namespace numkit::m::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — hand off to MStdLibrary install() via forward decls.
//
// The existing StdLibrary::registerComplexFunctions() is kept (empty) so
// the MStdLibrary.hpp interface is unchanged. The actual registrations
// happen in MStdLibrary.cpp alongside the other Phase-6c builtins.
// ════════════════════════════════════════════════════════════════════════

namespace numkit::m {

void StdLibrary::registerComplexFunctions(Engine &)
{
    // Intentionally empty — real/imag/conj/complex/angle now register
    // via the Phase-6c function-pointer path in StdLibrary::install().
}

} // namespace numkit::m
