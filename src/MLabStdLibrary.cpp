#include "MLabStdLibrary.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>

namespace mlab {

// ============================================================
// Helper: promote pair to complex if needed
// ============================================================
static std::pair<MValue, MValue> promoteToComplex(const MValue &a, const MValue &b, Allocator *alloc)
{
    MValue ca = a, cb = b;
    if (a.isComplex() && !b.isComplex())
        cb.promoteToComplex(alloc);
    else if (!a.isComplex() && b.isComplex())
        ca.promoteToComplex(alloc);
    return {ca, cb};
}

// ============================================================
// Helper: elementwise binary op on double arrays
// ============================================================
template<typename Op>
static MValue elementwiseDouble(const MValue &a, const MValue &b, Op op, Allocator *alloc)
{
    if (a.isScalar() && b.isScalar())
        return MValue::scalar(op(a.toScalar(), b.toScalar()), alloc);
    if (a.isScalar()) {
        auto r = MValue::matrix(b.dims().rows(), b.dims().cols(), MType::DOUBLE, alloc);
        double av = a.toScalar();
        for (size_t i = 0; i < b.numel(); ++i)
            r.doubleDataMut()[i] = op(av, b.doubleData()[i]);
        return r;
    }
    if (b.isScalar()) {
        auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
        double bv = b.toScalar();
        for (size_t i = 0; i < a.numel(); ++i)
            r.doubleDataMut()[i] = op(a.doubleData()[i], bv);
        return r;
    }
    if (a.dims() != b.dims())
        throw std::runtime_error("Matrix dimensions must agree");
    auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
    for (size_t i = 0; i < a.numel(); ++i)
        r.doubleDataMut()[i] = op(a.doubleData()[i], b.doubleData()[i]);
    return r;
}

// ============================================================
// Helper: elementwise binary op on complex arrays
// ============================================================
template<typename Op>
static MValue elementwiseComplex(const MValue &a, const MValue &b, Op op, Allocator *alloc)
{
    auto [ca, cb] = promoteToComplex(a, b, alloc);
    if (ca.isScalar() && cb.isScalar())
        return MValue::complexScalar(op(ca.toComplex(), cb.toComplex()), alloc);
    if (ca.isScalar()) {
        auto r = MValue::complexMatrix(cb.dims().rows(), cb.dims().cols(), alloc);
        Complex av = ca.toComplex();
        for (size_t i = 0; i < cb.numel(); ++i)
            r.complexDataMut()[i] = op(av, cb.complexData()[i]);
        return r;
    }
    if (cb.isScalar()) {
        auto r = MValue::complexMatrix(ca.dims().rows(), ca.dims().cols(), alloc);
        Complex bv = cb.toComplex();
        for (size_t i = 0; i < ca.numel(); ++i)
            r.complexDataMut()[i] = op(ca.complexData()[i], bv);
        return r;
    }
    if (ca.dims() != cb.dims())
        throw std::runtime_error("Matrix dimensions must agree");
    auto r = MValue::complexMatrix(ca.dims().rows(), ca.dims().cols(), alloc);
    for (size_t i = 0; i < ca.numel(); ++i)
        r.complexDataMut()[i] = op(ca.complexData()[i], cb.complexData()[i]);
    return r;
}

// ============================================================
// Helper: elementwise unary on double
// ============================================================
template<typename Op>
static MValue unaryDouble(const MValue &a, Op op, Allocator *alloc)
{
    if (a.isScalar())
        return MValue::scalar(op(a.toScalar()), alloc);
    auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
    for (size_t i = 0; i < a.numel(); ++i)
        r.doubleDataMut()[i] = op(a.doubleData()[i]);
    return r;
}

// ============================================================
// Helper: elementwise unary on complex
// ============================================================
template<typename Op>
static MValue unaryComplex(const MValue &a, Op op, Allocator *alloc)
{
    if (a.isScalar())
        return MValue::complexScalar(op(a.toComplex()), alloc);
    auto r = MValue::complexMatrix(a.dims().rows(), a.dims().cols(), alloc);
    for (size_t i = 0; i < a.numel(); ++i)
        r.complexDataMut()[i] = op(a.complexData()[i]);
    return r;
}

// ============================================================
// Install all
// ============================================================
void StdLibrary::install(Engine &engine)
{
    registerBinaryOps(engine);
    registerUnaryOps(engine);
    registerMathFunctions(engine);
    registerMatrixFunctions(engine);
    registerIOFunctions(engine);
    registerTypeFunctions(engine);
    registerCellStructFunctions(engine);
    registerStringFunctions(engine);
    registerComplexFunctions(engine);
}

// ============================================================
// Binary operators
// ============================================================
void StdLibrary::registerBinaryOps(Engine &engine)
{
    // --- Addition ---
    engine.registerBinaryOp("+", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isComplex() || b.isComplex())
            return elementwiseComplex(a, b, std::plus<Complex>{}, alloc);
        if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE)
            return elementwiseDouble(a, b, std::plus<double>{}, alloc);
        if (a.isChar() && b.isChar())
            return MValue::fromString(a.toString() + b.toString(), alloc);
        throw std::runtime_error("Unsupported types for +");
    });

    // --- Subtraction ---
    engine.registerBinaryOp("-", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isComplex() || b.isComplex())
            return elementwiseComplex(a, b, std::minus<Complex>{}, alloc);
        if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE)
            return elementwiseDouble(a, b, std::minus<double>{}, alloc);
        throw std::runtime_error("Unsupported types for -");
    });

    // --- Element-wise multiply ---
    engine.registerBinaryOp(".*", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isComplex() || b.isComplex())
            return elementwiseComplex(a, b, std::multiplies<Complex>{}, alloc);
        if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE)
            return elementwiseDouble(a, b, std::multiplies<double>{}, alloc);
        throw std::runtime_error("Unsupported types for .*");
    });

    // --- Matrix multiply ---
    engine.registerBinaryOp("*", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();

        // Complex matrix multiply
        if (a.isComplex() || b.isComplex()) {
            auto [ca, cb] = promoteToComplex(a, b, alloc);
            if (ca.isScalar() || cb.isScalar())
                return elementwiseComplex(a, b, std::multiplies<Complex>{}, alloc);
            size_t M = ca.dims().rows(), K = ca.dims().cols(), N = cb.dims().cols();
            if (K != cb.dims().rows())
                throw std::runtime_error("Inner matrix dimensions must agree");
            auto r = MValue::complexMatrix(M, N, alloc);
            for (size_t i = 0; i < M; ++i)
                for (size_t j = 0; j < N; ++j) {
                    Complex s(0, 0);
                    for (size_t k = 0; k < K; ++k)
                        s += ca.complexElem(i, k) * cb.complexElem(k, j);
                    r.complexDataMut()[j * M + i] = s;
                }
            return r;
        }

        // Double matrix multiply
        if (a.isScalar() || b.isScalar())
            return elementwiseDouble(a, b, std::multiplies<double>{}, alloc);
        if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE) {
            size_t M = a.dims().rows(), K = a.dims().cols(), N = b.dims().cols();
            if (K != b.dims().rows())
                throw std::runtime_error("Inner matrix dimensions must agree");
            auto r = MValue::matrix(M, N, MType::DOUBLE, alloc);
            for (size_t i = 0; i < M; ++i)
                for (size_t j = 0; j < N; ++j) {
                    double s = 0;
                    for (size_t k = 0; k < K; ++k)
                        s += a(i, k) * b(k, j);
                    r.elem(i, j) = s;
                }
            return r;
        }
        throw std::runtime_error("Unsupported types for *");
    });

    // --- Division ---
    engine.registerBinaryOp("/", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isComplex() || b.isComplex())
            return elementwiseComplex(a, b, std::divides<Complex>{}, alloc);
        if (a.type() == MType::DOUBLE && b.isScalar())
            return elementwiseDouble(a, b, std::divides<double>{}, alloc);
        if (a.isScalar() && b.isScalar())
            return MValue::scalar(a.toScalar() / b.toScalar(), alloc);
        throw std::runtime_error("Matrix right division not implemented");
    });

    // --- Element-wise division ---
    engine.registerBinaryOp("./", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isComplex() || b.isComplex())
            return elementwiseComplex(a, b, std::divides<Complex>{}, alloc);
        if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE)
            return elementwiseDouble(a, b, std::divides<double>{}, alloc);
        throw std::runtime_error("Unsupported types for ./");
    });

    // --- Power ---
    engine.registerBinaryOp("^", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isComplex() || b.isComplex()) {
            auto [ca, cb] = promoteToComplex(a, b, alloc);
            return MValue::complexScalar(std::pow(ca.toComplex(), cb.toComplex()), alloc);
        }
        if (a.isScalar() && b.isScalar())
            return MValue::scalar(std::pow(a.toScalar(), b.toScalar()), alloc);
        throw std::runtime_error("Matrix power not implemented");
    });

    // --- Element-wise power ---
    engine.registerBinaryOp(".^", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isComplex() || b.isComplex()) {
            return elementwiseComplex(
                a, b, [](const Complex &x, const Complex &y) { return std::pow(x, y); }, alloc);
        }
        if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE) {
            return elementwiseDouble(a, b, [](double x, double y) { return std::pow(x, y); }, alloc);
        }
        throw std::runtime_error("Unsupported types for .^");
    });

    // --- Backslash (left division) ---
    engine.registerBinaryOp("\\", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isScalar() && b.isScalar())
            return MValue::scalar(b.toScalar() / a.toScalar(), alloc);
        throw std::runtime_error("Matrix left division not implemented");
    });

    // --- Comparisons ---
    auto makeCmpOp = [](const std::string &op) -> BinaryOpFunc {
        return [op](const MValue &a, const MValue &b) -> MValue {
            auto cmp = [&](double x, double y) -> bool {
                if (op == "==")
                    return x == y;
                if (op == "~=")
                    return x != y;
                if (op == "<")
                    return x < y;
                if (op == ">")
                    return x > y;
                if (op == "<=")
                    return x <= y;
                if (op == ">=")
                    return x >= y;
                return false;
            };

            // String comparison for == and ~=
            if (a.isChar() && b.isChar()) {
                if (op == "==")
                    return MValue::logicalScalar(a.toString() == b.toString(), nullptr);
                if (op == "~=")
                    return MValue::logicalScalar(a.toString() != b.toString(), nullptr);
            }

            if (a.isScalar() && b.isScalar())
                return MValue::logicalScalar(cmp(a.toScalar(), b.toScalar()), nullptr);

            // Element-wise comparison for arrays
            if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE) {
                if (a.isScalar()) {
                    auto r = MValue::matrix(b.dims().rows(),
                                            b.dims().cols(),
                                            MType::LOGICAL,
                                            nullptr);
                    double av = a.toScalar();
                    for (size_t i = 0; i < b.numel(); ++i)
                        r.logicalDataMut()[i] = cmp(av, b.doubleData()[i]) ? 1 : 0;
                    return r;
                }
                if (b.isScalar()) {
                    auto r = MValue::matrix(a.dims().rows(),
                                            a.dims().cols(),
                                            MType::LOGICAL,
                                            nullptr);
                    double bv = b.toScalar();
                    for (size_t i = 0; i < a.numel(); ++i)
                        r.logicalDataMut()[i] = cmp(a.doubleData()[i], bv) ? 1 : 0;
                    return r;
                }
                if (a.dims() != b.dims())
                    throw std::runtime_error("Matrix dimensions must agree for comparison");
                auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, nullptr);
                for (size_t i = 0; i < a.numel(); ++i)
                    r.logicalDataMut()[i] = cmp(a.doubleData()[i], b.doubleData()[i]) ? 1 : 0;
                return r;
            }

            throw std::runtime_error("Unsupported types for " + op);
        };
    };

    engine.registerBinaryOp("==", makeCmpOp("=="));
    engine.registerBinaryOp("~=", makeCmpOp("~="));
    engine.registerBinaryOp("<", makeCmpOp("<"));
    engine.registerBinaryOp(">", makeCmpOp(">"));
    engine.registerBinaryOp("<=", makeCmpOp("<="));
    engine.registerBinaryOp(">=", makeCmpOp(">="));

    // --- Logical element-wise AND ---
    engine.registerBinaryOp("&", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        // scalar & scalar
        if (a.isScalar() && b.isScalar())
            return MValue::logicalScalar(a.toBool() && b.toBool(), alloc);
        // element-wise
        auto toBoolArray = [&](const MValue &v) -> std::vector<uint8_t> {
            std::vector<uint8_t> r(v.numel());
            if (v.isLogical()) {
                const uint8_t *d = v.logicalData();
                for (size_t i = 0; i < v.numel(); ++i)
                    r[i] = d[i] ? 1 : 0;
            } else if (v.type() == MType::DOUBLE) {
                const double *d = v.doubleData();
                for (size_t i = 0; i < v.numel(); ++i)
                    r[i] = (d[i] != 0.0) ? 1 : 0;
            } else {
                r[0] = v.toBool() ? 1 : 0;
            }
            return r;
        };
        if (a.isScalar()) {
            bool av = a.toBool();
            auto bb = toBoolArray(b);
            auto r = MValue::matrix(b.dims().rows(), b.dims().cols(), MType::LOGICAL, alloc);
            uint8_t *dst = r.logicalDataMut();
            for (size_t i = 0; i < bb.size(); ++i)
                dst[i] = (av && bb[i]) ? 1 : 0;
            return r;
        }
        if (b.isScalar()) {
            bool bv = b.toBool();
            auto aa = toBoolArray(a);
            auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, alloc);
            uint8_t *dst = r.logicalDataMut();
            for (size_t i = 0; i < aa.size(); ++i)
                dst[i] = (aa[i] && bv) ? 1 : 0;
            return r;
        }
        if (a.numel() != b.numel())
            throw std::runtime_error("Matrix dimensions must agree for &");
        auto aa = toBoolArray(a);
        auto bb = toBoolArray(b);
        auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, alloc);
        uint8_t *dst = r.logicalDataMut();
        for (size_t i = 0; i < aa.size(); ++i)
            dst[i] = (aa[i] && bb[i]) ? 1 : 0;
        return r;
    });

    // --- Logical element-wise OR ---
    engine.registerBinaryOp("|", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isScalar() && b.isScalar())
            return MValue::logicalScalar(a.toBool() || b.toBool(), alloc);
        auto toBoolArray = [&](const MValue &v) -> std::vector<uint8_t> {
            std::vector<uint8_t> r(v.numel());
            if (v.isLogical()) {
                const uint8_t *d = v.logicalData();
                for (size_t i = 0; i < v.numel(); ++i)
                    r[i] = d[i] ? 1 : 0;
            } else if (v.type() == MType::DOUBLE) {
                const double *d = v.doubleData();
                for (size_t i = 0; i < v.numel(); ++i)
                    r[i] = (d[i] != 0.0) ? 1 : 0;
            } else {
                r[0] = v.toBool() ? 1 : 0;
            }
            return r;
        };
        if (a.isScalar()) {
            bool av = a.toBool();
            auto bb = toBoolArray(b);
            auto r = MValue::matrix(b.dims().rows(), b.dims().cols(), MType::LOGICAL, alloc);
            uint8_t *dst = r.logicalDataMut();
            for (size_t i = 0; i < bb.size(); ++i)
                dst[i] = (av || bb[i]) ? 1 : 0;
            return r;
        }
        if (b.isScalar()) {
            bool bv = b.toBool();
            auto aa = toBoolArray(a);
            auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, alloc);
            uint8_t *dst = r.logicalDataMut();
            for (size_t i = 0; i < aa.size(); ++i)
                dst[i] = (aa[i] || bv) ? 1 : 0;
            return r;
        }
        if (a.numel() != b.numel())
            throw std::runtime_error("Matrix dimensions must agree for |");
        auto aa = toBoolArray(a);
        auto bb = toBoolArray(b);
        auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, alloc);
        uint8_t *dst = r.logicalDataMut();
        for (size_t i = 0; i < aa.size(); ++i)
            dst[i] = (aa[i] || bb[i]) ? 1 : 0;
        return r;
    });
}

// ============================================================
// Unary operators
// ============================================================
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

// ============================================================
// Math functions
// ============================================================
void StdLibrary::registerMathFunctions(Engine &engine)
{
    // --- sqrt ---
    engine.registerFunction("sqrt", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::sqrt(c); }, alloc)};
        // Check for negative → complex result
        if (a.isScalar() && a.toScalar() < 0)
            return {MValue::complexScalar(std::sqrt(Complex(a.toScalar(), 0.0)), alloc)};
        return {unaryDouble(a, [](double x) { return std::sqrt(x); }, alloc)};
    });

    // --- abs ---
    engine.registerFunction("abs", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex()) {
            // abs of complex returns double
            if (a.isScalar())
                return {MValue::scalar(std::abs(a.toComplex()), alloc)};
            auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
            for (size_t i = 0; i < a.numel(); ++i)
                r.doubleDataMut()[i] = std::abs(a.complexData()[i]);
            return {r};
        }
        return {unaryDouble(a, [](double x) { return std::abs(x); }, alloc)};
    });

    // --- sin ---
    engine.registerFunction("sin", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::sin(c); }, alloc)};
        return {unaryDouble(a, [](double x) { return std::sin(x); }, alloc)};
    });

    // --- cos ---
    engine.registerFunction("cos", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::cos(c); }, alloc)};
        return {unaryDouble(a, [](double x) { return std::cos(x); }, alloc)};
    });

    // --- tan ---
    engine.registerFunction("tan", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::tan(c); }, alloc)};
        return {unaryDouble(a, [](double x) { return std::tan(x); }, alloc)};
    });

    // --- exp ---
    engine.registerFunction("exp", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::exp(c); }, alloc)};
        return {unaryDouble(a, [](double x) { return std::exp(x); }, alloc)};
    });

    // --- log ---
    engine.registerFunction("log", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::log(c); }, alloc)};
        if (a.isScalar() && a.toScalar() < 0)
            return {MValue::complexScalar(std::log(Complex(a.toScalar(), 0.0)), alloc)};
        return {unaryDouble(a, [](double x) { return std::log(x); }, alloc)};
    });

    // --- log2 ---
    engine.registerFunction("log2", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        return {unaryDouble(args[0], [](double x) { return std::log2(x); }, alloc)};
    });

    // --- log10 ---
    engine
        .registerFunction("log10", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
            auto *alloc = &engine.allocator();
            return {unaryDouble(args[0], [](double x) { return std::log10(x); }, alloc)};
        });

    // --- floor ---
    engine
        .registerFunction("floor", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
            auto *alloc = &engine.allocator();
            return {unaryDouble(args[0], [](double x) { return std::floor(x); }, alloc)};
        });

    // --- ceil ---
    engine.registerFunction("ceil", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        return {unaryDouble(args[0], [](double x) { return std::ceil(x); }, alloc)};
    });

    // --- round ---
    engine
        .registerFunction("round", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
            auto *alloc = &engine.allocator();
            return {unaryDouble(args[0], [](double x) { return std::round(x); }, alloc)};
        });

    // --- fix (truncate toward zero) ---
    engine.registerFunction("fix", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        return {unaryDouble(args[0], [](double x) { return std::trunc(x); }, alloc)};
    });

    // --- mod ---
    engine.registerFunction("mod", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        return {elementwiseDouble(
            args[0],
            args[1],
            [](double a, double b) { return b != 0 ? a - std::floor(a / b) * b : a; },
            alloc)};
    });

    // --- rem ---
    engine.registerFunction("rem", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        return {elementwiseDouble(
            args[0], args[1], [](double a, double b) { return std::fmod(a, b); }, alloc)};
    });

    // --- sign ---
    engine.registerFunction("sign",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {unaryDouble(
                                    args[0],
                                    [](double x) { return (x > 0) ? 1.0 : (x < 0 ? -1.0 : 0.0); },
                                    alloc)};
                            });

    // --- max ---
    engine.registerFunction("max", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        if (args.size() == 2)
            return {elementwiseDouble(
                args[0], args[1], [](double a, double b) { return std::max(a, b); }, alloc)};
        auto &a = args[0];
        if (a.dims().isVector() || a.isScalar()) {
            double mx = a.doubleData()[0];
            size_t mi = 0;
            for (size_t i = 1; i < a.numel(); ++i)
                if (a.doubleData()[i] > mx) {
                    mx = a.doubleData()[i];
                    mi = i;
                }
            return {MValue::scalar(mx, alloc), MValue::scalar(static_cast<double>(mi + 1), alloc)};
        }
        // Column-wise max
        size_t R = a.dims().rows(), C = a.dims().cols();
        auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
        for (size_t c = 0; c < C; ++c) {
            double mx = a(0, c);
            for (size_t rr = 1; rr < R; ++rr)
                mx = std::max(mx, a(rr, c));
            r.doubleDataMut()[c] = mx;
        }
        return {r};
    });

    // --- min ---
    engine.registerFunction("min", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        if (args.size() == 2)
            return {elementwiseDouble(
                args[0], args[1], [](double a, double b) { return std::min(a, b); }, alloc)};
        auto &a = args[0];
        if (a.dims().isVector() || a.isScalar()) {
            double mn = a.doubleData()[0];
            size_t mi = 0;
            for (size_t i = 1; i < a.numel(); ++i)
                if (a.doubleData()[i] < mn) {
                    mn = a.doubleData()[i];
                    mi = i;
                }
            return {MValue::scalar(mn, alloc), MValue::scalar(static_cast<double>(mi + 1), alloc)};
        }
        size_t R = a.dims().rows(), C = a.dims().cols();
        auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
        for (size_t c = 0; c < C; ++c) {
            double mn = a(0, c);
            for (size_t rr = 1; rr < R; ++rr)
                mn = std::min(mn, a(rr, c));
            r.doubleDataMut()[c] = mn;
        }
        return {r};
    });

    // --- sum ---
    engine.registerFunction("sum",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.dims().isVector() || a.isScalar()) {
                                    double s = 0;
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        s += a.doubleData()[i];
                                    return {MValue::scalar(s, alloc)};
                                }
                                size_t R = a.dims().rows(), C = a.dims().cols();
                                auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
                                for (size_t c = 0; c < C; ++c) {
                                    double s = 0;
                                    for (size_t rr = 0; rr < R; ++rr)
                                        s += a(rr, c);
                                    r.doubleDataMut()[c] = s;
                                }
                                return {r};
                            });

    // --- prod ---
    engine.registerFunction("prod",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.dims().isVector() || a.isScalar()) {
                                    double p = 1;
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        p *= a.doubleData()[i];
                                    return {MValue::scalar(p, alloc)};
                                }
                                size_t R = a.dims().rows(), C = a.dims().cols();
                                auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
                                for (size_t c = 0; c < C; ++c) {
                                    double p = 1;
                                    for (size_t rr = 0; rr < R; ++rr)
                                        p *= a(rr, c);
                                    r.doubleDataMut()[c] = p;
                                }
                                return {r};
                            });

    // --- mean ---
    engine.registerFunction("mean",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.dims().isVector() || a.isScalar()) {
                                    double s = 0;
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        s += a.doubleData()[i];
                                    return {
                                        MValue::scalar(s / static_cast<double>(a.numel()), alloc)};
                                }
                                size_t R = a.dims().rows(), C = a.dims().cols();
                                auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
                                for (size_t c = 0; c < C; ++c) {
                                    double s = 0;
                                    for (size_t rr = 0; rr < R; ++rr)
                                        s += a(rr, c);
                                    r.doubleDataMut()[c] = s / static_cast<double>(R);
                                }
                                return {r};
                            });

    // --- linspace ---
    engine.registerFunction("linspace",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                double a = args[0].toScalar();
                                double b = args[1].toScalar();
                                size_t n = args.size() >= 3
                                               ? static_cast<size_t>(args[2].toScalar())
                                               : 100;
                                auto r = MValue::matrix(1, n, MType::DOUBLE, alloc);
                                if (n == 1) {
                                    r.doubleDataMut()[0] = b;
                                } else {
                                    for (size_t i = 0; i < n; ++i)
                                        r.doubleDataMut()[i] = a
                                                               + (b - a) * static_cast<double>(i)
                                                                     / static_cast<double>(n - 1);
                                }
                                return {r};
                            });

    // --- rand ---
    engine.registerFunction("rand",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                static std::mt19937 gen(std::random_device{}());
                                static std::uniform_real_distribution<double> dist(0.0, 1.0);
                                size_t r = args.empty() ? 1
                                                        : static_cast<size_t>(args[0].toScalar());
                                size_t c = args.size() >= 2
                                               ? static_cast<size_t>(args[1].toScalar())
                                               : r;
                                auto m = MValue::matrix(r, c, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < m.numel(); ++i)
                                    m.doubleDataMut()[i] = dist(gen);
                                return {m};
                            });

    // --- randn ---
    engine.registerFunction("randn",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                static std::mt19937 gen(std::random_device{}());
                                static std::normal_distribution<double> dist(0.0, 1.0);
                                size_t r = args.empty() ? 1
                                                        : static_cast<size_t>(args[0].toScalar());
                                size_t c = args.size() >= 2
                                               ? static_cast<size_t>(args[1].toScalar())
                                               : r;
                                auto m = MValue::matrix(r, c, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < m.numel(); ++i)
                                    m.doubleDataMut()[i] = dist(gen);
                                return {m};
                            });
}

// ============================================================
// Matrix functions
// ============================================================
void StdLibrary::registerMatrixFunctions(Engine &engine)
{
    // --- zeros ---
    engine.registerFunction("zeros",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                size_t r = static_cast<size_t>(args[0].toScalar());
                                size_t c = args.size() >= 2
                                               ? static_cast<size_t>(args[1].toScalar())
                                               : r;
                                return {MValue::matrix(r, c, MType::DOUBLE, alloc)};
                            });

    // --- ones ---
    engine.registerFunction("ones",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                size_t r = static_cast<size_t>(args[0].toScalar());
                                size_t c = args.size() >= 2
                                               ? static_cast<size_t>(args[1].toScalar())
                                               : r;
                                auto m = MValue::matrix(r, c, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < m.numel(); ++i)
                                    m.doubleDataMut()[i] = 1.0;
                                return {m};
                            });

    // --- eye ---
    engine.registerFunction("eye", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        size_t r = static_cast<size_t>(args[0].toScalar());
        size_t c = args.size() >= 2 ? static_cast<size_t>(args[1].toScalar()) : r;
        auto m = MValue::matrix(r, c, MType::DOUBLE, alloc);
        for (size_t i = 0; i < std::min(r, c); ++i)
            m.elem(i, i) = 1.0;
        return {m};
    });

    // --- size ---
    engine.registerFunction("size", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (args.size() >= 2) {
            int dim = static_cast<int>(args[1].toScalar());
            return {MValue::scalar(static_cast<double>(a.dims().dimSize(dim - 1)), alloc)};
        }
        auto r = MValue::matrix(1, 2, MType::DOUBLE, alloc);
        r.doubleDataMut()[0] = static_cast<double>(a.dims().rows());
        r.doubleDataMut()[1] = static_cast<double>(a.dims().cols());
        return {r};
    });

    // --- length ---
    engine.registerFunction("length",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                double len = static_cast<double>(
                                    std::max(args[0].dims().rows(), args[0].dims().cols()));
                                return {MValue::scalar(len, alloc)};
                            });

    // --- numel ---
    engine.registerFunction("numel",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {MValue::scalar(static_cast<double>(args[0].numel()), alloc)};
                            });

    // --- ndims ---
    engine.registerFunction("ndims",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {MValue::scalar(static_cast<double>(args[0].dims().ndims()),
                                                       alloc)};
                            });

    // --- reshape ---
    engine.registerFunction("reshape",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                size_t newR = static_cast<size_t>(args[1].toScalar());
                                size_t newC = static_cast<size_t>(args[2].toScalar());
                                if (newR * newC != a.numel())
                                    throw std::runtime_error(
                                        "Number of elements must not change in reshape");
                                auto r = MValue::matrix(newR, newC, a.type(), alloc);
                                if (a.rawBytes() > 0)
                                    std::memcpy(r.rawDataMut(), a.rawData(), a.rawBytes());
                                return {r};
                            });

    // --- transpose (function form) ---
    engine.registerFunction("transpose",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                size_t rows = a.dims().rows(), cols = a.dims().cols();
                                auto r = MValue::matrix(cols, rows, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < rows; ++i)
                                    for (size_t j = 0; j < cols; ++j)
                                        r.elem(j, i) = a(i, j);
                                return {r};
                            });

    // --- diag ---
    engine.registerFunction("diag",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.dims().isVector()) {
                                    // Vector → diagonal matrix
                                    size_t n = a.numel();
                                    auto r = MValue::matrix(n, n, MType::DOUBLE, alloc);
                                    for (size_t i = 0; i < n; ++i)
                                        r.elem(i, i) = a.doubleData()[i];
                                    return {r};
                                }
                                // Matrix → diagonal vector
                                size_t n = std::min(a.dims().rows(), a.dims().cols());
                                auto r = MValue::matrix(n, 1, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < n; ++i)
                                    r.doubleDataMut()[i] = a(i, i);
                                return {r};
                            });

    // --- sort ---
    engine.registerFunction("sort", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        std::vector<double> vals(a.doubleData(), a.doubleData() + a.numel());
        std::sort(vals.begin(), vals.end());
        bool isRow = a.dims().rows() == 1;
        auto r = isRow ? MValue::matrix(1, vals.size(), MType::DOUBLE, alloc)
                       : MValue::matrix(vals.size(), 1, MType::DOUBLE, alloc);
        std::memcpy(r.doubleDataMut(), vals.data(), vals.size() * sizeof(double));
        return {r};
    });

    // --- find ---
    engine.registerFunction("find",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                std::vector<double> indices;
                                if (a.isLogical()) {
                                    const uint8_t *ld = a.logicalData();
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        if (ld[i])
                                            indices.push_back(static_cast<double>(i + 1));
                                } else {
                                    const double *dd = a.doubleData();
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        if (dd[i] != 0.0)
                                            indices.push_back(static_cast<double>(i + 1));
                                }
                                auto r = MValue::matrix(1, indices.size(), MType::DOUBLE, alloc);
                                if (!indices.empty())
                                    std::memcpy(r.doubleDataMut(),
                                                indices.data(),
                                                indices.size() * sizeof(double));
                                return {r};
                            });

    // --- horzcat ---
    engine.registerFunction("horzcat",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.empty())
                                    return {MValue::empty()};
                                size_t rows = args[0].dims().rows();
                                size_t totalCols = 0;
                                for (auto &a : args) {
                                    if (a.dims().rows() != rows)
                                        throw std::runtime_error(
                                            "Dimensions must agree for horzcat");
                                    totalCols += a.dims().cols();
                                }
                                auto r = MValue::matrix(rows, totalCols, MType::DOUBLE, alloc);
                                size_t colOff = 0;
                                for (auto &a : args) {
                                    for (size_t c = 0; c < a.dims().cols(); ++c)
                                        for (size_t rr = 0; rr < rows; ++rr)
                                            r.elem(rr, colOff + c) = a(rr, c);
                                    colOff += a.dims().cols();
                                }
                                return {r};
                            });

    // --- vertcat ---
    engine.registerFunction("vertcat",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.empty())
                                    return {MValue::empty()};
                                size_t cols = args[0].dims().cols();
                                size_t totalRows = 0;
                                for (auto &a : args) {
                                    if (a.dims().cols() != cols)
                                        throw std::runtime_error(
                                            "Dimensions must agree for vertcat");
                                    totalRows += a.dims().rows();
                                }
                                auto r = MValue::matrix(totalRows, cols, MType::DOUBLE, alloc);
                                size_t rowOff = 0;
                                for (auto &a : args) {
                                    for (size_t c = 0; c < cols; ++c)
                                        for (size_t rr = 0; rr < a.dims().rows(); ++rr)
                                            r.elem(rowOff + rr, c) = a(rr, c);
                                    rowOff += a.dims().rows();
                                }
                                return {r};
                            });
}

// ============================================================
// I/O functions
// ============================================================
void StdLibrary::registerIOFunctions(Engine &engine)
{
    engine.registerFunction("disp", [](const std::vector<MValue> &args) -> std::vector<MValue> {
        for (auto &a : args) {
            if (a.isChar())
                std::cout << a.toString() << "\n";
            else if (a.isScalar())
                std::cout << a.toScalar() << "\n";
            else
                std::cout << a.debugString() << "\n";
        }
        return {};
    });

    engine.registerFunction("fprintf", [](const std::vector<MValue> &args) -> std::vector<MValue> {
        if (!args.empty() && args[0].isChar())
            std::cout << args[0].toString();
        return {};
    });

    engine.registerFunction("sprintf",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (!args.empty() && args[0].isChar())
                                    return {MValue::fromString(args[0].toString(), alloc)};
                                return {MValue::fromString("", alloc)};
                            });

    engine.registerFunction("error", [](const std::vector<MValue> &args) -> std::vector<MValue> {
        std::string msg = args.empty() ? "Error" : args[0].toString();
        throw std::runtime_error(msg);
    });

    engine.registerFunction("warning", [](const std::vector<MValue> &args) -> std::vector<MValue> {
        if (!args.empty() && args[0].isChar())
            std::cerr << "Warning: " << args[0].toString() << "\n";
        return {};
    });
}

// ============================================================
// Type functions
// ============================================================
void StdLibrary::registerTypeFunctions(Engine &engine)
{
    engine.registerFunction("double",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.type() == MType::DOUBLE)
                                    return {a};
                                if (a.isLogical())
                                    return {MValue::scalar(a.toBool() ? 1.0 : 0.0, alloc)};
                                if (a.isChar())
                                    return {MValue::scalar(static_cast<double>(
                                                               static_cast<unsigned char>(
                                                                   a.charData()[0])),
                                                           alloc)};
                                throw std::runtime_error("Cannot convert to double");
                            });

    engine.registerFunction("logical",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isLogical())
                                    return {a};
                                if (a.isScalar())
                                    return {MValue::logicalScalar(a.toScalar() != 0, alloc)};
                                // Array
                                auto r = MValue::matrix(a.dims().rows(),
                                                        a.dims().cols(),
                                                        MType::LOGICAL,
                                                        alloc);
                                for (size_t i = 0; i < a.numel(); ++i)
                                    r.logicalDataMut()[i] = a.doubleData()[i] != 0 ? 1 : 0;
                                return {r};
                            });

    engine.registerFunction("char",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isChar())
                                    return {a};
                                if (a.isScalar()) {
                                    char c = static_cast<char>(static_cast<int>(a.toScalar()));
                                    return {MValue::fromString(std::string(1, c), alloc)};
                                }
                                throw std::runtime_error("Cannot convert to char");
                            });

    engine.registerFunction("isnumeric",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {MValue::logicalScalar(args[0].isNumeric(), alloc)};
                            });

    engine.registerFunction("islogical",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {MValue::logicalScalar(args[0].isLogical(), alloc)};
                            });

    engine.registerFunction("ischar",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {MValue::logicalScalar(args[0].isChar(), alloc)};
                            });

    engine.registerFunction("iscell",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {MValue::logicalScalar(args[0].isCell(), alloc)};
                            });

    engine.registerFunction("isstruct",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {MValue::logicalScalar(args[0].isStruct(), alloc)};
                            });

    engine.registerFunction("isempty",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {MValue::logicalScalar(args[0].isEmpty(), alloc)};
                            });

    engine.registerFunction("isscalar",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {MValue::logicalScalar(args[0].isScalar(), alloc)};
                            });

    engine.registerFunction("isreal",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {MValue::logicalScalar(!args[0].isComplex(), alloc)};
                            });

    engine.registerFunction("isnan",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isScalar())
                                    return {MValue::logicalScalar(std::isnan(a.toScalar()), alloc)};
                                auto r = MValue::matrix(a.dims().rows(),
                                                        a.dims().cols(),
                                                        MType::LOGICAL,
                                                        alloc);
                                for (size_t i = 0; i < a.numel(); ++i)
                                    r.logicalDataMut()[i] = std::isnan(a.doubleData()[i]) ? 1 : 0;
                                return {r};
                            });

    engine.registerFunction("isinf",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isScalar())
                                    return {MValue::logicalScalar(std::isinf(a.toScalar()), alloc)};
                                auto r = MValue::matrix(a.dims().rows(),
                                                        a.dims().cols(),
                                                        MType::LOGICAL,
                                                        alloc);
                                for (size_t i = 0; i < a.numel(); ++i)
                                    r.logicalDataMut()[i] = std::isinf(a.doubleData()[i]) ? 1 : 0;
                                return {r};
                            });
}

// ============================================================
// Cell & Struct functions
// ============================================================
void StdLibrary::registerCellStructFunctions(Engine &engine)
{
    engine.registerFunction("struct", [](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto s = MValue::structure();
        for (size_t i = 0; i + 1 < args.size(); i += 2)
            s.field(args[i].toString()) = args[i + 1];
        return {s};
    });

    engine.registerFunction("fieldnames",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (!a.isStruct())
                                    throw std::runtime_error("fieldnames requires a struct");
                                auto &fields = a.structFields();
                                auto c = MValue::cell(fields.size(), 1);
                                size_t i = 0;
                                for (auto &[k, v] : fields)
                                    c.cellAt(i++) = MValue::fromString(k, alloc);
                                return {c};
                            });

    engine.registerFunction("isfield",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (!args[0].isStruct())
                                    return {MValue::logicalScalar(false, alloc)};
                                return {MValue::logicalScalar(args[0].hasField(args[1].toString()),
                                                              alloc)};
                            });

    engine.registerFunction("rmfield", [](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto s = args[0];
        if (!s.isStruct())
            throw std::runtime_error("rmfield requires a struct");
        s.structFields().erase(args[1].toString());
        return {s};
    });

    engine.registerFunction("cell", [](const std::vector<MValue> &args) -> std::vector<MValue> {
        size_t r = static_cast<size_t>(args[0].toScalar());
        size_t c = args.size() >= 2 ? static_cast<size_t>(args[1].toScalar()) : r;
        return {MValue::cell(r, c)};
    });
}

// ============================================================
// String functions
// ============================================================
void StdLibrary::registerStringFunctions(Engine &engine)
{
    engine.registerFunction("num2str",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                std::ostringstream os;
                                os << args[0].toScalar();
                                return {MValue::fromString(os.str(), alloc)};
                            });

    engine.registerFunction("str2num",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                try {
                                    return {MValue::scalar(std::stod(args[0].toString()), alloc)};
                                } catch (...) {
                                    return {MValue::empty()};
                                }
                            });

    engine.registerFunction("str2double",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                try {
                                    return {MValue::scalar(std::stod(args[0].toString()), alloc)};
                                } catch (...) {
                                    return {MValue::scalar(std::numeric_limits<double>::quiet_NaN(),
                                                           alloc)};
                                }
                            });

    engine.registerFunction("strcmp",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {
                                    MValue::logicalScalar(args[0].toString() == args[1].toString(),
                                                          alloc)};
                            });

    engine.registerFunction("strcmpi",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                std::string a = args[0].toString(), b = args[1].toString();
                                std::transform(a.begin(), a.end(), a.begin(), ::tolower);
                                std::transform(b.begin(), b.end(), b.begin(), ::tolower);
                                return {MValue::logicalScalar(a == b, alloc)};
                            });

    engine.registerFunction("upper",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                std::string s = args[0].toString();
                                std::transform(s.begin(), s.end(), s.begin(), ::toupper);
                                return {MValue::fromString(s, alloc)};
                            });

    engine.registerFunction("lower",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                std::string s = args[0].toString();
                                std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                                return {MValue::fromString(s, alloc)};
                            });

    engine.registerFunction("strtrim",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                std::string s = args[0].toString();
                                size_t start = s.find_first_not_of(" \t\r\n");
                                size_t end = s.find_last_not_of(" \t\r\n");
                                if (start == std::string::npos)
                                    return {MValue::fromString("", alloc)};
                                return {MValue::fromString(s.substr(start, end - start + 1), alloc)};
                            });

    engine.registerFunction("strsplit",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                std::string s = args[0].toString();
                                char delim = args.size() >= 2 ? args[1].toString()[0] : ' ';
                                std::vector<std::string> parts;
                                std::istringstream iss(s);
                                std::string token;
                                while (std::getline(iss, token, delim))
                                    if (!token.empty())
                                        parts.push_back(token);
                                auto c = MValue::cell(1, parts.size());
                                for (size_t i = 0; i < parts.size(); ++i)
                                    c.cellAt(i) = MValue::fromString(parts[i], alloc);
                                return {c};
                            });

    engine.registerFunction("strcat",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                std::string result;
                                for (auto &a : args)
                                    result += a.toString();
                                return {MValue::fromString(result, alloc)};
                            });
}

// ============================================================
// Complex functions
// ============================================================
void StdLibrary::registerComplexFunctions(Engine &engine)
{
    engine.registerFunction("real",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isComplex()) {
                                    if (a.isScalar())
                                        return {MValue::scalar(a.toComplex().real(), alloc)};
                                    auto r = MValue::matrix(a.dims().rows(),
                                                            a.dims().cols(),
                                                            MType::DOUBLE,
                                                            alloc);
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        r.doubleDataMut()[i] = a.complexData()[i].real();
                                    return {r};
                                }
                                return {a};
                            });

    engine.registerFunction("imag",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isComplex()) {
                                    if (a.isScalar())
                                        return {MValue::scalar(a.toComplex().imag(), alloc)};
                                    auto r = MValue::matrix(a.dims().rows(),
                                                            a.dims().cols(),
                                                            MType::DOUBLE,
                                                            alloc);
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        r.doubleDataMut()[i] = a.complexData()[i].imag();
                                    return {r};
                                }
                                return {MValue::scalar(0.0, alloc)};
                            });

    engine.registerFunction("conj", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::conj(c); }, alloc)};
        return {a};
    });

    engine.registerFunction("complex",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.size() == 1)
                                    return {MValue::complexScalar(args[0].toScalar(), 0.0, alloc)};
                                return {MValue::complexScalar(args[0].toScalar(),
                                                              args[1].toScalar(),
                                                              alloc)};
                            });

    engine
        .registerFunction("angle", [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
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
