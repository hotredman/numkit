// libs/signal/src/spectral_analysis/periodogram_pwelch.cpp
//
// periodogram + pwelch. Split from spectral_analysis/. spectrogram lives in
// time_frequency/spectrogram.cpp.

#include <numkit/signal/spectral_analysis/periodogram_pwelch.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch_arena.hpp>
#include <numkit/core/types.hpp>

#include "../dsp_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <memory_resource>

namespace numkit::signal {

namespace {

// Fill caller-provided buffer with Hamming window coefficients, same
// formula MATLAB uses. Buffer must have N elements; works with both
// std::vector and std::pmr::vector data().
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

std::tuple<Value, Value>
periodogram(Allocator &alloc, const Value &x, const Value &window, size_t nfft)
{
    const size_t N = x.numel();
    const double *xd = x.doubleData();

    ScratchArena scratch(alloc);
    auto win = scratch.vec<double>(N);
    if (window.numel() == N) {
        const double *w = window.doubleData();
        for (size_t i = 0; i < N; ++i)
            win[i] = w[i];
    } else {
        std::fill(win.begin(), win.end(), 1.0);
    }

    if (nfft == 0)
        nfft = nextPow2(N);

    auto buf = scratch.vec<Complex>(nfft);
    double winPower = 0.0;
    for (size_t i = 0; i < N; ++i) {
        buf[i] = Complex(xd[i] * win[i], 0.0);
        winPower += win[i] * win[i];
    }

    fftRadix2(scratch.resource(), buf, 1);

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

    ScratchArena scratch(alloc);

    size_t winLen;
    ScratchVec<double> win(scratch.resource());
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

    if (noverlap == 0)
        noverlap = winLen / 2;
    if (nfft == 0)
        nfft = nextPow2(winLen);

    double winPower = 0.0;
    for (size_t i = 0; i < winLen; ++i)
        winPower += win[i] * win[i];

    const size_t nOut = nfft / 2 + 1;
    auto psd = scratch.vec<double>(nOut);
    size_t nSegments = 0;
    const size_t step = winLen - noverlap;

    // Per-segment FFT buffer hoisted out of the loop: under arena
    // semantics a fresh `vec<Complex>(nfft)` per iteration would bump-
    // allocate without reuse, growing the arena footprint to
    // O(nSegments × nfft). Reusing one buffer keeps it at O(nfft).
    auto buf = scratch.vec<Complex>(nfft);

    for (size_t start = 0; start + winLen <= nx; start += step) {
        for (size_t i = 0; i < winLen; ++i)
            buf[i] = Complex(xd[start + i] * win[i], 0.0);
        for (size_t i = winLen; i < nfft; ++i)
            buf[i] = Complex(0.0, 0.0);

        fftRadix2(scratch.resource(), buf, 1);

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
