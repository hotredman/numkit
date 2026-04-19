#include "MDspHelpers.hpp"
#include "MDspLibrary.hpp"

#include <cmath>
#include <vector>

namespace numkit::m {

void DspLibrary::registerSpectralFunctions(Engine &engine)
{
    // --- periodogram(x) / periodogram(x, window, nfft) ---
    engine.registerFunction("periodogram",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.empty())
                                    throw std::runtime_error(
                                        "periodogram requires at least 1 argument");

                                auto &xv = args[0];
                                size_t N = xv.numel();
                                const double *x = xv.doubleData();

                                // Optional window (default: rectwin)
                                std::vector<double> win(N, 1.0);
                                if (args.size() >= 2 && !args[1].isChar() && args[1].numel() == N) {
                                    const double *w = args[1].doubleData();
                                    for (size_t i = 0; i < N; ++i)
                                        win[i] = w[i];
                                }

                                size_t nfft = (args.size() >= 3)
                                                  ? static_cast<size_t>(args[2].toScalar())
                                                  : nextPow2(N);

                                std::vector<Complex> buf(nfft, Complex(0, 0));
                                double winPower = 0.0;
                                for (size_t i = 0; i < N; ++i) {
                                    buf[i] = Complex(x[i] * win[i], 0.0);
                                    winPower += win[i] * win[i];
                                }

                                fftRadix2(buf, 1);

                                size_t nOut = nfft / 2 + 1;
                                auto Pxx = MValue::matrix(nOut, 1, MType::DOUBLE, alloc);
                                auto F = MValue::matrix(nOut, 1, MType::DOUBLE, alloc);
                                double scale = 1.0 / (winPower * nfft);

                                for (size_t i = 0; i < nOut; ++i) {
                                    double mag2 = std::norm(buf[i]);
                                    if (i > 0 && i < nfft / 2)
                                        mag2 *= 2.0;
                                    Pxx.doubleDataMut()[i] = mag2 * scale;
                                    F.doubleDataMut()[i] = M_PI * i / (nOut - 1);
                                }
                                {
                                    outs[0] = Pxx;
                                    if (nargout > 1)
                                        outs[1] = F;
                                    return;
                                }
                            });

    // --- pwelch(x) / pwelch(x, window, noverlap, nfft) ---
    engine.registerFunction(
        "pwelch", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty())
                throw std::runtime_error("pwelch requires at least 1 argument");

            auto &xv = args[0];
            size_t nx = xv.numel();
            const double *x = xv.doubleData();

            size_t winLen = (args.size() >= 2 && !args[1].isChar())
                                ? args[1].numel()
                                : std::min(nx, static_cast<size_t>(256));
            size_t noverlap = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar())
                                                 : winLen / 2;
            size_t nfft = (args.size() >= 4) ? static_cast<size_t>(args[3].toScalar())
                                             : nextPow2(winLen);

            // Window: provided or default Hamming
            std::vector<double> win(winLen);
            if (args.size() >= 2 && !args[1].isChar() && args[1].numel() == winLen) {
                const double *w = args[1].doubleData();
                for (size_t i = 0; i < winLen; ++i)
                    win[i] = w[i];
            } else {
                for (size_t i = 0; i < winLen; ++i)
                    win[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (winLen - 1));
            }

            double winPower = 0.0;
            for (size_t i = 0; i < winLen; ++i)
                winPower += win[i] * win[i];

            size_t nOut = nfft / 2 + 1;
            std::vector<double> psd(nOut, 0.0);
            size_t nSegments = 0;
            size_t step = winLen - noverlap;

            for (size_t start = 0; start + winLen <= nx; start += step) {
                std::vector<Complex> buf(nfft, Complex(0, 0));
                for (size_t i = 0; i < winLen; ++i)
                    buf[i] = Complex(x[start + i] * win[i], 0.0);

                fftRadix2(buf, 1);

                for (size_t i = 0; i < nOut; ++i) {
                    double mag2 = std::norm(buf[i]);
                    if (i > 0 && i < nfft / 2)
                        mag2 *= 2.0;
                    psd[i] += mag2;
                }
                nSegments++;
            }

            double scale = 1.0 / (winPower * nfft * nSegments);
            auto Pxx = MValue::matrix(nOut, 1, MType::DOUBLE, alloc);
            auto F = MValue::matrix(nOut, 1, MType::DOUBLE, alloc);
            for (size_t i = 0; i < nOut; ++i) {
                Pxx.doubleDataMut()[i] = psd[i] * scale;
                F.doubleDataMut()[i] = M_PI * i / (nOut - 1);
            }
            {
                outs[0] = Pxx;
                if (nargout > 1)
                    outs[1] = F;
                return;
            }
        });

    // --- spectrogram(x, window, noverlap, nfft) ---
    engine.registerFunction(
        "spectrogram",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty())
                throw std::runtime_error("spectrogram requires at least 1 argument");

            auto &xv = args[0];
            size_t nx = xv.numel();
            const double *x = xv.doubleData();

            size_t winLen = 256;
            std::vector<double> win;

            if (args.size() >= 2 && !args[1].isChar()) {
                if (args[1].numel() == 1) {
                    winLen = static_cast<size_t>(args[1].toScalar());
                } else {
                    winLen = args[1].numel();
                    win.resize(winLen);
                    const double *w = args[1].doubleData();
                    for (size_t i = 0; i < winLen; ++i)
                        win[i] = w[i];
                }
            }
            if (winLen > nx)
                winLen = nx;

            if (win.empty()) {
                win.resize(winLen);
                for (size_t i = 0; i < winLen; ++i)
                    win[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (winLen - 1));
            }

            size_t noverlap = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar())
                                                 : winLen / 2;
            size_t nfft = (args.size() >= 4) ? static_cast<size_t>(args[3].toScalar())
                                             : nextPow2(winLen);

            size_t nFreqs = nfft / 2 + 1;
            size_t step = winLen - noverlap;

            size_t nSegments = 0;
            for (size_t start = 0; start + winLen <= nx; start += step)
                nSegments++;

            auto S = MValue::complexMatrix(nFreqs, nSegments, alloc);
            auto F = MValue::matrix(nFreqs, 1, MType::DOUBLE, alloc);
            auto T = MValue::matrix(1, nSegments, MType::DOUBLE, alloc);

            size_t seg = 0;
            for (size_t start = 0; start + winLen <= nx; start += step) {
                std::vector<Complex> buf(nfft, Complex(0, 0));
                for (size_t i = 0; i < winLen; ++i)
                    buf[i] = Complex(x[start + i] * win[i], 0.0);

                fftRadix2(buf, 1);

                for (size_t i = 0; i < nFreqs; ++i)
                    S.complexDataMut()[i + seg * nFreqs] = buf[i];

                T.doubleDataMut()[seg] = static_cast<double>(start + winLen / 2);
                seg++;
            }

            for (size_t i = 0; i < nFreqs; ++i)
                F.doubleDataMut()[i] = M_PI * i / (nFreqs - 1);

            {
                outs[0] = S;
                if (nargout > 1)
                    outs[1] = F;
                if (nargout > 2)
                    outs[2] = T;
                return;
            }
        });
}

} // namespace numkit::m