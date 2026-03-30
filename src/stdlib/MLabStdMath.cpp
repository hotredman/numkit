#include "MLabStdLibrary.hpp"
#include "MLabStdHelpers.hpp"

#include <cmath>
#include <limits>
#include <random>

namespace mlab {

void StdLibrary::registerMathFunctions(Engine &engine)
{
    // --- sqrt ---
    engine.registerFunction("sqrt", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::sqrt(c); }, alloc)};
        if (a.isScalar() && a.toScalar() < 0)
            return {MValue::complexScalar(std::sqrt(Complex(a.toScalar(), 0.0)), alloc)};
        return {unaryDouble(a, [](double x) { return std::sqrt(x); }, alloc)};
    });

    // --- abs ---
    engine.registerFunction("abs", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex()) {
            if (a.isScalar())
                return {MValue::scalar(std::abs(a.toComplex()), alloc)};
            auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
            for (size_t i = 0; i < a.numel(); ++i)
                r.doubleDataMut()[i] = std::abs(a.complexData()[i]);
            return {r};
        }
        return {unaryDouble(a, [](double x) { return std::abs(x); }, alloc)};
    });

    // --- Trig functions ---
    engine.registerFunction("sin", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::sin(c); }, alloc)};
        return {unaryDouble(a, [](double x) { return std::sin(x); }, alloc)};
    });

    engine.registerFunction("cos", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::cos(c); }, alloc)};
        return {unaryDouble(a, [](double x) { return std::cos(x); }, alloc)};
    });

    engine.registerFunction("tan", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::tan(c); }, alloc)};
        return {unaryDouble(a, [](double x) { return std::tan(x); }, alloc)};
    });

    engine.registerFunction("asin", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::asin(c); }, alloc)};
        return {unaryDouble(a, [](double x) { return std::asin(x); }, alloc)};
    });

    engine.registerFunction("acos", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::acos(c); }, alloc)};
        return {unaryDouble(a, [](double x) { return std::acos(x); }, alloc)};
    });

    engine.registerFunction("atan", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::atan(c); }, alloc)};
        return {unaryDouble(a, [](double x) { return std::atan(x); }, alloc)};
    });

    engine.registerFunction("atan2",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error("atan2 requires 2 arguments");
                                return {elementwiseDouble(
                                    args[0], args[1],
                                    [](double y, double x) { return std::atan2(y, x); }, alloc)};
                            });

    // --- exp / log ---
    engine.registerFunction("exp", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::exp(c); }, alloc)};
        return {unaryDouble(a, [](double x) { return std::exp(x); }, alloc)};
    });

    engine.registerFunction("log", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.isComplex())
            return {unaryComplex(a, [](const Complex &c) { return std::log(c); }, alloc)};
        if (a.isScalar() && a.toScalar() < 0)
            return {MValue::complexScalar(std::log(Complex(a.toScalar(), 0.0)), alloc)};
        return {unaryDouble(a, [](double x) { return std::log(x); }, alloc)};
    });

    engine.registerFunction("log2", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        return {unaryDouble(args[0], [](double x) { return std::log2(x); }, alloc)};
    });

    engine.registerFunction("log10", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        return {unaryDouble(args[0], [](double x) { return std::log10(x); }, alloc)};
    });

    // --- Rounding ---
    engine.registerFunction("floor", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        return {unaryDouble(args[0], [](double x) { return std::floor(x); }, alloc)};
    });

    engine.registerFunction("ceil", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        return {unaryDouble(args[0], [](double x) { return std::ceil(x); }, alloc)};
    });

    engine.registerFunction("round", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        return {unaryDouble(args[0], [](double x) { return std::round(x); }, alloc)};
    });

    engine.registerFunction("fix", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        return {unaryDouble(args[0], [](double x) { return std::trunc(x); }, alloc)};
    });

    // --- mod / rem ---
    engine.registerFunction("mod", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        return {elementwiseDouble(
            args[0], args[1],
            [](double a, double b) { return b != 0 ? a - std::floor(a / b) * b : a; }, alloc)};
    });

    engine.registerFunction("rem", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        return {elementwiseDouble(
            args[0], args[1], [](double a, double b) { return std::fmod(a, b); }, alloc)};
    });

    // --- sign ---
    engine.registerFunction("sign", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        return {unaryDouble(
            args[0], [](double x) { return (x > 0) ? 1.0 : (x < 0 ? -1.0 : 0.0); }, alloc)};
    });

    // --- max / min ---
    engine.registerFunction("max", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        if (args.size() == 2)
            return {elementwiseDouble(
                args[0], args[1], [](double a, double b) { return std::max(a, b); }, alloc)};
        auto &a = args[0];
        if (a.dims().isVector() || a.isScalar()) {
            double mx = a.doubleData()[0];
            size_t mi = 0;
            for (size_t i = 1; i < a.numel(); ++i)
                if (a.doubleData()[i] > mx) { mx = a.doubleData()[i]; mi = i; }
            return {MValue::scalar(mx, alloc), MValue::scalar(static_cast<double>(mi + 1), alloc)};
        }
        size_t R = a.dims().rows(), C = a.dims().cols();
        auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
        for (size_t c = 0; c < C; ++c) {
            double mx = a(0, c);
            for (size_t rr = 1; rr < R; ++rr) mx = std::max(mx, a(rr, c));
            r.doubleDataMut()[c] = mx;
        }
        return {r};
    });

    engine.registerFunction("min", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        if (args.size() == 2)
            return {elementwiseDouble(
                args[0], args[1], [](double a, double b) { return std::min(a, b); }, alloc)};
        auto &a = args[0];
        if (a.dims().isVector() || a.isScalar()) {
            double mn = a.doubleData()[0];
            size_t mi = 0;
            for (size_t i = 1; i < a.numel(); ++i)
                if (a.doubleData()[i] < mn) { mn = a.doubleData()[i]; mi = i; }
            return {MValue::scalar(mn, alloc), MValue::scalar(static_cast<double>(mi + 1), alloc)};
        }
        size_t R = a.dims().rows(), C = a.dims().cols();
        auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
        for (size_t c = 0; c < C; ++c) {
            double mn = a(0, c);
            for (size_t rr = 1; rr < R; ++rr) mn = std::min(mn, a(rr, c));
            r.doubleDataMut()[c] = mn;
        }
        return {r};
    });

    // --- Reductions: sum, prod, mean ---
    engine.registerFunction("sum", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.dims().isVector() || a.isScalar()) {
            double s = 0;
            for (size_t i = 0; i < a.numel(); ++i) s += a.doubleData()[i];
            return {MValue::scalar(s, alloc)};
        }
        size_t R = a.dims().rows(), C = a.dims().cols();
        auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
        for (size_t c = 0; c < C; ++c) {
            double s = 0;
            for (size_t rr = 0; rr < R; ++rr) s += a(rr, c);
            r.doubleDataMut()[c] = s;
        }
        return {r};
    });

    engine.registerFunction("prod", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.dims().isVector() || a.isScalar()) {
            double p = 1;
            for (size_t i = 0; i < a.numel(); ++i) p *= a.doubleData()[i];
            return {MValue::scalar(p, alloc)};
        }
        size_t R = a.dims().rows(), C = a.dims().cols();
        auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
        for (size_t c = 0; c < C; ++c) {
            double p = 1;
            for (size_t rr = 0; rr < R; ++rr) p *= a(rr, c);
            r.doubleDataMut()[c] = p;
        }
        return {r};
    });

    engine.registerFunction("mean", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (a.dims().isVector() || a.isScalar()) {
            double s = 0;
            for (size_t i = 0; i < a.numel(); ++i) s += a.doubleData()[i];
            return {MValue::scalar(s / static_cast<double>(a.numel()), alloc)};
        }
        size_t R = a.dims().rows(), C = a.dims().cols();
        auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
        for (size_t c = 0; c < C; ++c) {
            double s = 0;
            for (size_t rr = 0; rr < R; ++rr) s += a(rr, c);
            r.doubleDataMut()[c] = s / static_cast<double>(R);
        }
        return {r};
    });

    // --- linspace / logspace ---
    engine.registerFunction("linspace",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                double a = args[0].toScalar();
                                double b = args[1].toScalar();
                                size_t n = args.size() >= 3
                                               ? static_cast<size_t>(args[2].toScalar()) : 100;
                                auto r = MValue::matrix(1, n, MType::DOUBLE, alloc);
                                if (n == 1) {
                                    r.doubleDataMut()[0] = b;
                                } else {
                                    for (size_t i = 0; i < n; ++i)
                                        r.doubleDataMut()[i] = a + (b - a) * static_cast<double>(i)
                                                                   / static_cast<double>(n - 1);
                                }
                                return {r};
                            });

    engine.registerFunction("logspace",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error("logspace requires at least 2 arguments");
                                double a = args[0].toScalar();
                                double b = args[1].toScalar();
                                size_t n = args.size() >= 3
                                               ? static_cast<size_t>(args[2].toScalar()) : 50;
                                auto r = MValue::matrix(1, n, MType::DOUBLE, alloc);
                                if (n == 1) {
                                    r.doubleDataMut()[0] = std::pow(10.0, b);
                                } else {
                                    for (size_t i = 0; i < n; ++i) {
                                        double exponent = a + (b - a) * static_cast<double>(i)
                                                              / static_cast<double>(n - 1);
                                        r.doubleDataMut()[i] = std::pow(10.0, exponent);
                                    }
                                }
                                return {r};
                            });

    // --- rand / randn ---
    engine.registerFunction("rand",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                static std::mt19937 gen(std::random_device{}());
                                static std::uniform_real_distribution<double> dist(0.0, 1.0);
                                size_t r = args.empty() ? 1 : static_cast<size_t>(args[0].toScalar());
                                size_t c = args.size() >= 2 ? static_cast<size_t>(args[1].toScalar()) : r;
                                auto m = MValue::matrix(r, c, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < m.numel(); ++i)
                                    m.doubleDataMut()[i] = dist(gen);
                                return {m};
                            });

    engine.registerFunction("randn",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                static std::mt19937 gen(std::random_device{}());
                                static std::normal_distribution<double> dist(0.0, 1.0);
                                size_t r = args.empty() ? 1 : static_cast<size_t>(args[0].toScalar());
                                size_t c = args.size() >= 2 ? static_cast<size_t>(args[1].toScalar()) : r;
                                auto m = MValue::matrix(r, c, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < m.numel(); ++i)
                                    m.doubleDataMut()[i] = dist(gen);
                                return {m};
                            });

    // --- Angle conversions ---
    engine.registerFunction("deg2rad",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {unaryDouble(
                                    args[0],
                                    [](double x) { return x * (3.14159265358979323846 / 180.0); },
                                    alloc)};
                            });

    engine.registerFunction("rad2deg",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {unaryDouble(
                                    args[0],
                                    [](double x) { return x * (180.0 / 3.14159265358979323846); },
                                    alloc)};
                            });
}

} // namespace mlab
