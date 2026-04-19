#include "MLabStdHelpers.hpp"
#include "MLabStdLibrary.hpp"

#include <cmath>
#include <complex>

namespace mlab {

void StdLibrary::registerComplexFunctions(Engine &engine)
{
    engine.registerFunction(
        "real", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isComplex()) {
                if (a.isScalar()) {
                    outs[0] = MValue::scalar(a.toComplex().real(), alloc);
                    return;
                }
                auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
                for (size_t i = 0; i < a.numel(); ++i)
                    r.doubleDataMut()[i] = a.complexData()[i].real();
                {
                    outs[0] = r;
                    return;
                }
            }
            {
                outs[0] = a;
                return;
            }
        });

    engine.registerFunction(
        "imag", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isComplex()) {
                if (a.isScalar()) {
                    outs[0] = MValue::scalar(a.toComplex().imag(), alloc);
                    return;
                }
                auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
                for (size_t i = 0; i < a.numel(); ++i)
                    r.doubleDataMut()[i] = a.complexData()[i].imag();
                {
                    outs[0] = r;
                    return;
                }
            }
            {
                outs[0] = MValue::scalar(0.0, alloc);
                return;
            }
        });

    engine.registerFunction(
        "conj", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isComplex()) {
                outs[0] = unaryComplex(a, [](const Complex &c) { return std::conj(c); }, alloc);
                return;
            }
            {
                outs[0] = a;
                return;
            }
        });

    // complex(R) → R + 0i, same shape.
    // complex(R, I) → R + Ii, elementwise with scalar broadcasting.
    // Real and imag parts must be numeric/logical/char; both scalar or
    // matching shapes (one may be scalar and will broadcast).
    engine.registerFunction("complex",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                auto &a = args[0];
                                if (args.size() == 1) {
                                    if (a.isScalar()) {
                                        outs[0] = MValue::complexScalar(
                                            a.toScalar(), 0.0, alloc);
                                        return;
                                    }
                                    auto r = createLike(a, MType::COMPLEX, alloc);
                                    Complex *dst = r.complexDataMut();
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        dst[i] = Complex(a.elemAsDouble(i), 0.0);
                                    outs[0] = r;
                                    return;
                                }
                                auto &b = args[1];
                                if (a.isScalar() && b.isScalar()) {
                                    outs[0] = MValue::complexScalar(
                                        a.toScalar(), b.toScalar(), alloc);
                                    return;
                                }
                                const MValue &shape = a.isScalar() ? b : a;
                                if (!a.isScalar() && !b.isScalar()
                                    && a.dims() != b.dims())
                                    throw std::runtime_error(
                                        "complex: real and imaginary parts must "
                                        "have matching dimensions");
                                auto r = createLike(shape, MType::COMPLEX, alloc);
                                Complex *dst = r.complexDataMut();
                                size_t n = shape.numel();
                                double aScalar = a.isScalar() ? a.toScalar() : 0.0;
                                double bScalar = b.isScalar() ? b.toScalar() : 0.0;
                                for (size_t i = 0; i < n; ++i) {
                                    double re = a.isScalar() ? aScalar
                                                             : a.elemAsDouble(i);
                                    double im = b.isScalar() ? bScalar
                                                             : b.elemAsDouble(i);
                                    dst[i] = Complex(re, im);
                                }
                                outs[0] = r;
                            });

    engine.registerFunction(
        "angle", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isComplex()) {
                if (a.isScalar()) {
                    outs[0] = MValue::scalar(std::arg(a.toComplex()), alloc);
                    return;
                }
                auto r = createLike(a, MType::DOUBLE, alloc);
                for (size_t i = 0; i < a.numel(); ++i)
                    r.doubleDataMut()[i] = std::arg(a.complexData()[i]);
                outs[0] = r;
                return;
            }
            {
                outs[0] = unaryDouble(a, [](double x) { return std::atan2(0.0, x); }, alloc);
                return;
            }
        });
}

} // namespace mlab