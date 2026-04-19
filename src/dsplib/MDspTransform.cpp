#include "MDspHelpers.hpp"
#include "MDspLibrary.hpp"
#include "MStdHelpers.hpp"

#include <cmath>

namespace numkit {

void DspLibrary::registerTransformFunctions(Engine &engine)
{
    // --- unwrap(phase) --- unwrap radian phase angles
    engine.registerFunction(
        "unwrap", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty())
                throw std::runtime_error("unwrap requires 1 argument");
            auto &pv = args[0];
            size_t n = pv.numel();
            const double *p = pv.doubleData();

            auto r = createLike(pv, MType::DOUBLE, alloc);
            double *out = r.doubleDataMut();
            out[0] = p[0];
            for (size_t i = 1; i < n; ++i) {
                double d = p[i] - p[i - 1];
                d = d - 2.0 * M_PI * std::round(d / (2.0 * M_PI));
                out[i] = out[i - 1] + d;
            }
            {
                outs[0] = r;
                return;
            }
        });

    // --- hilbert(x) --- analytic signal via FFT
    engine.registerFunction("hilbert",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.empty())
                                    throw std::runtime_error("hilbert requires 1 argument");
                                auto &xv = args[0];
                                size_t N = xv.numel();
                                size_t fftLen = nextPow2(N);

                                auto buf = prepareFFTBuffer(xv, N, fftLen);
                                fftRadix2(buf, 1);

                                // Zero negative frequencies, double positive
                                for (size_t i = 1; i < fftLen / 2; ++i)
                                    buf[i] *= 2.0;
                                for (size_t i = fftLen / 2 + 1; i < fftLen; ++i)
                                    buf[i] = Complex(0.0, 0.0);

                                // IFFT
                                for (auto &v : buf)
                                    v = std::conj(v);
                                fftRadix2(buf, 1);
                                double invN = 1.0 / static_cast<double>(fftLen);
                                for (auto &v : buf)
                                    v = std::conj(v) * invN;

                                {
                                    outs[0] = packComplexResult(buf, N, alloc);
                                    return;
                                }
                            });

    // --- envelope(x) --- amplitude envelope via Hilbert transform
    engine.registerFunction("envelope",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.empty())
                                    throw std::runtime_error("envelope requires 1 argument");
                                auto &xv = args[0];
                                size_t N = xv.numel();
                                size_t fftLen = nextPow2(N);

                                auto buf = prepareFFTBuffer(xv, N, fftLen);
                                fftRadix2(buf, 1);

                                for (size_t i = 1; i < fftLen / 2; ++i)
                                    buf[i] *= 2.0;
                                for (size_t i = fftLen / 2 + 1; i < fftLen; ++i)
                                    buf[i] = Complex(0.0, 0.0);

                                for (auto &v : buf)
                                    v = std::conj(v);
                                fftRadix2(buf, 1);
                                double invN = 1.0 / static_cast<double>(fftLen);
                                for (auto &v : buf)
                                    v = std::conj(v) * invN;

                                auto r = createLike(xv, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < N; ++i)
                                    r.doubleDataMut()[i] = std::abs(buf[i]);
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });
}

} // namespace numkit