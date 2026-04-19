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
            auto ca = createLike(a, MType::DOUBLE, alloc);
            const char *cd = a.charData();
            double *dd = ca.doubleDataMut();
            for (size_t i = 0; i < a.numel(); ++i)
                dd[i] = static_cast<double>(static_cast<unsigned char>(cd[i]));
            return elementwiseDouble(ca, b, std::plus<double>{}, alloc);
        }
        if (a.type() == MType::DOUBLE && b.isChar()) {
            auto cb = createLike(b, MType::DOUBLE, alloc);
            const char *cd = b.charData();
            double *dd = cb.doubleDataMut();
            for (size_t i = 0; i < b.numel(); ++i)
                dd[i] = static_cast<double>(static_cast<unsigned char>(cd[i]));
            return elementwiseDouble(a, cb, std::plus<double>{}, alloc);
        }
        // string + string → string concatenation (MATLAB "a" + "b" = "ab")
        if (a.isString() && b.isString())
            return MValue::stringScalar(a.toString() + b.toString(), alloc);
        // string + char or char + string → string
        if (a.isString() && b.isChar())
            return MValue::stringScalar(a.toString() + b.toString(), alloc);
        if (a.isChar() && b.isString())
            return MValue::stringScalar(a.toString() + b.toString(), alloc);
        // string + numeric → string (convert number to string)
        if (a.isString() && b.isNumeric())
            return MValue::stringScalar(a.toString() + std::to_string(b.toScalar()), alloc);
        if (a.isNumeric() && b.isString())
            return MValue::stringScalar(std::to_string(a.toScalar()) + b.toString(), alloc);
        { auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateAdd(x, y); }, alloc);
          if (!r.isUnset()) return r; }
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
        { auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateSub(x, y); }, alloc);
          if (!r.isUnset()) return r; }
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
        { auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateMul(x, y); }, alloc);
          if (!r.isUnset()) return r; }
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

        { auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateMul(x, y); }, alloc);
          if (!r.isUnset()) return r; }
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
        { auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateDiv(x, y); }, alloc);
          if (!r.isUnset()) return r; }
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
        { auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateDiv(x, y); }, alloc);
          if (!r.isUnset()) return r; }
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
        { auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) -> decltype(x) {
            double r = std::pow(static_cast<double>(x), static_cast<double>(y));
            if constexpr (std::is_integral_v<decltype(x)>) {
                r = std::round(r);
                if (r > static_cast<double>(std::numeric_limits<decltype(x)>::max())) return std::numeric_limits<decltype(x)>::max();
                if (r < static_cast<double>(std::numeric_limits<decltype(x)>::min())) return std::numeric_limits<decltype(x)>::min();
                return static_cast<decltype(x)>(r);
            } else { return static_cast<decltype(x)>(r); }
          }, alloc);
          if (!r.isUnset()) return r; }
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

            // String comparison (MATLAB string type)
            if (a.isString() || b.isString()) {
                auto toStr = [](const MValue &v) -> std::string {
                    if (v.isString() || v.isChar())
                        return v.toString();
                    throw std::runtime_error(
                        "Comparison between string and non-string is not supported");
                };
                std::string sa = toStr(a), sb = toStr(b);
                if (op == "==")
                    return MValue::logicalScalar(sa == sb, nullptr);
                if (op == "~=")
                    return MValue::logicalScalar(sa != sb, nullptr);
                if (op == "<")
                    return MValue::logicalScalar(sa < sb, nullptr);
                if (op == ">")
                    return MValue::logicalScalar(sa > sb, nullptr);
                if (op == "<=")
                    return MValue::logicalScalar(sa <= sb, nullptr);
                if (op == ">=")
                    return MValue::logicalScalar(sa >= sb, nullptr);
            }

            // Complex comparison: only == and ~= are supported
            if (a.isComplex() || b.isComplex()) {
                if (op != "==" && op != "~=")
                    throw std::runtime_error(
                        "Operator '" + op
                        + "' is not supported for complex operands");
                bool isEq = (op == "==");
                auto ceq = [](Complex x, Complex y) {
                    return x.real() == y.real() && x.imag() == y.imag();
                };
                auto getC = [](const MValue &v, size_t i) -> Complex {
                    if (v.isComplex())
                        return v.complexData()[i];
                    return Complex(v.type() == MType::LOGICAL
                                       ? static_cast<double>(v.logicalData()[i])
                                       : v.doubleData()[i],
                                   0.0);
                };
                if (a.isScalar() && b.isScalar()) {
                    Complex ca = a.isComplex() ? a.toComplex()
                                               : Complex(a.toScalar(), 0.0);
                    Complex cb = b.isComplex() ? b.toComplex()
                                               : Complex(b.toScalar(), 0.0);
                    return MValue::logicalScalar(isEq ? ceq(ca, cb) : !ceq(ca, cb),
                                                 nullptr);
                }
                if (a.isScalar()) {
                    Complex ca = a.isComplex() ? a.toComplex()
                                               : Complex(a.toScalar(), 0.0);
                    auto r = createLike(b, MType::LOGICAL, nullptr);
                    for (size_t i = 0; i < b.numel(); ++i)
                        r.logicalDataMut()[i] =
                            (isEq ? ceq(ca, getC(b, i)) : !ceq(ca, getC(b, i)))
                                ? 1
                                : 0;
                    return r;
                }
                if (b.isScalar()) {
                    Complex cb = b.isComplex() ? b.toComplex()
                                               : Complex(b.toScalar(), 0.0);
                    auto r = createLike(a, MType::LOGICAL, nullptr);
                    for (size_t i = 0; i < a.numel(); ++i)
                        r.logicalDataMut()[i] =
                            (isEq ? ceq(getC(a, i), cb) : !ceq(getC(a, i), cb))
                                ? 1
                                : 0;
                    return r;
                }
                if (a.dims() != b.dims())
                    throw std::runtime_error(
                        "Matrix dimensions must agree for comparison");
                auto r = createLike(a, MType::LOGICAL, nullptr);
                for (size_t i = 0; i < a.numel(); ++i)
                    r.logicalDataMut()[i] =
                        (isEq ? ceq(getC(a, i), getC(b, i))
                              : !ceq(getC(a, i), getC(b, i)))
                            ? 1
                            : 0;
                return r;
            }

            // Numeric comparison (double, logical, integer, single — with broadcasting)
            auto getD = [](const MValue &v, size_t r, size_t c) -> double {
                size_t idx = c * v.dims().rows() + r;
                if (v.isLogical()) return static_cast<double>(v.logicalData()[idx]);
                if (isIntegerType(v.type()) || v.type() == MType::SINGLE) return v.toScalar(); // scalar only for now
                return v.doubleData()[idx];
            };
            auto getDScalar = [](const MValue &v) -> double {
                if (v.isLogical()) return v.toBool() ? 1.0 : 0.0;
                return v.toScalar();
            };

            if (a.isScalar() && b.isScalar())
                return MValue::logicalScalar(cmp(getDScalar(a), getDScalar(b)), nullptr);

            // 3D path — same-shape elementwise and scalar broadcasting.
            if (a.dims().is3D() || b.dims().is3D()) {
                auto elemD = [](const MValue &v, size_t i) -> double {
                    if (v.isLogical()) return v.logicalData()[i];
                    if (v.type() == MType::DOUBLE) return v.doubleData()[i];
                    return v.elemAsDouble(i);
                };
                if (a.isScalar()) {
                    auto r = createLike(b, MType::LOGICAL, nullptr);
                    double s = getDScalar(a);
                    for (size_t i = 0; i < b.numel(); ++i)
                        r.logicalDataMut()[i] = cmp(s, elemD(b, i)) ? 1 : 0;
                    return r;
                }
                if (b.isScalar()) {
                    auto r = createLike(a, MType::LOGICAL, nullptr);
                    double s = getDScalar(b);
                    for (size_t i = 0; i < a.numel(); ++i)
                        r.logicalDataMut()[i] = cmp(elemD(a, i), s) ? 1 : 0;
                    return r;
                }
                if (a.dims() != b.dims())
                    throw std::runtime_error(
                        "3D broadcasting not supported — dimensions must match");
                auto r = createLike(a, MType::LOGICAL, nullptr);
                for (size_t i = 0; i < a.numel(); ++i)
                    r.logicalDataMut()[i] =
                        cmp(elemD(a, i), elemD(b, i)) ? 1 : 0;
                return r;
            }

            size_t ar = a.dims().rows(), ac = a.dims().cols();
            size_t br = b.dims().rows(), bc = b.dims().cols();
            size_t outR, outC;
            if (!broadcastDims(ar, ac, br, bc, outR, outC))
                throw std::runtime_error("Matrix dimensions must agree for comparison");

            auto r = MValue::matrix(outR, outC, MType::LOGICAL, nullptr);
            uint8_t *dst = r.logicalDataMut();
            for (size_t c = 0; c < outC; ++c) {
                size_t ca = (ac == 1) ? 0 : c;
                size_t cb = (bc == 1) ? 0 : c;
                for (size_t row = 0; row < outR; ++row) {
                    size_t ra = (ar == 1) ? 0 : row;
                    size_t rb = (br == 1) ? 0 : row;
                    dst[c * outR + row] = cmp(getD(a, ra, ca), getD(b, rb, cb)) ? 1 : 0;
                }
            }
            return r;
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
            auto r = createLike(b, MType::LOGICAL, alloc);
            uint8_t *dst = r.logicalDataMut();
            for (size_t i = 0; i < bb.size(); ++i)
                dst[i] = (av && bb[i]) ? 1 : 0;
            return r;
        }
        if (b.isScalar()) {
            bool bv = b.toBool();
            auto aa = toBoolArray(a);
            auto r = createLike(a, MType::LOGICAL, alloc);
            uint8_t *dst = r.logicalDataMut();
            for (size_t i = 0; i < aa.size(); ++i)
                dst[i] = (aa[i] && bv) ? 1 : 0;
            return r;
        }
        if (a.numel() != b.numel())
            throw std::runtime_error("Matrix dimensions must agree for &");
        auto aa = toBoolArray(a);
        auto bb = toBoolArray(b);
        auto r = createLike(a, MType::LOGICAL, alloc);
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
            auto r = createLike(b, MType::LOGICAL, alloc);
            uint8_t *dst = r.logicalDataMut();
            for (size_t i = 0; i < bb.size(); ++i)
                dst[i] = (av || bb[i]) ? 1 : 0;
            return r;
        }
        if (b.isScalar()) {
            bool bv = b.toBool();
            auto aa = toBoolArray(a);
            auto r = createLike(a, MType::LOGICAL, alloc);
            uint8_t *dst = r.logicalDataMut();
            for (size_t i = 0; i < aa.size(); ++i)
                dst[i] = (aa[i] || bv) ? 1 : 0;
            return r;
        }
        if (a.numel() != b.numel())
            throw std::runtime_error("Matrix dimensions must agree for |");
        auto aa = toBoolArray(a);
        auto bb = toBoolArray(b);
        auto r = createLike(a, MType::LOGICAL, alloc);
        uint8_t *dst = r.logicalDataMut();
        for (size_t i = 0; i < aa.size(); ++i)
            dst[i] = (aa[i] || bb[i]) ? 1 : 0;
        return r;
    });
}

} // namespace mlab
