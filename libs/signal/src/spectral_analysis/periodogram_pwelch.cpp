// libs/signal/src/spectral_analysis/periodogram_pwelch.cpp
//
// periodogram + pwelch. Split from MDspSpectral. spectrogram lives in
// time_frequency/spectrogram.cpp.

#include <numkit/signal/spectral_analysis/periodogram_pwelch.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "../dsp_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace numkit::signal {

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

std::tuple<Value, Value>
periodogram(Allocator &alloc, const Value &x, const Value &window, size_t nfft)
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
    auto Pxx = Value::matrix(nOut, 1, ValueType::DOUBLE, &alloc);
    auto F = Value::matrix(nOut, 1, ValueType::DOUBLE, &alloc);
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

std::tuple<Value, Value>
pwelch(Allocator &alloc,
       const Value &x,
       const Value &window,
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
    auto Pxx = Value::matrix(nOut, 1, ValueType::DOUBLE, &alloc);
    auto F = Value::matrix(nOut, 1, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < nOut; ++i) {
        Pxx.doubleDataMut()[i] = psd[i] * scale;
        F.doubleDataMut()[i] = M_PI * i / (nOut - 1);
    }

    return std::make_tuple(std::move(Pxx), std::move(F));
}

namespace detail {

void periodogram_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("periodogram: requires at least 1 argument",
                     0, 0, "periodogram", "", "m:periodogram:nargin");

    Value window = Value::empty();
    if (args.size() >= 2 && !args[1].isChar())
        window = args[1];
    const size_t nfft = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;

    auto [Pxx, F] = periodogram(ctx.engine->allocator(), args[0], window, nfft);
    outs[0] = std::move(Pxx);
    if (nargout > 1)
        outs[1] = std::move(F);
}

void pwelch_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pwelch: requires at least 1 argument",
                     0, 0, "pwelch", "", "m:pwelch:nargin");

    Value window = Value::empty();
    if (args.size() >= 2 && !args[1].isChar())
        window = args[1];
    const size_t noverlap = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;
    const size_t nfft = (args.size() >= 4) ? static_cast<size_t>(args[3].toScalar()) : 0;

    auto [Pxx, F] = pwelch(ctx.engine->allocator(), args[0], window, noverlap, nfft);
    outs[0] = std::move(Pxx);
    if (nargout > 1)
        outs[1] = std::move(F);
}

} // namespace detail

} // namespace numkit::signal
