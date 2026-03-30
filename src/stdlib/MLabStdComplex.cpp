#include "MLabStdLibrary.hpp"
#include "MLabStdHelpers.hpp"

#include <cmath>
#include <complex>

namespace mlab {

void StdLibrary::registerComplexFunctions(Engine &engine)
{
    engine.registerFunction("real",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isComplex()) {
                                    if (a.isScalar())
                                        return {MValue::scalar(a.toComplex().real(), alloc)};
                                    auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        r.doubleDataMut()[i] = a.complexData()[i].real();
                                    return {r};
                                }
                                return {a};
                            });

    engine.registerFunction("imag",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isComplex()) {
                                    if (a.isScalar())
                                        return {MValue::scalar(a.toComplex().imag(), alloc)};
                                    auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        r.doubleDataMut()[i] = a.complexData()[i].imag();
                                    return {r};
                                }
                                return {MValue::scalar(0.0, alloc)};
                            });

    engine.registerFunction("conj", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::conj(c); }, alloc)};
        return {a};
    });

    engine.registerFunction("complex",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.size() == 1)
                                    return {MValue::complexScalar(args[0].toScalar(), 0.0, alloc)};
                                return {MValue::complexScalar(args[0].toScalar(), args[1].toScalar(), alloc)};
                            });

    engine.registerFunction("angle", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex()) {
            if (a.isScalar())
                return {MValue::scalar(std::arg(a.toComplex()), alloc)};
            auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
            for (size_t i = 0; i < a.numel(); ++i)
                r.doubleDataMut()[i] = std::arg(a.complexData()[i]);
            return {r};
        }
        return {unaryDouble(a, [](double x) { return std::atan2(0.0, x); }, alloc)};
    });
}

} // namespace mlab
