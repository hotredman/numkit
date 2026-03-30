#include "MLabStdLibrary.hpp"
#include "MLabStdHelpers.hpp"

#include <complex>
#include <functional>
#include <stdexcept>

namespace mlab {

void StdLibrary::registerUnaryOps(Engine &engine)
{
    // --- Unary minus ---
    engine.registerUnaryOp("-", [&engine](const MValue &a) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isComplex())
            return unaryComplex(a, std::negate<Complex>{}, alloc);
        if (a.type() == MType::DOUBLE)
            return unaryDouble(a, std::negate<double>{}, alloc);
        throw std::runtime_error("Unsupported unary -");
    });

    // --- Logical not (element-wise) ---
    engine.registerUnaryOp("~", [&engine](const MValue &a) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isLogical()) {
            if (a.isScalar())
                return MValue::logicalScalar(!a.toBool(), alloc);
            auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, alloc);
            const uint8_t *src = a.logicalData();
            uint8_t *dst = r.logicalDataMut();
            for (size_t i = 0; i < a.numel(); ++i)
                dst[i] = src[i] ? 0 : 1;
            return r;
        }
        if (a.type() == MType::DOUBLE) {
            if (a.isScalar())
                return MValue::logicalScalar(a.toScalar() == 0.0, alloc);
            auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, alloc);
            const double *src = a.doubleData();
            uint8_t *dst = r.logicalDataMut();
            for (size_t i = 0; i < a.numel(); ++i)
                dst[i] = (src[i] == 0.0) ? 1 : 0;
            return r;
        }
        return MValue::logicalScalar(!a.toBool(), alloc);
    });

    // --- Unary plus (identity) ---
    engine.registerUnaryOp("+", [](const MValue &a) -> MValue { return a; });

    // --- Conjugate transpose ---
    engine.registerUnaryOp("'", [&engine](const MValue &a) -> MValue {
        auto *alloc = &engine.allocator();
        size_t rows = a.dims().rows(), cols = a.dims().cols();

        if (a.isComplex()) {
            if (a.isScalar())
                return MValue::complexScalar(std::conj(a.toComplex()), alloc);
            auto r = MValue::complexMatrix(cols, rows, alloc);
            for (size_t i = 0; i < rows; ++i)
                for (size_t j = 0; j < cols; ++j)
                    r.complexDataMut()[i * cols + j] = std::conj(a.complexElem(i, j));
            return r;
        }
        if (a.type() == MType::DOUBLE) {
            if (a.isScalar())
                return MValue::scalar(a.toScalar(), alloc);
            auto r = MValue::matrix(cols, rows, MType::DOUBLE, alloc);
            for (size_t i = 0; i < rows; ++i)
                for (size_t j = 0; j < cols; ++j)
                    r.elem(j, i) = a(i, j);
            return r;
        }
        throw std::runtime_error("Transpose not supported for this type");
    });

    // --- Non-conjugate transpose ---
    engine.registerUnaryOp(".'", [&engine](const MValue &a) -> MValue {
        auto *alloc = &engine.allocator();
        size_t rows = a.dims().rows(), cols = a.dims().cols();

        if (a.isComplex()) {
            if (a.isScalar())
                return MValue::complexScalar(a.toComplex(), alloc);
            auto r = MValue::complexMatrix(cols, rows, alloc);
            for (size_t i = 0; i < rows; ++i)
                for (size_t j = 0; j < cols; ++j)
                    r.complexDataMut()[i * cols + j] = a.complexElem(i, j);
            return r;
        }
        if (a.type() == MType::DOUBLE) {
            if (a.isScalar())
                return MValue::scalar(a.toScalar(), alloc);
            auto r = MValue::matrix(cols, rows, MType::DOUBLE, alloc);
            for (size_t i = 0; i < rows; ++i)
                for (size_t j = 0; j < cols; ++j)
                    r.elem(j, i) = a(i, j);
            return r;
        }
        throw std::runtime_error("Transpose not supported for this type");
    });
}

} // namespace mlab
