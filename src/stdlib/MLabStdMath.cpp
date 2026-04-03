#include "MLabStdHelpers.hpp"
#include "MLabStdLibrary.hpp"

#include <cmath>
#include <limits>
#include <random>

namespace mlab {

void StdLibrary::registerMathFunctions(Engine &engine)
{
    // --- sqrt ---
    engine.registerFunction(
        "sqrt", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isComplex()) {
                outs[0] = unaryComplex(a, [](const Complex &c) { return std::sqrt(c); }, alloc);
                return;
            }
            if (a.isScalar() && a.toScalar() < 0) {
                outs[0] = MValue::complexScalar(std::sqrt(Complex(a.toScalar(), 0.0)), alloc);
                return;
            }
            {
                outs[0] = unaryDouble(a, [](double x) { return std::sqrt(x); }, alloc);
                return;
            }
        });

    // --- abs ---
    engine.registerFunction(
        "abs", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isComplex()) {
                if (a.isScalar()) {
                    outs[0] = MValue::scalar(std::abs(a.toComplex()), alloc);
                    return;
                }
                auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
                for (size_t i = 0; i < a.numel(); ++i)
                    r.doubleDataMut()[i] = std::abs(a.complexData()[i]);
                {
                    outs[0] = r;
                    return;
                }
            }
            {
                outs[0] = unaryDouble(a, [](double x) { return std::abs(x); }, alloc);
                return;
            }
        });

    // --- Trig functions ---
    engine.registerFunction(
        "sin", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isComplex()) {
                outs[0] = unaryComplex(a, [](const Complex &c) { return std::sin(c); }, alloc);
                return;
            }
            {
                outs[0] = unaryDouble(a, [](double x) { return std::sin(x); }, alloc);
                return;
            }
        });

    engine.registerFunction(
        "cos", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isComplex()) {
                outs[0] = unaryComplex(a, [](const Complex &c) { return std::cos(c); }, alloc);
                return;
            }
            {
                outs[0] = unaryDouble(a, [](double x) { return std::cos(x); }, alloc);
                return;
            }
        });

    engine.registerFunction(
        "tan", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isComplex()) {
                outs[0] = unaryComplex(a, [](const Complex &c) { return std::tan(c); }, alloc);
                return;
            }
            {
                outs[0] = unaryDouble(a, [](double x) { return std::tan(x); }, alloc);
                return;
            }
        });

    engine.registerFunction(
        "asin", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isComplex()) {
                outs[0] = unaryComplex(a, [](const Complex &c) { return std::asin(c); }, alloc);
                return;
            }
            {
                outs[0] = unaryDouble(a, [](double x) { return std::asin(x); }, alloc);
                return;
            }
        });

    engine.registerFunction(
        "acos", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isComplex()) {
                outs[0] = unaryComplex(a, [](const Complex &c) { return std::acos(c); }, alloc);
                return;
            }
            {
                outs[0] = unaryDouble(a, [](double x) { return std::acos(x); }, alloc);
                return;
            }
        });

    engine.registerFunction(
        "atan", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isComplex()) {
                outs[0] = unaryComplex(a, [](const Complex &c) { return std::atan(c); }, alloc);
                return;
            }
            {
                outs[0] = unaryDouble(a, [](double x) { return std::atan(x); }, alloc);
                return;
            }
        });

    engine.registerFunction(
        "atan2", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.size() < 2)
                throw std::runtime_error("atan2 requires 2 arguments");
            {
                outs[0] = elementwiseDouble(
                    args[0], args[1], [](double y, double x) { return std::atan2(y, x); }, alloc);
                return;
            }
        });

    // --- exp / log ---
    engine.registerFunction(
        "exp", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isComplex()) {
                outs[0] = unaryComplex(a, [](const Complex &c) { return std::exp(c); }, alloc);
                return;
            }
            {
                outs[0] = unaryDouble(a, [](double x) { return std::exp(x); }, alloc);
                return;
            }
        });

    engine.registerFunction(
        "log", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isComplex()) {
                outs[0] = unaryComplex(a, [](const Complex &c) { return std::log(c); }, alloc);
                return;
            }
            if (a.isScalar() && a.toScalar() < 0) {
                outs[0] = MValue::complexScalar(std::log(Complex(a.toScalar(), 0.0)), alloc);
                return;
            }
            {
                outs[0] = unaryDouble(a, [](double x) { return std::log(x); }, alloc);
                return;
            }
        });

    engine.registerFunction(
        "log2", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            {
                outs[0] = unaryDouble(args[0], [](double x) { return std::log2(x); }, alloc);
                return;
            }
        });

    engine.registerFunction(
        "log10", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            {
                outs[0] = unaryDouble(args[0], [](double x) { return std::log10(x); }, alloc);
                return;
            }
        });

    // --- Rounding ---
    engine.registerFunction(
        "floor", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            {
                outs[0] = unaryDouble(args[0], [](double x) { return std::floor(x); }, alloc);
                return;
            }
        });

    engine.registerFunction(
        "ceil", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            {
                outs[0] = unaryDouble(args[0], [](double x) { return std::ceil(x); }, alloc);
                return;
            }
        });

    engine.registerFunction(
        "round", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            {
                outs[0] = unaryDouble(args[0], [](double x) { return std::round(x); }, alloc);
                return;
            }
        });

    engine.registerFunction(
        "fix", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            {
                outs[0] = unaryDouble(args[0], [](double x) { return std::trunc(x); }, alloc);
                return;
            }
        });

    // --- mod / rem ---
    engine.registerFunction("mod",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                {
                                    outs[0] = elementwiseDouble(
                                        args[0],
                                        args[1],
                                        [](double a, double b) {
                                            return b != 0 ? a - std::floor(a / b) * b : a;
                                        },
                                        alloc);
                                    return;
                                }
                            });

    engine.registerFunction(
        "rem", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            {
                outs[0] = elementwiseDouble(
                    args[0], args[1], [](double a, double b) { return std::fmod(a, b); }, alloc);
                return;
            }
        });

    // --- sign ---
    engine.registerFunction(
        "sign", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            {
                outs[0] = unaryDouble(
                    args[0], [](double x) { return (x > 0) ? 1.0 : (x < 0 ? -1.0 : 0.0); }, alloc);
                return;
            }
        });

    // --- max / min ---
    engine.registerFunction(
        "max", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.size() == 2) {
                outs[0] = elementwiseDouble(
                    args[0], args[1], [](double a, double b) { return std::max(a, b); }, alloc);
                return;
            }
            auto &a = args[0];
            if (a.dims().isVector() || a.isScalar()) {
                double mx = a.doubleData()[0];
                size_t mi = 0;
                for (size_t i = 1; i < a.numel(); ++i)
                    if (a.doubleData()[i] > mx) {
                        mx = a.doubleData()[i];
                        mi = i;
                    }
                {
                    outs[0] = MValue::scalar(mx, alloc);
                    if (nargout > 1)
                        outs[1] = MValue::scalar(static_cast<double>(mi + 1), alloc);
                    return;
                }
            }
            size_t R = a.dims().rows(), C = a.dims().cols();
            auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
            for (size_t c = 0; c < C; ++c) {
                double mx = a(0, c);
                for (size_t rr = 1; rr < R; ++rr)
                    mx = std::max(mx, a(rr, c));
                r.doubleDataMut()[c] = mx;
            }
            {
                outs[0] = r;
                return;
            }
        });

    engine.registerFunction(
        "min", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.size() == 2) {
                outs[0] = elementwiseDouble(
                    args[0], args[1], [](double a, double b) { return std::min(a, b); }, alloc);
                return;
            }
            auto &a = args[0];
            if (a.dims().isVector() || a.isScalar()) {
                double mn = a.doubleData()[0];
                size_t mi = 0;
                for (size_t i = 1; i < a.numel(); ++i)
                    if (a.doubleData()[i] < mn) {
                        mn = a.doubleData()[i];
                        mi = i;
                    }
                {
                    outs[0] = MValue::scalar(mn, alloc);
                    if (nargout > 1)
                        outs[1] = MValue::scalar(static_cast<double>(mi + 1), alloc);
                    return;
                }
            }
            size_t R = a.dims().rows(), C = a.dims().cols();
            auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
            for (size_t c = 0; c < C; ++c) {
                double mn = a(0, c);
                for (size_t rr = 1; rr < R; ++rr)
                    mn = std::min(mn, a(rr, c));
                r.doubleDataMut()[c] = mn;
            }
            {
                outs[0] = r;
                return;
            }
        });

    // --- Reductions: sum, prod, mean ---
    engine.registerFunction("sum",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                auto &a = args[0];
                                if (a.dims().isVector() || a.isScalar()) {
                                    double s = 0;
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        s += a.doubleData()[i];
                                    {
                                        outs[0] = MValue::scalar(s, alloc);
                                        return;
                                    }
                                }
                                size_t R = a.dims().rows(), C = a.dims().cols();
                                auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
                                for (size_t c = 0; c < C; ++c) {
                                    double s = 0;
                                    for (size_t rr = 0; rr < R; ++rr)
                                        s += a(rr, c);
                                    r.doubleDataMut()[c] = s;
                                }
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    engine.registerFunction("prod",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                auto &a = args[0];
                                if (a.dims().isVector() || a.isScalar()) {
                                    double p = 1;
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        p *= a.doubleData()[i];
                                    {
                                        outs[0] = MValue::scalar(p, alloc);
                                        return;
                                    }
                                }
                                size_t R = a.dims().rows(), C = a.dims().cols();
                                auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
                                for (size_t c = 0; c < C; ++c) {
                                    double p = 1;
                                    for (size_t rr = 0; rr < R; ++rr)
                                        p *= a(rr, c);
                                    r.doubleDataMut()[c] = p;
                                }
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    engine.registerFunction("mean",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                auto &a = args[0];
                                if (a.dims().isVector() || a.isScalar()) {
                                    double s = 0;
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        s += a.doubleData()[i];
                                    {
                                        outs[0] = MValue::scalar(s / static_cast<double>(a.numel()),
                                                                 alloc);
                                        return;
                                    }
                                }
                                size_t R = a.dims().rows(), C = a.dims().cols();
                                auto r = MValue::matrix(1, C, MType::DOUBLE, alloc);
                                for (size_t c = 0; c < C; ++c) {
                                    double s = 0;
                                    for (size_t rr = 0; rr < R; ++rr)
                                        s += a(rr, c);
                                    r.doubleDataMut()[c] = s / static_cast<double>(R);
                                }
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    // --- linspace / logspace ---
    engine.registerFunction("linspace",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
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
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    engine.registerFunction("logspace",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error(
                                        "logspace requires at least 2 arguments");
                                double a = args[0].toScalar();
                                double b = args[1].toScalar();
                                size_t n = args.size() >= 3
                                               ? static_cast<size_t>(args[2].toScalar())
                                               : 50;
                                auto r = MValue::matrix(1, n, MType::DOUBLE, alloc);
                                if (n == 1) {
                                    r.doubleDataMut()[0] = std::pow(10.0, b);
                                } else {
                                    for (size_t i = 0; i < n; ++i) {
                                        double exponent = a
                                                          + (b - a) * static_cast<double>(i)
                                                                / static_cast<double>(n - 1);
                                        r.doubleDataMut()[i] = std::pow(10.0, exponent);
                                    }
                                }
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    // --- rand / randn ---
    engine.registerFunction("rand",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
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
                                {
                                    outs[0] = m;
                                    return;
                                }
                            });

    engine.registerFunction("randn",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
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
                                {
                                    outs[0] = m;
                                    return;
                                }
                            });

    // --- Angle conversions ---
    engine.registerFunction(
        "deg2rad", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            {
                outs[0] = unaryDouble(
                    args[0], [](double x) { return x * (3.14159265358979323846 / 180.0); }, alloc);
                return;
            }
        });

    engine.registerFunction(
        "rad2deg", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            {
                outs[0] = unaryDouble(
                    args[0], [](double x) { return x * (180.0 / 3.14159265358979323846); }, alloc);
                return;
            }
        });
}

} // namespace mlab