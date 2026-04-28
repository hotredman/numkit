// libs/builtin/src/MStdComplex.cpp

#include <numkit/builtin/math/elementary/complex.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

Value real(Allocator &alloc, const Value &x)
{
    Allocator *p = &alloc;
    if (!x.isComplex())
        return x;
    if (x.isScalar())
        return Value::scalar(x.toComplex().real(), p);
    auto r = createLike(x, ValueType::DOUBLE, p);
    for (size_t i = 0; i < x.numel(); ++i)
        r.doubleDataMut()[i] = x.complexData()[i].real();
    return r;
}

Value imag(Allocator &alloc, const Value &x)
{
    Allocator *p = &alloc;
    if (!x.isComplex())
        return Value::scalar(0.0, p);
    if (x.isScalar())
        return Value::scalar(x.toComplex().imag(), p);
    auto r = createLike(x, ValueType::DOUBLE, p);
    for (size_t i = 0; i < x.numel(); ++i)
        r.doubleDataMut()[i] = x.complexData()[i].imag();
    return r;
}

Value conj(Allocator &alloc, const Value &x)
{
    Allocator *p = &alloc;
    if (!x.isComplex())
        return x;
    return unaryComplex(x, [](const Complex &c) { return std::conj(c); }, p);
}

Value complex(Allocator &alloc, const Value &re)
{
    Allocator *p = &alloc;
    if (re.isScalar())
        return Value::complexScalar(re.toScalar(), 0.0, p);
    auto r = createLike(re, ValueType::COMPLEX, p);
    Complex *dst = r.complexDataMut();
    for (size_t i = 0; i < re.numel(); ++i)
        dst[i] = Complex(re.elemAsDouble(i), 0.0);
    return r;
}

Value complex(Allocator &alloc, const Value &re, const Value &im)
{
    Allocator *p = &alloc;
    if (re.isScalar() && im.isScalar())
        return Value::complexScalar(re.toScalar(), im.toScalar(), p);
    const Value &shape = re.isScalar() ? im : re;
    if (!re.isScalar() && !im.isScalar() && re.dims() != im.dims())
        throw Error("complex: real and imaginary parts must have matching dimensions",
                     0, 0, "complex", "", "m:complex:dimagree");
    auto r = createLike(shape, ValueType::COMPLEX, p);
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

Value angle(Allocator &alloc, const Value &x)
{
    Allocator *p = &alloc;
    if (x.isComplex()) {
        if (x.isScalar())
            return Value::scalar(std::arg(x.toComplex()), p);
        auto r = createLike(x, ValueType::DOUBLE, p);
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

void real_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("real: requires 1 argument", 0, 0, "real", "", "m:real:nargin");
    outs[0] = real(ctx.engine->allocator(), args[0]);
}

void imag_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imag: requires 1 argument", 0, 0, "imag", "", "m:imag:nargin");
    outs[0] = imag(ctx.engine->allocator(), args[0]);
}

void conj_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("conj: requires 1 argument", 0, 0, "conj", "", "m:conj:nargin");
    outs[0] = conj(ctx.engine->allocator(), args[0]);
}

void complex_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("complex: requires 1 or 2 arguments", 0, 0, "complex", "",
                     "m:complex:nargin");
    if (args.size() == 1)
        outs[0] = complex(ctx.engine->allocator(), args[0]);
    else
        outs[0] = complex(ctx.engine->allocator(), args[0], args[1]);
}

void angle_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("angle: requires 1 argument", 0, 0, "angle", "", "m:angle:nargin");
    outs[0] = angle(ctx.engine->allocator(), args[0]);
}

} // namespace detail

} // namespace numkit::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — hand off to MStdLibrary install() via forward decls.
//
// The existing BuiltinLibrary::registerComplexFunctions() is kept (empty) so
// the MStdLibrary.hpp interface is unchanged. The actual registrations
// happen in library.cpp alongside the other Phase-6c builtins.
// ════════════════════════════════════════════════════════════════════════

namespace numkit {

void BuiltinLibrary::registerComplexFunctions(Engine &)
{
    // Intentionally empty — real/imag/conj/complex/angle now register
    // via the Phase-6c function-pointer path in BuiltinLibrary::install().
}

} // namespace numkit
