// libs/signal/src/time_frequency/spectrogram.cpp
//
// Short-time Fourier transform. Split from spectral_analysis/. The
// time-collapsed power-spectrum estimators periodogram / pwelch
// live in spectral_analysis/periodogram_pwelch.cpp.

#include <numkit/signal/time_frequency/spectrogram.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "../dsp_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <memory_resource>

namespace numkit::signal {

namespace {

// Fill caller-provided buffer with Hamming window coefficients, same
// formula MATLAB uses.
void fillHammingWindow(double *w, size_t N)
{
    if (N == 1) {
        w[0] = 1.0;
        return;
    }
    for (size_t i = 0; i < N; ++i)
        w[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (N - 1));
}

} // anonymous namespace

std::tuple<Value, Value, Value>
spectrogram(std::pmr::memory_resource *mr,
            const Value &x,
            const Value &window,
            size_t noverlap,
            size_t nfft)
{
    const size_t nx = x.numel();
    const double *xd = x.doubleData();

    ScratchArena scratch(mr);

    size_t winLen;
    ScratchVec<double> win(&scratch);
    if (window.numel() > 0) {
        winLen = window.numel();
        win.resize(winLen);
        const double *w = window.doubleData();
        for (size_t i = 0; i < winLen; ++i)
            win[i] = w[i];
    } else {
        winLen = std::min(nx, static_cast<size_t>(256));
        win.resize(winLen);
        fillHammingWindow(win.data(), winLen);
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

    auto S = Value::complexMatrix(nFreqs, nSegments, mr);
    auto F = Value::matrix(nFreqs, 1, ValueType::DOUBLE, mr);
    auto T = Value::matrix(1, nSegments, ValueType::DOUBLE, mr);

    // Per-segment FFT buffer hoisted: see pwelch for the rationale —
    // a fresh allocation per loop iteration would grow the arena to
    // O(nSegments × nfft) instead of O(nfft).
    auto buf = ScratchVec<Complex>(nfft, &scratch);

    size_t seg = 0;
    for (size_t start = 0; start + winLen <= nx; start += step) {
        for (size_t i = 0; i < winLen; ++i)
            buf[i] = Complex(xd[start + i] * win[i], 0.0);
        for (size_t i = winLen; i < nfft; ++i)
            buf[i] = Complex(0.0, 0.0);

        fftRadix2(&scratch, buf, 1);

        for (size_t i = 0; i < nFreqs; ++i)
            S.complexDataMut()[i + seg * nFreqs] = buf[i];

        T.doubleDataMut()[seg] = static_cast<double>(start + winLen / 2);
        seg++;
    }

    for (size_t i = 0; i < nFreqs; ++i)
        F.doubleDataMut()[i] = M_PI * i / (nFreqs - 1);

    return std::make_tuple(std::move(S), std::move(F), std::move(T));
}

namespace detail {

void spectrogram_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("spectrogram: requires at least 1 argument",
                     0, 0, "spectrogram", "", "m:spectrogram:nargin");

    // MATLAB's spectrogram(x, N) accepts a scalar-N (window length) and
    // builds a Hamming window of that length. If arg 1 is a vector, it's
    // the explicit window. Adapter handles both by synthesizing a Hamming
    // vector here when it sees a scalar.
    Value window = Value::empty();
    if (args.size() >= 2 && !args[1].isChar()) {
        if (args[1].numel() == 1) {
            const size_t winLen = static_cast<size_t>(args[1].toScalar());
            auto w = Value::matrix(1, winLen, ValueType::DOUBLE, ctx.engine->resource());
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

    auto [S, F, T] = spectrogram(ctx.engine->resource(), args[0], window, noverlap, nfft);
    outs[0] = std::move(S);
    if (nargout > 1)
        outs[1] = std::move(F);
    if (nargout > 2)
        outs[2] = std::move(T);
}

} // namespace detail

} // namespace numkit::signal
