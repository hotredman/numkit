#include "MLabStdLibrary.hpp"
#include "MLabStdHelpers.hpp"

#include <cmath>
#include <complex>

namespace mlab {

void StdLibrary::registerComplexFunctions(Engine &engine)
{
    engine.registerFunction("real",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isComplex()) {
                                    if (a.isScalar())
                                        { outs[0] = MValue::scalar(a.toComplex().real(), alloc); return; }
                                    auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        r.doubleDataMut()[i] = a.complexData()[i].real();
                                    { outs[0] = r; return; }
                                }
                                { outs[0] = a; return; }
                            });

    engine.registerFunction("imag",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isComplex()) {
                                    if (a.isScalar())
                                        { outs[0] = MValue::scalar(a.toComplex().imag(), alloc); return; }
                                    auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        r.doubleDataMut()[i] = a.complexData()[i].imag();
                                    { outs[0] = r; return; }
                                }
                                { outs[0] = MValue::scalar(0.0, alloc); return; }
                            });

    engine.registerFunction("conj", [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            { outs[0] = unaryComplex(a, [](const Complex &c) { return std::conj(c); }, alloc); return; }
        { outs[0] = a; return; }
    });

    engine.registerFunction("complex",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                auto *alloc = &engine.allocator();
                                if (args.size() == 1)
                                    { outs[0] = MValue::complexScalar(args[0].toScalar(), 0.0, alloc); return; }
                                { outs[0] = MValue::complexScalar(args[0].toScalar(), args[1].toScalar(), alloc); return; }
                            });

    engine.registerFunction("angle", [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex()) {
            if (a.isScalar())
                { outs[0] = MValue::scalar(std::arg(a.toComplex()), alloc); return; }
            auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
            for (size_t i = 0; i < a.numel(); ++i)
                r.doubleDataMut()[i] = std::arg(a.complexData()[i]);
            { outs[0] = r; return; }
        }
        { outs[0] = unaryDouble(a, [](double x) { return std::atan2(0.0, x); }, alloc); return; }
    });
}

} // namespace mlab
