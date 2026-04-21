// libs/dsp/src/MDspSpectral.cpp

#include <numkit/m/dsp/MDspSpectral.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MDspHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace numkit::m::dsp {

namespace {

// Hamming window coefficients, same formula MATLAB uses.
std::vector<double> hammingWindow(size_t N)
{
    std::vector<double> w(N);
    if (N == 1) {
        w[0] = 1.0;
        return w;
    }
    for (size_t i = 0; i < N; ++i)
        w[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (N - 1));
    return w;
}

} // anonymous namespace

// ── periodogram ────────────────────────────────────────────────────────
std::tuple<MValue, MValue>
periodogram(Allocator &alloc, const MValue &x, const MValue &window, size_t nfft)
{
    const size_t N = x.numel();
    const double *xd = x.doubleData();

    std::vector<double> win(N, 1.0);
    if (window.numel() == N) {
        const double *w = window.doubleData();
        for (size_t i = 0; i < N; ++i)
            win[i] = w[i];
    }

    if (nfft == 0)
        nfft = nextPow2(N);

    std::vector<Complex> buf(nfft, Complex(0, 0));
    double winPower = 0.0;
    for (size_t i = 0; i < N; ++i) {
        buf[i] = Complex(xd[i] * win[i], 0.0);
        winPower += win[i] * win[i];
    }

    fftRadix2(buf, 1);

    const size_t nOut = nfft / 2 + 1;
    auto Pxx = MValue::matrix(nOut, 1, MType::DOUBLE, &alloc);
    auto F = MValue::matrix(nOut, 1, MType::DOUBLE, &alloc);
    const double scale = 1.0 / (winPower * nfft);

    for (size_t i = 0; i < nOut; ++i) {
        double mag2 = std::norm(buf[i]);
        if (i > 0 && i < nfft / 2)
            mag2 *= 2.0;
        Pxx.doubleDataMut()[i] = mag2 * scale;
        F.doubleDataMut()[i] = M_PI * i / (nOut - 1);
    }

    return std::make_tuple(std::move(Pxx), std::move(F));
}

// ── pwelch ─────────────────────────────────────────────────────────────
std::tuple<MValue, MValue>
pwelch(Allocator &alloc,
       const MValue &x,
       const MValue &window,
       size_t noverlap,
       size_t nfft)
{
    const size_t nx = x.numel();
    const double *xd = x.doubleData();

    size_t winLen;
    std::vector<double> win;
    if (window.numel() > 0) {
        winLen = window.numel();
        win.resize(winLen);
        const double *w = window.doubleData();
        for (size_t i = 0; i < winLen; ++i)
            win[i] = w[i];
    } else {
        winLen = std::min(nx, static_cast<size_t>(256));
        win = hammingWindow(winLen);
    }

    if (noverlap == 0)
        noverlap = winLen / 2;
    if (nfft == 0)
        nfft = nextPow2(winLen);

    double winPower = 0.0;
    for (size_t i = 0; i < winLen; ++i)
        winPower += win[i] * win[i];

    const size_t nOut = nfft / 2 + 1;
    std::vector<double> psd(nOut, 0.0);
    size_t nSegments = 0;
    const size_t step = winLen - noverlap;

    for (size_t start = 0; start + winLen <= nx; start += step) {
        std::vector<Complex> buf(nfft, Complex(0, 0));
        for (size_t i = 0; i < winLen; ++i)
            buf[i] = Complex(xd[start + i] * win[i], 0.0);

        fftRadix2(buf, 1);

        for (size_t i = 0; i < nOut; ++i) {
            double mag2 = std::norm(buf[i]);
            if (i > 0 && i < nfft / 2)
                mag2 *= 2.0;
            psd[i] += mag2;
        }
        nSegments++;
    }

    const double scale = 1.0 / (winPower * nfft * nSegments);
    auto Pxx = MValue::matrix(nOut, 1, MType::DOUBLE, &alloc);
    auto F = MValue::matrix(nOut, 1, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < nOut; ++i) {
        Pxx.doubleDataMut()[i] = psd[i] * scale;
        F.doubleDataMut()[i] = M_PI * i / (nOut - 1);
    }

    return std::make_tuple(std::move(Pxx), std::move(F));
}

// ── spectrogram ────────────────────────────────────────────────────────
std::tuple<MValue, MValue, MValue>
spectrogram(Allocator &alloc,
            const MValue &x,
            const MValue &window,
            size_t noverlap,
            size_t nfft)
{
    const size_t nx = x.numel();
    const double *xd = x.doubleData();

    size_t winLen;
    std::vector<double> win;
    if (window.numel() > 0) {
        winLen = window.numel();
        win.resize(winLen);
        const double *w = window.doubleData();
        for (size_t i = 0; i < winLen; ++i)
            win[i] = w[i];
    } else {
        winLen = std::min(nx, static_cast<size_t>(256));
        win = hammingWindow(winLen);
    }

    if (winLen > nx)
        winLen = nx;

    if (noverlap == 0)
        noverlap = winLen / 2;
    if (nfft == 0)
        nfft = nextPow2(winLen);

    const size_t nFreqs = nfft / 2 + 1;
    const size_t step = winLen - noverlap;

    size_t nSegments = 0;
    for (size_t start = 0; start + winLen <= nx; start += step)
        nSegments++;

    auto S = MValue::complexMatrix(nFreqs, nSegments, &alloc);
    auto F = MValue::matrix(nFreqs, 1, MType::DOUBLE, &alloc);
    auto T = MValue::matrix(1, nSegments, MType::DOUBLE, &alloc);

    size_t seg = 0;
    for (size_t start = 0; start + winLen <= nx; start += step) {
        std::vector<Complex> buf(nfft, Complex(0, 0));
        for (size_t i = 0; i < winLen; ++i)
            buf[i] = Complex(xd[start + i] * win[i], 0.0);

        fftRadix2(buf, 1);

        for (size_t i = 0; i < nFreqs; ++i)
            S.complexDataMut()[i + seg * nFreqs] = buf[i];

        T.doubleDataMut()[seg] = static_cast<double>(start + winLen / 2);
        seg++;
    }

    for (size_t i = 0; i < nFreqs; ++i)
        F.doubleDataMut()[i] = M_PI * i / (nFreqs - 1);

    return std::make_tuple(std::move(S), std::move(F), std::move(T));
}

// ── Engine adapters ────────────────────────────────────────────────────
namespace detail {

void periodogram_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("periodogram: requires at least 1 argument",
                     0, 0, "periodogram", "", "MATLAB:periodogram:nargin");

    // MATLAB semantics: ignore char-typed (flag) arg 1, and treat wrong-size as empty.
    MValue window = MValue::empty();
    if (args.size() >= 2 && !args[1].isChar())
        window = args[1];
    const size_t nfft = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;

    auto [Pxx, F] = periodogram(ctx.engine->allocator(), args[0], window, nfft);
    outs[0] = std::move(Pxx);
    if (nargout > 1)
        outs[1] = std::move(F);
}

void pwelch_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("pwelch: requires at least 1 argument",
                     0, 0, "pwelch", "", "MATLAB:pwelch:nargin");

    MValue window = MValue::empty();
    if (args.size() >= 2 && !args[1].isChar())
        window = args[1];
    const size_t noverlap = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;
    const size_t nfft = (args.size() >= 4) ? static_cast<size_t>(args[3].toScalar()) : 0;

    auto [Pxx, F] = pwelch(ctx.engine->allocator(), args[0], window, noverlap, nfft);
    outs[0] = std::move(Pxx);
    if (nargout > 1)
        outs[1] = std::move(F);
}

void spectrogram_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("spectrogram: requires at least 1 argument",
                     0, 0, "spectrogram", "", "MATLAB:spectrogram:nargin");

    // MATLAB's spectrogram(x, N) accepts a scalar-N (window length) and
    // builds a Hamming window of that length. If arg 1 is a vector, it's
    // the explicit window. Adapter handles both by synthesizing a Hamming
    // vector here when it sees a scalar.
    MValue window = MValue::empty();
    if (args.size() >= 2 && !args[1].isChar()) {
        if (args[1].numel() == 1) {
            const size_t winLen = static_cast<size_t>(args[1].toScalar());
            auto w = MValue::matrix(1, winLen, MType::DOUBLE, &ctx.engine->allocator());
            if (winLen == 1) {
                w.doubleDataMut()[0] = 1.0;
            } else {
                for (size_t i = 0; i < winLen; ++i)
                    w.doubleDataMut()[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (winLen - 1));
            }
            window = std::move(w);
        } else {
            window = args[1];
        }
    }
    const size_t noverlap = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;
    const size_t nfft = (args.size() >= 4) ? static_cast<size_t>(args[3].toScalar()) : 0;

    auto [S, F, T] = spectrogram(ctx.engine->allocator(), args[0], window, noverlap, nfft);
    outs[0] = std::move(S);
    if (nargout > 1)
        outs[1] = std::move(F);
    if (nargout > 2)
        outs[2] = std::move(T);
}

} // namespace detail

} // namespace numkit::m::dsp
