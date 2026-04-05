#include "MLabStdLibrary.hpp"
#include "MLabStdHelpers.hpp"

#include <cmath>
#include <functional>
#include <stdexcept>

namespace mlab {

void StdLibrary::registerBinaryOps(Engine &engine)
{
    // --- Addition ---
    engine.registerBinaryOp("+", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isEmpty() || b.isEmpty())
            return MValue::empty();
        if (a.isComplex() || b.isComplex())
            return elementwiseComplex(a, b, std::plus<Complex>{}, alloc);
        if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE)
            return elementwiseDouble(a, b, std::plus<double>{}, alloc);
        if (a.isChar() && b.isChar())
            return MValue::fromString(a.toString() + b.toString(), alloc);
        // char + double or double + char → double arithmetic (MATLAB behavior)
        if (a.isChar() && b.type() == MType::DOUBLE) {
            auto ca = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
            const char *cd = a.charData();
            double *dd = ca.doubleDataMut();
            for (size_t i = 0; i < a.numel(); ++i)
                dd[i] = static_cast<double>(static_cast<unsigned char>(cd[i]));
            return elementwiseDouble(ca, b, std::plus<double>{}, alloc);
        }
        if (a.type() == MType::DOUBLE && b.isChar()) {
            auto cb = MValue::matrix(b.dims().rows(), b.dims().cols(), MType::DOUBLE, alloc);
            const char *cd = b.charData();
            double *dd = cb.doubleDataMut();
            for (size_t i = 0; i < b.numel(); ++i)
                dd[i] = static_cast<double>(static_cast<unsigned char>(cd[i]));
            return elementwiseDouble(a, cb, std::plus<double>{}, alloc);
        }
        throw std::runtime_error("Unsupported types for +");
    });

    // --- Subtraction ---
    engine.registerBinaryOp("-", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isEmpty() || b.isEmpty())
            return MValue::empty();
        if (a.isComplex() || b.isComplex())
            return elementwiseComplex(a, b, std::minus<Complex>{}, alloc);
        if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE)
            return elementwiseDouble(a, b, std::minus<double>{}, alloc);
        throw std::runtime_error("Unsupported types for -");
    });

    // --- Element-wise multiply ---
    engine.registerBinaryOp(".*", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isEmpty() || b.isEmpty())
            return MValue::empty();
        if (a.isComplex() || b.isComplex())
            return elementwiseComplex(a, b, std::multiplies<Complex>{}, alloc);
        if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE)
            return elementwiseDouble(a, b, std::multiplies<double>{}, alloc);
        throw std::runtime_error("Unsupported types for .*");
    });

    // --- Matrix multiply ---
    engine.registerBinaryOp("*", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isEmpty() || b.isEmpty())
            return MValue::empty();

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
        if (a.isEmpty() || b.isEmpty())
            return MValue::empty();
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
        if (a.isEmpty() || b.isEmpty())
            return MValue::empty();
        if (a.isComplex() || b.isComplex())
            return elementwiseComplex(a, b, std::divides<Complex>{}, alloc);
        if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE)
            return elementwiseDouble(a, b, std::divides<double>{}, alloc);
        throw std::runtime_error("Unsupported types for ./");
    });

    // --- Power ---
    engine.registerBinaryOp("^", [&engine](const MValue &a, const MValue &b) -> MValue {
        auto *alloc = &engine.allocator();
        if (a.isEmpty() || b.isEmpty())
            return MValue::empty();
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
        if (a.isEmpty() || b.isEmpty())
            return MValue::empty();
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
        if (a.isEmpty() || b.isEmpty())
            return MValue::empty();
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

            if (a.isChar() && b.isChar()) {
                if (op == "==")
                    return MValue::logicalScalar(a.toString() == b.toString(), nullptr);
                if (op == "~=")
                    return MValue::logicalScalar(a.toString() != b.toString(), nullptr);
            }

            // Complex comparison: only == and ~= are supported
            if (a.isComplex() || b.isComplex()) {
                if (op != "==" && op != "~=")
                    throw std::runtime_error("Operator '" + op + "' is not supported for complex operands");
                auto toC = [](const MValue &v) -> Complex {
                    return v.isComplex() ? v.toComplex() : Complex(v.toScalar(), 0.0);
                };
                if (a.isScalar() && b.isScalar()) {
                    Complex ca = toC(a), cb = toC(b);
                    bool eq = (ca.real() == cb.real()) && (ca.imag() == cb.imag());
                    return MValue::logicalScalar(op == "==" ? eq : !eq, nullptr);
                }
                throw std::runtime_error("Complex array comparison not yet supported");
            }

            if (a.isScalar() && b.isScalar())
                return MValue::logicalScalar(cmp(a.toScalar(), b.toScalar()), nullptr);

            if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE) {
                if (a.isScalar()) {
                    auto r = MValue::matrix(b.dims().rows(), b.dims().cols(), MType::LOGICAL, nullptr);
                    double av = a.toScalar();
                    for (size_t i = 0; i < b.numel(); ++i)
                        r.logicalDataMut()[i] = cmp(av, b.doubleData()[i]) ? 1 : 0;
                    return r;
                }
                if (b.isScalar()) {
                    auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, nullptr);
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
        if (a.isScalar() && b.isScalar())
            return MValue::logicalScalar(a.toBool() && b.toBool(), alloc);
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

} // namespace mlab
