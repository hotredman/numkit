#include "MStdLibrary.hpp"
#include "MStdHelpers.hpp"

#include <complex>
#include <functional>
#include <stdexcept>

namespace numkit::m {

void StdLibrary::registerUnaryOps(Engine &engine)
{
    // --- Unary minus ---
    engine.registerUnaryOp("-", [&engine](const MValue &a) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isEmpty()) {
            // Preserve shape. Char/logical promote to double empty
            // (MATLAB: -'' → 1x0 double, -false(3,0) → 3x0 double).
            MType outType = a.isComplex()                   ? MType::COMPLEX
                            : (a.isChar() || a.isLogical()) ? MType::DOUBLE
                                                            : a.type();
            return createLike(a, outType, alloc);
        }
        if (a.isComplex())
            return unaryComplex(a, std::negate<Complex>{}, alloc);
        if (a.type() == MType::DOUBLE)
            return unaryDouble(a, std::negate<double>{}, alloc);
        if (a.type() == MType::SINGLE)
            return unaryTyped<float>(a, MType::SINGLE, [](float x) { return -x; }, alloc);
        if (isIntegerType(a.type())) {
            switch (a.type()) {
            case MType::INT8:   return unaryTyped<int8_t>(a, a.type(), [](int8_t x) { return saturateNeg(x); }, alloc);
            case MType::INT16:  return unaryTyped<int16_t>(a, a.type(), [](int16_t x) { return saturateNeg(x); }, alloc);
            case MType::INT32:  return unaryTyped<int32_t>(a, a.type(), [](int32_t x) { return saturateNeg(x); }, alloc);
            case MType::INT64:  return unaryTyped<int64_t>(a, a.type(), [](int64_t x) { return saturateNeg(x); }, alloc);
            case MType::UINT8:  return unaryTyped<uint8_t>(a, a.type(), [](uint8_t) { return uint8_t(0); }, alloc);
            case MType::UINT16: return unaryTyped<uint16_t>(a, a.type(), [](uint16_t) { return uint16_t(0); }, alloc);
            case MType::UINT32: return unaryTyped<uint32_t>(a, a.type(), [](uint32_t) { return uint32_t(0); }, alloc);
            case MType::UINT64: return unaryTyped<uint64_t>(a, a.type(), [](uint64_t) { return uint64_t(0); }, alloc);
            default: break;
            }
        }
        throw std::runtime_error("Unsupported unary -");
    });

    // --- Logical not (element-wise) ---
    engine.registerUnaryOp("~", [&engine](const MValue &a) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isLogical()) {
            if (a.isScalar())
                return MValue::logicalScalar(!a.toBool(), alloc);
            auto r = createLike(a, MType::LOGICAL, alloc);
            const uint8_t *src = a.logicalData();
            uint8_t *dst = r.logicalDataMut();
            for (size_t i = 0; i < a.numel(); ++i)
                dst[i] = src[i] ? 0 : 1;
            return r;
        }
        if (a.type() == MType::DOUBLE) {
            if (a.isScalar())
                return MValue::logicalScalar(a.toScalar() == 0.0, alloc);
            auto r = createLike(a, MType::LOGICAL, alloc);
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
        if (a.dims().is3D())
            throw std::runtime_error("transpose is not defined for N-D arrays");
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
        if (a.dims().is3D())
            throw std::runtime_error("transpose is not defined for N-D arrays");
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

} // namespace numkit::m
