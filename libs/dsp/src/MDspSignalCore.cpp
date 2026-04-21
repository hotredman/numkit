#include "MDspHelpers.hpp"
#include <numkit/m/dsp/MDspLibrary.hpp>
#include "MStdHelpers.hpp"

#include <cmath>

namespace numkit::m {

void DspLibrary::registerSignalCoreFunctions(Engine &engine)
{
    // --- nextpow2(n) ---
    engine.registerFunction("nextpow2",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                double n = args[0].toScalar();
                                if (n <= 0) {
                                    outs[0] = MValue::scalar(0.0, alloc);
                                    return;
                                }
                                {
                                    outs[0] = MValue::scalar(std::ceil(std::log2(n)), alloc);
                                    return;
                                }
                            });

    // --- fft(x), ifft(X) moved to MDspFft.cpp — registered below in MDspLibrary ---

    // --- fftshift(X) ---
    engine.registerFunction(
        "fftshift",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty())
                throw std::runtime_error("fftshift requires 1 argument");
            auto &x = args[0];
            size_t N = x.numel();
            size_t shift = N / 2;

            if (x.isComplex()) {
                auto r = createLike(x, MType::COMPLEX, alloc);
                const Complex *src = x.complexData();
                Complex *dst = r.complexDataMut();
                for (size_t i = 0; i < N; ++i)
                    dst[i] = src[(i + shift) % N];
                {
                    outs[0] = r;
                    return;
                }
            }
            auto r = createLike(x, MType::DOUBLE, alloc);
            const double *src = x.doubleData();
            double *dst = r.doubleDataMut();
            for (size_t i = 0; i < N; ++i)
                dst[i] = src[(i + shift) % N];
            {
                outs[0] = r;
                return;
            }
        });

    // --- ifftshift(X) ---
    engine.registerFunction(
        "ifftshift",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty())
                throw std::runtime_error("ifftshift requires 1 argument");
            auto &x = args[0];
            size_t N = x.numel();
            size_t shift = (N + 1) / 2;

            if (x.isComplex()) {
                auto r = createLike(x, MType::COMPLEX, alloc);
                const Complex *src = x.complexData();
                Complex *dst = r.complexDataMut();
                for (size_t i = 0; i < N; ++i)
                    dst[i] = src[(i + shift) % N];
                {
                    outs[0] = r;
                    return;
                }
            }
            auto r = createLike(x, MType::DOUBLE, alloc);
            const double *src = x.doubleData();
            double *dst = r.doubleDataMut();
            for (size_t i = 0; i < N; ++i)
                dst[i] = src[(i + shift) % N];
            {
                outs[0] = r;
                return;
            }
        });
}

} // namespace numkit::m