// libs/signal/src/spectral_analysis/periodogram_pwelch.cpp
//
// periodogram + pwelch. Split from spectral_analysis/. spectrogram lives in
// time_frequency/spectrogram.cpp.

#include <numkit/signal/spectral_analysis/periodogram_pwelch.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

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
periodogram(const Value &x, const Value &window, size_t nfft, double fs, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    const double *xd = x.doubleData();

    ScratchArena scratch(mr);
    auto win = ScratchVec<double>(N, &scratch);
    if (window.numel() == N) {
        const double *w = window.doubleData();
        for (size_t i = 0; i < N; ++i)
            win[i] = w[i];
    } else {
        std::fill(win.begin(), win.end(), 1.0);
    }

    // MATLAB default NFFT is max(256, 2^nextpow2(N)) — not just 2^nextpow2(N).
    // (pwelch already uses this rule.)
    if (nfft == 0)
        nfft = std::max<size_t>(256, nextPow2(N));

    auto buf = ScratchVec<Complex>(nfft, &scratch);
    double winPower = 0.0;
    for (size_t i = 0; i < N; ++i) {
        buf[i] = Complex(xd[i] * win[i], 0.0);
        winPower += win[i] * win[i];
    }

    fftRadix2(&scratch, buf, 1);

    const size_t nOut = nfft / 2 + 1;
    auto Pxx = Value::matrix(nOut, 1, ValueType::DOUBLE, mr);
    auto F = Value::matrix(nOut, 1, ValueType::DOUBLE, mr);
    // MATLAB PSD scaling: Pxx[k] = |X[k]|^2 / (winPower * fs), with
    // one-sided doubling for interior bins. With default fs = 2*pi
    // this matches MATLAB periodogram(x) without explicit fs.
    const double scale = 1.0 / (winPower * fs);

    for (size_t i = 0; i < nOut; ++i) {
        double mag2 = std::norm(buf[i]);
        if (i > 0 && i < nfft / 2)
            mag2 *= 2.0;
        Pxx.doubleDataMut()[i] = mag2 * scale;
        // Frequency vector spans [0, fs/2] inclusive on a one-sided grid.
        F.doubleDataMut()[i] = (fs / 2.0) * static_cast<double>(i)
                               / static_cast<double>(nOut - 1);
    }

    return std::make_tuple(std::move(Pxx), std::move(F));
}

std::tuple<Value, Value>
pwelch(const Value &x, const Value &window, size_t noverlap, size_t nfft, double fs, std::pmr::memory_resource *mr)
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
        // MATLAB pwelch default: divide x into 8 segments with 50%
        // overlap. Solving:
        //   nx = (winLen - noverlap)*8 + noverlap = 8*winLen - 7*noverlap
        // with noverlap = floor(winLen/2) gives winLen = floor(nx / 4.5).
        winLen = std::max<size_t>(1,
            static_cast<size_t>(std::floor(static_cast<double>(nx) / 4.5)));
        if (winLen > nx) winLen = nx;
        win.resize(winLen);
        fillHammingWindow(win.data(), winLen);
    }

    if (noverlap == 0)
        noverlap = winLen / 2;
    if (nfft == 0)
        nfft = std::max<size_t>(256, nextPow2(winLen));

    double winPower = 0.0;
    for (size_t i = 0; i < winLen; ++i)
        winPower += win[i] * win[i];

    const size_t nOut = nfft / 2 + 1;
    auto psd = ScratchVec<double>(nOut, &scratch);
    size_t nSegments = 0;
    const size_t step = winLen - noverlap;

    // Per-segment FFT buffer hoisted out of the loop: under arena
    // semantics a fresh `vec<Complex>(nfft)` per iteration would bump-
    // allocate without reuse, growing the arena footprint to
    // O(nSegments × nfft). Reusing one buffer keeps it at O(nfft).
    auto buf = ScratchVec<Complex>(nfft, &scratch);

    for (size_t start = 0; start + winLen <= nx; start += step) {
        for (size_t i = 0; i < winLen; ++i)
            buf[i] = Complex(xd[start + i] * win[i], 0.0);
        for (size_t i = winLen; i < nfft; ++i)
            buf[i] = Complex(0.0, 0.0);

        fftRadix2(&scratch, buf, 1);

        for (size_t i = 0; i < nOut; ++i) {
            double mag2 = std::norm(buf[i]);
            if (i > 0 && i < nfft / 2)
                mag2 *= 2.0;
            psd[i] += mag2;
        }
        nSegments++;
    }

    // MATLAB Welch PSD scaling: Pxx[k] = mean_segments(|X|^2) /
    // (winPower * fs). With default fs = 2*pi this matches MATLAB's
    // pwelch(x) without explicit fs.
    const double scale = 1.0 / (winPower * fs * nSegments);
    auto Pxx = Value::matrix(nOut, 1, ValueType::DOUBLE, mr);
    auto F = Value::matrix(nOut, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < nOut; ++i) {
        Pxx.doubleDataMut()[i] = psd[i] * scale;
        F.doubleDataMut()[i] = (fs / 2.0) * static_cast<double>(i)
                               / static_cast<double>(nOut - 1);
    }

    return std::make_tuple(std::move(Pxx), std::move(F));
}

// ════════════════════════════════════════════════════════════════════
// cpsd / mscohere — cross spectra via Welch's method
// ════════════════════════════════════════════════════════════════════
//
// Common machinery: walk overlapping windowed segments of x and y,
// FFT each, accumulate three spectra:
//   Sxx[k] = Σ |X[k]|²
//   Syy[k] = Σ |Y[k]|²
//   Sxy[k] = Σ X[k]·conj(Y[k])
// `cpsd` returns Sxy normalised the same way pwelch normalises Pxx.
// `mscohere` returns |Sxy|² / (Sxx · Syy) — independent of any
// per-segment scaling, so the normalisation cancels out and the
// result is a real number in [0, 1].

namespace {

struct CrossWelchOut {
    std::vector<std::complex<double>> Sxy;
    std::vector<double> Sxx, Syy;
    std::vector<double> F;
    double winPower = 0.0;
    size_t nSegments = 0;
    size_t nfft = 0;
};

CrossWelchOut crossWelch(const Value &x, const Value &y, const Value &window, size_t noverlap, size_t nfft, std::pmr::memory_resource *mr)
{
    const size_t nx = x.numel();
    const size_t ny = y.numel();
    if (nx != ny)
        throw Error("cpsd/mscohere: x and y must have the same length",
                    0, 0, "cpsd", "", "numkit:cpsd:size");
    const double *xd = x.doubleData();
    const double *yd = y.doubleData();

    ScratchArena scratch(mr);
    size_t winLen;
    ScratchVec<double> win(&scratch);
    if (window.numel() > 0) {
        winLen = window.numel();
        win.resize(winLen);
        const double *w = window.doubleData();
        for (size_t i = 0; i < winLen; ++i) win[i] = w[i];
    } else {
        // MATLAB cpsd / mscohere / tfestimate default: 8 Hamming-windowed
        // segments with 50% overlap (winLen = floor(nx / 4.5)).
        winLen = std::max<size_t>(1,
            static_cast<size_t>(std::floor(static_cast<double>(nx) / 4.5)));
        if (winLen > nx) winLen = nx;
        win.resize(winLen);
        fillHammingWindow(win.data(), winLen);
    }
    if (noverlap == 0) noverlap = winLen / 2;
    if (nfft == 0)     nfft = std::max<size_t>(256, nextPow2(winLen));

    double winPower = 0.0;
    for (size_t i = 0; i < winLen; ++i) winPower += win[i] * win[i];

    const size_t nOut = nfft / 2 + 1;
    auto bufX = ScratchVec<Complex>(nfft, &scratch);
    auto bufY = ScratchVec<Complex>(nfft, &scratch);

    CrossWelchOut o;
    o.Sxx.assign(nOut, 0.0);
    o.Syy.assign(nOut, 0.0);
    o.Sxy.assign(nOut, std::complex<double>(0.0, 0.0));

    const size_t step = winLen - noverlap;
    for (size_t start = 0; start + winLen <= nx; start += step) {
        for (size_t i = 0; i < winLen; ++i) {
            bufX[i] = Complex(xd[start + i] * win[i], 0.0);
            bufY[i] = Complex(yd[start + i] * win[i], 0.0);
        }
        for (size_t i = winLen; i < nfft; ++i) {
            bufX[i] = Complex(0.0, 0.0);
            bufY[i] = Complex(0.0, 0.0);
        }
        fftRadix2(&scratch, bufX, 1);
        fftRadix2(&scratch, bufY, 1);
        for (size_t i = 0; i < nOut; ++i) {
            const double sx = std::norm(bufX[i]);
            const double sy = std::norm(bufY[i]);
            const std::complex<double> sxy = bufX[i] * std::conj(bufY[i]);
            o.Sxx[i] += sx;
            o.Syy[i] += sy;
            o.Sxy[i] += sxy;
        }
        ++o.nSegments;
    }
    o.winPower = winPower;
    o.nfft = nfft;
    o.F.assign(nOut, 0.0);
    for (size_t i = 0; i < nOut; ++i)
        o.F[i] = M_PI * static_cast<double>(i) / static_cast<double>(nOut - 1);
    return o;
}

} // anonymous

std::tuple<Value, Value>
cpsd(const Value &x, const Value &y, const Value &window, size_t noverlap, size_t nfft, double fs, std::pmr::memory_resource *mr)
{
    auto o = crossWelch(x, y, window, noverlap, nfft, mr);
    const size_t nOut = o.Sxx.size();
    // Rescale frequency vector to [0, fs/2] (crossWelch uses [0, pi]).
    auto rescaleF = [&]() {
        for (size_t i = 0; i < nOut; ++i)
            o.F[i] = (fs / 2.0) * static_cast<double>(i)
                     / static_cast<double>(nOut - 1);
    };
    rescaleF();
    if (o.nSegments == 0) {
        Value Pxy0 = Value::matrix(nOut, 1, ValueType::COMPLEX, mr);
        Value F0   = Value::matrix(nOut, 1, ValueType::DOUBLE, mr);
        if (nOut > 0) std::copy(o.F.begin(), o.F.end(), F0.doubleDataMut());
        return std::make_tuple(std::move(Pxy0), std::move(F0));
    }
    // MATLAB cpsd scaling: Pxy[k] = mean_segments(X*conj(Y)) /
    // (winPower * fs). Default fs = 2*pi mirrors MATLAB.
    const double scale = 1.0 / (o.winPower * fs *
                                 static_cast<double>(o.nSegments));
    Value Pxy = Value::matrix(nOut, 1, ValueType::COMPLEX, mr);
    Value F   = Value::matrix(nOut, 1, ValueType::DOUBLE, mr);
    auto *cd = Pxy.complexDataMut();
    auto *fd = F.doubleDataMut();
    for (size_t i = 0; i < nOut; ++i) {
        std::complex<double> s = o.Sxy[i] * scale;
        if (i > 0 && i < o.nfft / 2) s *= 2.0;
        cd[i] = s;
        fd[i] = o.F[i];
    }
    return std::make_tuple(std::move(Pxy), std::move(F));
}

std::tuple<Value, Value>
mscohere(const Value &x, const Value &y, const Value &window, size_t noverlap, size_t nfft, double fs, std::pmr::memory_resource *mr)
{
    auto o = crossWelch(x, y, window, noverlap, nfft, mr);
    const size_t nOut = o.Sxx.size();
    Value Cxy = Value::matrix(nOut, 1, ValueType::DOUBLE, mr);
    Value F   = Value::matrix(nOut, 1, ValueType::DOUBLE, mr);
    auto *cd = Cxy.doubleDataMut();
    auto *fd = F.doubleDataMut();
    for (size_t i = 0; i < nOut; ++i) {
        const double denom = o.Sxx[i] * o.Syy[i];
        const double num   = std::norm(o.Sxy[i]);
        cd[i] = (denom > 0.0) ? num / denom : 0.0;
        fd[i] = (fs / 2.0) * static_cast<double>(i)
                / static_cast<double>(nOut - 1);
    }
    return std::make_tuple(std::move(Cxy), std::move(F));
}

std::tuple<Value, Value>
tfestimate(const Value &x, const Value &y, const Value &window, size_t noverlap, size_t nfft, double fs, std::pmr::memory_resource *mr)
{
    auto o = crossWelch(x, y, window, noverlap, nfft, mr);
    const size_t nOut = o.Sxx.size();
    Value Txy = Value::matrix(nOut, 1, ValueType::COMPLEX, mr);
    Value F   = Value::matrix(nOut, 1, ValueType::DOUBLE, mr);
    auto *td = Txy.complexDataMut();
    auto *fd = F.doubleDataMut();
    // MATLAB convention: Txy = Pyx / Pxx where Pyx = E[Y · conj(X)]
    // = conj(Pxy). The per-segment scaling factor cancels in the
    // ratio, so the raw accumulators are sufficient.
    for (size_t i = 0; i < nOut; ++i) {
        const std::complex<double> Syx = std::conj(o.Sxy[i]);
        td[i] = (o.Sxx[i] > 0.0)
                ? Syx / o.Sxx[i]
                : std::complex<double>(0.0, 0.0);
        fd[i] = (fs / 2.0) * static_cast<double>(i)
                / static_cast<double>(nOut - 1);
    }
    return std::make_tuple(std::move(Txy), std::move(F));
}

} // namespace numkit::signal
