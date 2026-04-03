#include "MLabStdLibrary.hpp"

#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace mlab {

void StdLibrary::registerWindowFunctions(Engine &engine)
{
    // --- hamming(N) ---
    engine.registerFunction(
        "hamming", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            size_t N = static_cast<size_t>(args[0].toScalar());
            auto r = MValue::matrix(N, 1, MType::DOUBLE, alloc);
            for (size_t i = 0; i < N; ++i)
                r.doubleDataMut()[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (N - 1));
            {
                outs[0] = r;
                return;
            }
        });

    // --- hanning(N) / hann(N) ---
    auto hannFunc =
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            size_t N = static_cast<size_t>(args[0].toScalar());
            auto r = MValue::matrix(N, 1, MType::DOUBLE, alloc);
            for (size_t i = 0; i < N; ++i)
                r.doubleDataMut()[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (N - 1)));
            {
                outs[0] = r;
                return;
            }
        };
    engine.registerFunction("hanning", hannFunc);
    engine.registerFunction("hann", hannFunc);

    // --- blackman(N) ---
    engine.registerFunction("blackman",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                size_t N = static_cast<size_t>(args[0].toScalar());
                                auto r = MValue::matrix(N, 1, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < N; ++i) {
                                    double x = 2.0 * M_PI * i / (N - 1);
                                    r.doubleDataMut()[i] = 0.42 - 0.5 * std::cos(x)
                                                           + 0.08 * std::cos(2.0 * x);
                                }
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    // --- kaiser(N, beta) ---
    engine.registerFunction(
        "kaiser", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            size_t N = static_cast<size_t>(args[0].toScalar());
            double beta = (args.size() >= 2) ? args[1].toScalar() : 0.5;
            auto r = MValue::matrix(N, 1, MType::DOUBLE, alloc);
            // I0(x) approximation using series expansion
            auto besseli0 = [](double x) -> double {
                double sum = 1.0, term = 1.0;
                for (int k = 1; k <= 25; ++k) {
                    term *= (x / (2.0 * k)) * (x / (2.0 * k));
                    sum += term;
                    if (term < 1e-16 * sum)
                        break;
                }
                return sum;
            };
            double denom = besseli0(beta);
            for (size_t i = 0; i < N; ++i) {
                double alpha = 2.0 * i / (N - 1) - 1.0;
                r.doubleDataMut()[i] = besseli0(beta * std::sqrt(1.0 - alpha * alpha)) / denom;
            }
            {
                outs[0] = r;
                return;
            }
        });

    // --- rectwin(N) ---
    engine.registerFunction("rectwin",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                size_t N = static_cast<size_t>(args[0].toScalar());
                                auto r = MValue::matrix(N, 1, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < N; ++i)
                                    r.doubleDataMut()[i] = 1.0;
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    // --- bartlett(N) ---
    engine.registerFunction("bartlett",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                size_t N = static_cast<size_t>(args[0].toScalar());
                                auto r = MValue::matrix(N, 1, MType::DOUBLE, alloc);
                                double half = (N - 1) / 2.0;
                                for (size_t i = 0; i < N; ++i)
                                    r.doubleDataMut()[i] = 1.0 - std::abs((i - half) / half);
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });
}

} // namespace mlab