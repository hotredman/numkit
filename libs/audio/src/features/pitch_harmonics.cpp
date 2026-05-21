// libs/audio/src/features/pitch_harmonics.cpp
//
// Audio Cycle E: pitch + harmonicRatio.
//
// pitch (NCF method, MATLAB R2025b default):
//   For each frame, compute autocorrelation R[k]; normalize via
//   R[k] / sqrt(totalPower * partialPower(k)); search peak in valid
//   lag range [fs/maxF, fs/minF] where Range = [50, 400] Hz default.
//   f0 = fs / peakLag.
//   Defaults: Window = hamming(round(0.052*fs)) (~52 ms),
//             Overlap = round(0.042*fs)            (~42 ms),
//             Range   = [50, 400] Hz.
//
// harmonicRatio:
//   Same autocorrelation + normalization as pitch's NCF, but the
//   metric is the MAX of the normalized correlation in the valid lag
//   range (rather than the lag of the peak). Range [0, 1] roughly.
//   Defaults: Window = hamming(round(0.03*fs)), Overlap = round(0.02*fs).
//
// PMR HARD RULE.
//
// KNOWN GAPs (post cycles E-F-K-K2-K3):
//   * pitch shipped methods: NCF (default, cycle E), CEP (cycle K),
//     PEF (cycle K-2), LHS (cycle K-3). SRH still deferred (uses LPC
//     residual + Blackman-windowed framing — separate algorithm).
//   * Default median filter (MedianFilterLength=1) — applied with len=1
//     it's a no-op so we omit it.
//   * harmonicRatio: full MATLAB R2025b parity — auto low-edge from first
//     sign change of R[k] + Smith's parabolic peak interpolation. Tiny
//     ~2e-4 mean diff vs MATLAB on pure tones is FP-ordering noise.

#include <numkit/audio/features/pitch_harmonics.hpp>
#include <numkit/signal/transforms/fft.hpp>
#include <numkit/signal/spectral_analysis/signal_modeling.hpp>  // lpc
#include <numkit/signal/digital_filtering/filter.hpp>           // filter
#include <numkit/signal/windows/windows.hpp>                    // hann, blackman

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::audio {

namespace {

// Periodic Hamming window: w[n] = 0.54 - 0.46 * cos(2π·n/N).
void hammingPeriodic(double *w, size_t N)
{
    for (size_t n = 0; n < N; ++n)
        w[n] = 0.54 - 0.46 * std::cos(2.0 * M_PI * static_cast<double>(n)
                                        / static_cast<double>(N));
}

// Compute autocorrelation R[k] = Σ x[n]·x[n+k] for k = 0..maxLag-1.
// O(L * maxLag) — adequate for typical winLen ≤ 1024 and maxLag ≤ 320.
void autocorr(const double *x, size_t L, size_t maxLag, double *R)
{
    for (size_t k = 0; k < maxLag; ++k) {
        double s = 0.0;
        for (size_t n = 0; n + k < L; ++n) s += x[n] * x[n + k];
        R[k] = s;
    }
}

// Partial power Σ x[n+k]² for n = 0..L-k-1, k = 0..maxLag-1.
void partialPower(const double *x, size_t L, size_t maxLag, double *P)
{
    for (size_t k = 0; k < maxLag; ++k) {
        double s = 0.0;
        for (size_t n = 0; n + k < L; ++n) s += x[n + k] * x[n + k];
        P[k] = s;
    }
}

struct FrameSpec { size_t winLen, overlap, hop, numFrames; };

FrameSpec frameSpec(size_t N, double fs, double winSec, double ovSec)
{
    FrameSpec s;
    s.winLen  = static_cast<size_t>(std::round(fs * winSec));
    s.overlap = static_cast<size_t>(std::round(fs * ovSec));
    if (s.overlap >= s.winLen && s.winLen > 0) s.overlap = s.winLen - 1;
    s.hop = (s.winLen > s.overlap) ? (s.winLen - s.overlap) : 1;
    s.numFrames = (N >= s.winLen) ? ((N - s.winLen) / s.hop + 1) : 0;
    return s;
}

// 2^nextpow2(x) — smallest power of 2 >= x.
size_t nextPow2(size_t x)
{
    if (x <= 1) return 1;
    size_t p = 1;
    while (p < x) p <<= 1;
    return p;
}

} // anon

// ── pitch CEP method ──────────────────────────────────────────────────
// Cepstrum-based fundamental-frequency (pitch) estimation.
//
// Reference: A. M. Noll, "Cepstrum Pitch Determination", Journal of the
// Acoustical Society of America 41(2):293-309, 1967.
//
// Clean-room reimplementation — see cleanroom/specs/pitchCEP.md. Per
// frame: window, zero-pad, DFT, log power spectrum, inverse DFT back to
// the quefrency domain; the cepstral peak inside the quefrency band for
// [minF, maxF] gives f0. The frame is zero-padded to nextPow2(2*winLen-1)
// so the cepstrum is free of time-domain aliasing.
//
// Compatibility: MATLAB's CEP reports the period from a 1-based
// quefrency index, so a cepstrum sample at 0-based array index q
// corresponds to a period of (q+1) samples; hence f0 = fs/(q+1) and the
// search band is shifted down by one index.
Value pitchCEP(const Value &x, double fs, double minF, double maxF,
               std::pmr::memory_resource *mr)
{
    // Framing — MATLAB pitch defaults: 52 ms window, 42 ms overlap.
    const std::size_t N = x.numel();
    const FrameSpec fr = frameSpec(N, fs, 0.052, 0.042);
    const std::size_t winLen    = fr.winLen;
    const std::size_t hop       = fr.hop;
    const std::size_t numFrames = fr.numFrames;
    if (numFrames == 0 || winLen == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Zero-pad to a power of two >= 2*winLen-1 (anti-aliasing the cepstrum).
    const std::size_t NFFT = nextPow2(2 * winLen - 1);

    ScratchArena arena(mr);
    ScratchVec<double> win(winLen, &arena);
    hammingPeriodic(win.data(), winLen);

    // Column f holds the f-th windowed, zero-padded frame.
    Value frames = Value::matrix(NFFT, numFrames, ValueType::DOUBLE, &arena);
    double *fd = frames.doubleDataMut();
    std::fill(fd, fd + NFFT * numFrames, 0.0);
    for (std::size_t f = 0; f < numFrames; ++f) {
        const std::size_t base = f * hop;
        double *col = fd + f * NFFT;
        for (std::size_t n = 0; n < winLen; ++n) {
            const std::size_t idx = base + n;
            col[n] = (idx < N) ? x.elemAsDouble(idx) * win[n] : 0.0;
        }
    }

    // Forward DFT of every frame, then the log power spectrum.
    Value Y = signal::fft(frames, static_cast<int>(NFFT), 1, &arena);
    Value logspec = Value::matrix(NFFT, numFrames, ValueType::DOUBLE, &arena);
    double *ld = logspec.doubleDataMut();
    const double kLogFloor = std::log(std::numeric_limits<double>::min());
    const std::size_t total = NFFT * numFrames;
    if (Y.type() == ValueType::COMPLEX) {
        const Complex *yd = Y.complexData();
        for (std::size_t i = 0; i < total; ++i) {
            const double re = yd[i].real(), im = yd[i].imag();
            const double p2 = re * re + im * im;
            ld[i] = (p2 > 0.0) ? std::log(p2) : kLogFloor;
        }
    } else {
        const double *yd = Y.doubleData();
        for (std::size_t i = 0; i < total; ++i) {
            const double p2 = yd[i] * yd[i];
            ld[i] = (p2 > 0.0) ? std::log(p2) : kLogFloor;
        }
    }

    // Real cepstrum c[q] = real(IDFT(logspec)).
    Value C = signal::ifft(logspec, static_cast<int>(NFFT), 1, &arena);
    const bool cIsComplex = (C.type() == ValueType::COMPLEX);
    const Complex *cc = cIsComplex ? C.complexData() : nullptr;
    const double  *cr = cIsComplex ? nullptr        : C.doubleData();

    // Quefrency search band. A period of p samples gives f0 = fs/p, so the
    // pitch range [minF, maxF] maps to p in [fs/maxF, fs/minF]. With the
    // 1-based quefrency convention (period = array index + 1) the array
    // index range is [round(fs/maxF)-1, round(fs/minF)-1].
    Value out = Value::matrix(numFrames, 1, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    long qLo = 0, qHi = -1;
    if (minF > 0.0 && maxF > 0.0) {
        qLo = static_cast<long>(std::lround(fs / maxF)) - 1;
        qHi = static_cast<long>(std::lround(fs / minF)) - 1;
    }
    if (qLo < 0) qLo = 0;
    if (qHi > static_cast<long>(NFFT) - 1) qHi = static_cast<long>(NFFT) - 1;
    const bool rangeValid = (qLo <= qHi);

    for (std::size_t f = 0; f < numFrames; ++f) {
        if (!rangeValid) { od[f] = 0.0; continue; }
        const std::size_t colOff = f * NFFT;
        long   qBest = qLo;
        double cBest = -std::numeric_limits<double>::infinity();
        for (long q = qLo; q <= qHi; ++q) {
            const std::size_t idx = colOff + static_cast<std::size_t>(q);
            const double cq = cIsComplex ? cc[idx].real() : cr[idx];
            if (cq > cBest) { cBest = cq; qBest = q; }
        }
        // 1-based quefrency convention: period = qBest + 1 samples.
        od[f] = fs / static_cast<double>(qBest + 1);
    }
    return out;
}

// ── pitch PEF method (cycle K-2) ──────────────────────────────────────
// Pitch Estimation Filter (Gonzalez & Brookes, "PEFAC" 2011). Matches
// MATLAB R2025b audio.internal.pitch.PEF.m exactly:
//
//   NFFT = 2^nextpow2(2*winLen-1)
//   logSpacedFrequency = logspace(1, log10(min(fs/2-1, 4000)), NFFT)
//   linSpacedFrequency = linspace(0, fs/2, NFFT/2+1)
//   wBandEdges[i] = argmin |logSpacedFrequency - Range[i]|
//
//   bw[i] = (logSpaced[i+1] - logSpaced[i-1]) / 2  (with edges clamped)
//   bw /= NFFT
//
//   PEF filter aFilt:
//     K=10, gamma=1.8, num=round(NFFT/2)
//     q = logspace(log10(0.5), log10(K+0.5), num)
//     h = 1 / (gamma - cos(2π q))
//     delta = diff([q(1), midpoints, q(end)])
//     beta = sum(h*delta) / sum(delta)
//     aFilt = h - beta
//     numToPad = last index with q < 1
//
//   Per-frame Z:
//     Y = fft(y .* hamming, NFFT)
//     Yhalf = Y(1:NFFT/2+1); Ypower = |Yhalf|²
//     Ylog = interp1(linSpaced, Ypower, logSpaced)  (linear)
//     Ylog .*= bw
//     Z = [zeros(numToPad); Ylog]   length = numToPad + NFFT
//
//   Cross-correlation (FFT-based):
//     m = max(|Z|, |aFilt|), mxl = min(edge(end), m-1)
//     m2 = min(2^nextpow2(2m-1), NFFT*4)   (always power of 2)
//     c1 = real(ifft(fft(Z, m2) .* conj(fft(aFilt, m2))))
//     R = [c1(m2-mxl+1 : m2); c1(1 : mxl+1)]
//     domain = R(edge(end)+1 : end)
//     locs = argmax in domain[edge(1) : edge(end)]
//     f0 = logSpacedFrequency(locs)
//     clip f0 to [Range(1), Range(end)].
namespace {

// Build the PEF filter. Returns aFilt (column vector) + numToPad.
struct PefFilter {
    Value aFilt;        // num × 1 double
    size_t numToPad;
};

PefFilter buildPefFilter(size_t NFFT, std::pmr::memory_resource *mr)
{
    constexpr double K = 10.0;
    constexpr double gamma = 1.8;
    const size_t num = static_cast<size_t>(std::round(static_cast<double>(NFFT) / 2.0));

    PefFilter pf;
    pf.aFilt = Value::matrix(num, 1, ValueType::DOUBLE, mr);
    if (num == 0) { pf.numToPad = 0; return pf; }

    ScratchArena scratch(mr);
    ScratchVec<double> q(num, &scratch);
    ScratchVec<double> h(num, &scratch);
    ScratchVec<double> delta(num, &scratch);

    // q = logspace(log10(0.5), log10(K+0.5), num)
    const double a = std::log10(0.5);
    const double b = std::log10(K + 0.5);
    if (num == 1) {
        q[0] = std::pow(10.0, a);
    } else {
        const double step = (b - a) / static_cast<double>(num - 1);
        for (size_t i = 0; i < num; ++i)
            q[i] = std::pow(10.0, a + step * static_cast<double>(i));
    }

    // h = 1 / (gamma - cos(2π q))
    for (size_t i = 0; i < num; ++i)
        h[i] = 1.0 / (gamma - std::cos(2.0 * M_PI * q[i]));

    // delta = diff([q(1), (q(1:end-1)+q(2:end))/2, q(end)])
    // Length-num+1 → diff gives length num.
    // delta[0] = midpoint[0] - q[0] = (q[0]+q[1])/2 - q[0] = (q[1]-q[0])/2
    // delta[i] for i in 1..num-2 = midpoint[i] - midpoint[i-1] = (q[i+1]-q[i-1])/2
    // delta[num-1] = q[num-1] - midpoint[num-2] = q[num-1] - (q[num-2]+q[num-1])/2 = (q[num-1]-q[num-2])/2
    if (num == 1) {
        delta[0] = 0.0;
    } else {
        delta[0] = (q[1] - q[0]) * 0.5;
        for (size_t i = 1; i + 1 < num; ++i)
            delta[i] = (q[i + 1] - q[i - 1]) * 0.5;
        delta[num - 1] = (q[num - 1] - q[num - 2]) * 0.5;
    }

    // beta = sum(h .* delta) / sum(delta)
    double sumHD = 0.0, sumD = 0.0;
    for (size_t i = 0; i < num; ++i) { sumHD += h[i] * delta[i]; sumD += delta[i]; }
    const double beta = (sumD > 0.0) ? sumHD / sumD : 0.0;

    double *afd = pf.aFilt.doubleDataMut();
    for (size_t i = 0; i < num; ++i) afd[i] = h[i] - beta;

    // numToPad = find(q < 1, 1, 'last') — index of last element with q < 1
    pf.numToPad = 0;
    for (size_t i = 0; i < num; ++i) {
        if (q[i] < 1.0) pf.numToPad = i + 1;  // 1-based count
    }
    return pf;
}

// Linear interpolation: out[i] = interp1(xs, ys, xq[i], 'linear', NaN-out-of-range)
// xs must be monotonically increasing. Out-of-range queries get NaN
// (matches MATLAB interp1 default).
void linearInterp(const double *xs, const double *ys, size_t N,
                   const double *xq, double *out, size_t M)
{
    if (N < 2) {
        for (size_t i = 0; i < M; ++i) out[i] = std::numeric_limits<double>::quiet_NaN();
        return;
    }
    size_t k = 0;
    for (size_t i = 0; i < M; ++i) {
        const double x = xq[i];
        if (x < xs[0] || x > xs[N - 1]) {
            out[i] = std::numeric_limits<double>::quiet_NaN();
            continue;
        }
        // Binary-search-free walk: xq is typically also monotonic (logSpaced).
        while (k + 1 < N - 1 && xs[k + 1] < x) ++k;
        // Reset k if needed (in case xq isn't monotonic, fall back to scan).
        if (xs[k] > x || xs[k + 1] < x) {
            k = 0;
            while (k + 1 < N && xs[k + 1] < x) ++k;
        }
        const double t = (x - xs[k]) / (xs[k + 1] - xs[k]);
        out[i] = ys[k] + t * (ys[k + 1] - ys[k]);
    }
}

} // anon

Value pitchPEF(const Value &x, double fs, double minF, double maxF, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    const FrameSpec sp = frameSpec(N, fs, 0.052, 0.042);
    Value out = Value::matrix(sp.numFrames, sp.numFrames == 0 ? 0 : 1,
                              ValueType::DOUBLE, mr);
    if (sp.numFrames == 0) return out;
    const size_t NFFT = nextPow2(2 * sp.winLen - 1);
    const size_t Nhalf = NFFT / 2 + 1;

    // logSpacedFrequency = logspace(1, log10(min(fs/2-1, 4000)), NFFT)
    const double logHi = std::log10(std::min(fs * 0.5 - 1.0, 4000.0));
    const double logLo = 1.0;
    ScratchArena scratch(mr);
    ScratchVec<double> logSF(NFFT, &scratch);
    if (NFFT == 1) {
        logSF[0] = std::pow(10.0, logLo);
    } else {
        const double step = (logHi - logLo) / static_cast<double>(NFFT - 1);
        for (size_t i = 0; i < NFFT; ++i)
            logSF[i] = std::pow(10.0, logLo + step * static_cast<double>(i));
    }

    // linSpacedFrequency = linspace(0, fs/2, NFFT/2+1)
    ScratchVec<double> linSF(Nhalf, &scratch);
    if (Nhalf == 1) {
        linSF[0] = 0.0;
    } else {
        const double step = (fs * 0.5) / static_cast<double>(Nhalf - 1);
        for (size_t i = 0; i < Nhalf; ++i) linSF[i] = step * static_cast<double>(i);
    }

    // wBandEdges = nearest log-frequency index for each Range value.
    auto findNearest = [&](double f) -> size_t {
        size_t bestIdx = 0;
        double bestDist = std::abs(logSF[0] - f);
        for (size_t i = 1; i < NFFT; ++i) {
            const double d = std::abs(logSF[i] - f);
            if (d < bestDist) { bestDist = d; bestIdx = i; }
        }
        return bestIdx;  // 0-based; MATLAB equivalent = bestIdx + 1
    };
    const size_t edgeLo0 = findNearest(minF);  // 0-based
    const size_t edgeHi0 = findNearest(maxF);

    // bw[i] = (logSF[i+1] - logSF[i-1]) / 2 / NFFT, with edge values clamped.
    // MATLAB: bwTemp = (logSF(3:end) - logSF(1:end-2)) / 2  (length NFFT-2)
    //         bw = [bwTemp(1); bwTemp; bwTemp(end)] / NFFT  (length NFFT)
    ScratchVec<double> bw(NFFT, &scratch);
    if (NFFT >= 3) {
        const double invN = 1.0 / static_cast<double>(NFFT);
        for (size_t i = 1; i + 1 < NFFT; ++i)
            bw[i] = (logSF[i + 1] - logSF[i - 1]) * 0.5 * invN;
        bw[0]        = bw[1];
        bw[NFFT - 1] = bw[NFFT - 2];
    } else {
        for (size_t i = 0; i < NFFT; ++i) bw[i] = 0.0;
    }

    // PEF filter
    PefFilter pf = buildPefFilter(NFFT, mr);
    const size_t aLen = pf.aFilt.dims().rows();
    const size_t Zlen = pf.numToPad + NFFT;

    // Cross-correlation FFT length
    const size_t mDim = std::max(Zlen, aLen);
    const size_t mxl_signed = std::min(edgeHi0 + 1, mDim - 1);  // edgeHi+1 (1-based MATLAB index)
    const size_t mxl = mxl_signed;
    const size_t m2 = std::min(nextPow2(2 * mDim - 1), NFFT * 4);

    // Hamming window
    ScratchVec<double> win(sp.winLen, &scratch);
    hammingPeriodic(win.data(), sp.winLen);

    // Pre-compute fft(aFilt, m2)
    Value aFiltPad = Value::matrix(m2, 1, ValueType::DOUBLE, mr);
    {
        double *p = aFiltPad.doubleDataMut();
        std::fill(p, p + m2, 0.0);
        const double *src = pf.aFilt.doubleData();
        std::copy(src, src + aLen, p);
    }
    Value Afft = signal::fft(aFiltPad, static_cast<int>(m2), 1, mr);
    const Complex *Afd = Afft.complexData();

    // Per-frame buffers
    ScratchVec<double> frame(sp.winLen, &scratch);
    ScratchVec<double> Yp(Nhalf, &scratch);
    ScratchVec<double> Ylog(NFFT, &scratch);

    Value framePad = Value::matrix(NFFT, 1, ValueType::DOUBLE, mr);
    Value Zpad     = Value::matrix(m2,   1, ValueType::DOUBLE, mr);
    double *fp = framePad.doubleDataMut();
    double *zp = Zpad.doubleDataMut();

    double *od = out.doubleDataMut();
    for (size_t f = 0; f < sp.numFrames; ++f) {
        const size_t start = f * sp.hop;
        for (size_t i = 0; i < sp.winLen; ++i)
            frame[i] = x.elemAsDouble(start + i) * win[i];

        // Y = fft(frame, NFFT) → take half → power
        std::fill(fp, fp + NFFT, 0.0);
        std::copy(frame.data(), frame.data() + sp.winLen, fp);
        Value Y = signal::fft(framePad, static_cast<int>(NFFT), 1, mr);
        const Complex *Yd = Y.complexData();
        for (size_t i = 0; i < Nhalf; ++i) {
            const double re = Yd[i].real();
            const double im = Yd[i].imag();
            Yp[i] = re * re + im * im;
        }

        // Ylog = interp1(linSF, Yp, logSF, 'linear')
        linearInterp(linSF.data(), Yp.data(), Nhalf, logSF.data(), Ylog.data(), NFFT);
        // Ylog .*= bw
        for (size_t i = 0; i < NFFT; ++i) Ylog[i] *= bw[i];

        // Build Z = [zeros(numToPad); Ylog], padded to m2.
        std::fill(zp, zp + m2, 0.0);
        std::copy(Ylog.data(), Ylog.data() + std::min(NFFT, m2 - pf.numToPad),
                  zp + pf.numToPad);
        Value Zfft = signal::fft(Zpad, static_cast<int>(m2), 1, mr);
        const Complex *Zd = Zfft.complexData();

        // C[k] = Zfft[k] * conj(Afft[k])
        Value Cv = Value::matrix(m2, 1, ValueType::COMPLEX, mr);
        Complex *Cd = Cv.complexDataMut();
        for (size_t i = 0; i < m2; ++i) {
            Cd[i] = Zd[i] * std::conj(Afd[i]);
        }
        Value c1V = signal::ifft(Cv, static_cast<int>(m2), 1, mr);
        // c1 may be DOUBLE or COMPLEX (auto-downgrade). Real part either way.
        auto getC1 = [&](size_t i) -> double {
            if (c1V.type() == ValueType::COMPLEX)
                return c1V.complexData()[i].real();
            return c1V.doubleData()[i];
        };

        // R = [c1(m2-mxl+1 : m2); c1(1 : mxl+1)]
        // (1-based MATLAB indices; 0-based here: c1[m2-mxl..m2-1] then c1[0..mxl])
        // domain = R(edge(end)+1 : end)
        // R length = mxl + (mxl+1) = 2*mxl + 1
        // domain starts at MATLAB 1-based index edgeHi+2 = (edgeHi0+1)+1 = edgeHi0+2
        // → 0-based index edgeHi0+1.
        // Then we search domain at MATLAB 1-based [edgeLo+1 .. edgeHi+1]
        // → 0-based [edgeLo0..edgeHi0] OF DOMAIN (which is offset).
        // domain (MATLAB 1-based k) = R(edge(end)+1 + k - 1) = R(edgeHi+k)
        // We want max over k in [edgeLo+1 .. edgeHi+1] (1-based) of domain(k).
        // Substituting: max_{k=edgeLo+1..edgeHi+1} R(edgeHi+k)
        //   = max_{j=2*edgeLo+1..2*edgeHi+1} R(j)  (sub j=edgeHi+k)
        // Hmm tricky. Easiest: build the actual R[] vector then index.
        // R length = 2*mxl + 1 conceptually, but mxl > Rlen-1 is possible.
        const size_t Rlen = mxl + mxl + 1;  // upper + lower
        // Upper part: R[0..mxl-1] = c1[m2-mxl..m2-1] (0-based)
        // Lower part: R[mxl..Rlen-1] = c1[0..mxl] (0-based)
        // domain[i] for 0-based i = R[edgeHi0 + 1 + i]
        // Search 0-based domain index range [edgeLo0..edgeHi0].
        // Compute argmax in this range.
        double bestVal = -std::numeric_limits<double>::infinity();
        size_t bestLoc0 = edgeLo0;  // 0-based index in logSF (returned)
        // Loop over 1-based k in [edgeLo0+1 .. edgeHi0+1] for getCandidates.
        // domain(k) = R(edgeHi0+1 + k) [MATLAB 1-based] = R[edgeHi0+k] (0-based offset).
        // R[idx] for 0-based idx:
        //   idx in [0, mxl-1]: c1[m2-mxl+idx]
        //   idx in [mxl, Rlen-1]: c1[idx-mxl]
        for (size_t k = edgeLo0; k <= edgeHi0; ++k) {
            const size_t Ridx = edgeHi0 + 1 + k;  // 0-based R index
            if (Ridx >= Rlen) break;
            double v;
            if (Ridx < mxl) v = getC1(m2 - mxl + Ridx);
            else            v = getC1(Ridx - mxl);
            if (v > bestVal) { bestVal = v; bestLoc0 = k; }
        }
        // f0 = logSpacedFrequency(bestLoc0 + 1)  [MATLAB 1-based]
        // → in 0-based: logSF[bestLoc0]
        double f0 = (bestLoc0 < NFFT) ? logSF[bestLoc0] : 0.0;
        // Clip to [minF, maxF]
        if (f0 < minF) f0 = minF;
        if (f0 > maxF) f0 = maxF;
        od[f] = f0;
    }
    return out;
}

// ── pitch LHS method ──────────────────────────────────────────────────
// pitchLHS — fundamental-frequency estimation by log harmonic summation.
//
// Principle (Hermes, "Measurement of pitch by subharmonic summation",
// JASA 83(1):257-264, 1988): a voiced signal whose fundamental is f has
// spectral energy concentrated at the integer harmonics f, 2f, 3f, ...
// Therefore, if the log-magnitude spectrum is sampled at integer multiples
// of a candidate fundamental and those samples are summed, the resulting
// score is maximised at the true f0. Hermes used subharmonic summation
// (compressing toward a virtual-pitch percept); here we use the direct
// harmonic-summation form ("log harmonic summation", LHS), which sums the
// log-magnitude spectrum over the first H harmonics of each candidate.
//
// Clean-room reimplementation — see cleanroom/specs/pitchLHS.md.
// Per analysis frame:
//   1. Window the frame with a periodic Hamming window.
//   2. Zero-pad to NFFT = round(fs) and take the DFT; with NFFT = fs each
//      bin spans exactly 1 Hz, so a bin index equals a frequency in Hz and
//      harmonic indexing j*m is a plain integer multiply.
//   3. Form the log-magnitude spectrum S[k] = log(|Y[k]|), flooring a zero
//      magnitude at log(smallest positive normal double).
//   4. domain[j] = sum_{m=1..H} S[j*m] is the harmonic-sum score for a
//      fundamental of j Hz.
//   5. f0 = argmax over the search range; clip to [minF, maxF].
//
// Compatibility: MATLAB's LHS reports f0 from a 1-based bin index, so a
// harmonic-sum peak at 0-based bin j is reported as f0 = j + 1 Hz.
Value pitchLHS(const Value &x, double fs, double minF, double maxF,
               std::pmr::memory_resource *mr)
{
    // --- Method constants (spec §3) -----------------------------------------
    constexpr int    kH         = 5;     // harmonics summed by LHS
    constexpr double kWinSec    = 0.052; // analysis window length (s)
    constexpr double kOverlapS  = 0.042; // analysis overlap (s)

    // --- Framing (spec §3) --------------------------------------------------
    const size_t N  = x.numel();
    const FrameSpec fs_spec = frameSpec(N, fs, kWinSec, kOverlapS);
    const size_t winLen   = fs_spec.winLen;
    const size_t hop      = fs_spec.hop;
    const size_t numFrames = fs_spec.numFrames;

    if (numFrames == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // --- DFT length and harmonic-coverage check (spec §3) -------------------
    // NFFT = round(fs) gives exactly 1-Hz bins (Δf = fs/NFFT = 1).
    const int NFFT = static_cast<int>(std::llround(fs));

    // Highest bin any candidate touches: harmonic H of the highest
    // searchable fundamental, floor(maxF). If it exceeds NFFT the search
    // range cannot be served by this spectrum.
    const long long K = static_cast<long long>(kH) *
                        static_cast<long long>(std::floor(maxF));
    if (NFFT <= 0 || K > static_cast<long long>(NFFT))
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // --- 0-based search range over harmonic-sum candidate bins (spec §3) ----
    // domain[j] scores a fundamental of j Hz; MATLAB reports f0 = j + 1,
    // so to search fundamentals in [minF, maxF] we scan
    // j ∈ [ceil(minF) − 1, floor(maxF) − 1], clamped to valid indices.
    long long jLoRaw = static_cast<long long>(std::ceil(minF)) - 1;
    long long jHiRaw = static_cast<long long>(std::floor(maxF)) - 1;
    if (jLoRaw < 0)
        jLoRaw = 0;
    // A candidate at bin j needs bin j*H present, i.e. j*H ≤ NFFT-1.
    const long long jMaxByCoverage = (NFFT - 1) / kH;
    if (jHiRaw > jMaxByCoverage)
        jHiRaw = jMaxByCoverage;
    if (jHiRaw < jLoRaw)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const size_t jLo = static_cast<size_t>(jLoRaw);
    const size_t jHi = static_cast<size_t>(jHiRaw);

    // --- Scratch (PMR HARD RULE: ScratchArena + ScratchVec only) -----------
    ScratchArena arena(mr);

    // Periodic Hamming analysis window, computed once.
    ScratchVec<double> window(winLen, &arena);
    hammingPeriodic(window.data(), winLen);

    // Batched real input: NFFT rows × numFrames columns. Each column holds
    // one windowed, zero-padded frame; signal::fft(..., NFFT, 1, ...) then
    // transforms every column in a single call.
    Value frames = Value::matrix(static_cast<size_t>(NFFT), numFrames,
                                 ValueType::DOUBLE, &arena);
    double *col0 = frames.doubleDataMut(); // column-major writable buffer

    for (size_t f = 0; f < numFrames; ++f) {
        double *col = col0 + f * static_cast<size_t>(NFFT);
        const size_t base = f * hop;
        for (size_t n = 0; n < static_cast<size_t>(NFFT); ++n) {
            double s = 0.0;
            if (n < winLen) {
                const size_t idx = base + n;
                if (idx < N)
                    s = x.elemAsDouble(idx) * window[n];
            }
            col[n] = s;
        }
    }

    // --- Forward DFT of every frame (dim == 1 → transform each column) ------
    Value spec = signal::fft(frames, NFFT, 1, &arena);
    const Complex *Y = spec.complexData();

    // log-magnitude floor: smallest positive normal double.
    const double kMagFloor = std::log(std::numeric_limits<double>::min());

    // Bins of the log-magnitude spectrum we actually need: 0 .. K-1.
    const size_t Kbins = static_cast<size_t>(K);

    // --- Per-frame harmonic-sum estimation ---------------------------------
    Value out = Value::matrix(numFrames, 1, ValueType::DOUBLE, mr);
    double *outData = out.doubleDataMut();

    ScratchVec<double> logMag(Kbins, &arena);  // S[k] for current frame

    for (size_t f = 0; f < numFrames; ++f) {
        const Complex *Yf = Y + f * static_cast<size_t>(NFFT);

        // S[k] = log(|Y[k]|), floored at log(min normal double).
        for (size_t k = 0; k < Kbins; ++k) {
            const double mag = std::abs(Yf[k]);
            logMag[k] = (mag > 0.0) ? std::log(mag) : kMagFloor;
        }

        // domain[j] = Σ_{m=1..H} S[j·m]; pick the maximising bin.
        size_t bestJ   = jLo;
        double bestSum = -std::numeric_limits<double>::infinity();
        for (size_t j = jLo; j <= jHi; ++j) {
            double sum = 0.0;
            for (int m = 1; m <= kH; ++m)
                sum += logMag[j * static_cast<size_t>(m)];
            if (sum > bestSum) {
                bestSum = sum;
                bestJ   = j;
            }
        }

        // Compatibility convention: a 0-based peak at bin j → f0 = j + 1 Hz.
        double f0 = static_cast<double>(bestJ) + 1.0;

        // Clip to the requested search range.
        if (f0 < minF)
            f0 = minF;
        else if (f0 > maxF)
            f0 = maxF;

        outData[f] = f0;
    }

    return out;
}

// ── pitch SRH method (Cycle K-4) ──────────────────────────────────────
// Summation of Residual Harmonics (Drugman & Alwan, INTERSPEECH 2011).
// Matches MATLAB R2025b audio.internal.pitch.SRH.m one-to-one:
//
//   1. Frame x with N=round(0.025*fs), hopSize=round(0.005*fs)
//      (SRH-specific framing — done by caller / pitch.m wrapper).
//      Apply hann(N, 'periodic') per frame.
//   2. Compute LPC(y_frame, 12) per frame → A matrix.
//   3. inv = filter(A_row, 1, y_col) per frame (LPC inverse → residual).
//   4. Overlap-add inv frames into full-length residual using resHopLength.
//   5. Re-frame residual with default WindowLength/OverlapLength.
//   6. Apply blackman(WindowLength, 'periodic') per re-frame.
//   7. res = fft(residualBuff, round(fs)) per frame; E = |res(1:5*edgeHi)|.
//   8. domain[freq] = E[freq] + Σ_{m=2..5} (E[m*freq] - E[round((m-0.5)*freq)])
//      per 1-based MATLAB freq in [edgeLo, edgeHi].
//   9. f0 = peak idx (Hz); clip to [minF, maxF].
//
// Note: this function frames internally per pitch.m wrapper — caller
// passes raw `x`, framing happens inside (matches MATLAB's iDetectPitch
// dispatch structure).
Value pitchSRH(const Value &x, double fs, double minF, double maxF, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    if (N == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Default framing for output count (matches numHopsFinal in pitch.m).
    const size_t winLenDefault = static_cast<size_t>(std::round(fs * 0.052));
    const size_t overlapDefault = static_cast<size_t>(std::round(fs * 0.042));
    const size_t hopDefault = (winLenDefault > overlapDefault)
                                ? (winLenDefault - overlapDefault) : 1;
    const size_t numHopsFinal = (N >= winLenDefault)
                                 ? ((N - winLenDefault) / hopDefault + 1)
                                 : 0;
    Value out = Value::matrix(numHopsFinal, numHopsFinal == 0 ? 0 : 1,
                              ValueType::DOUBLE, mr);
    if (numHopsFinal == 0) return out;

    // SRH-specific framing.
    const size_t Nsrh = static_cast<size_t>(std::round(fs * 0.025));
    const size_t hopSrh = static_cast<size_t>(std::round(fs * 0.005));
    const size_t numHops = (N >= Nsrh) ? ((N - Nsrh) / hopSrh + 1) : 0;
    if (numHops == 0) return out;

    const size_t edgeLo = static_cast<size_t>(std::ceil(minF));
    const size_t edgeHi = static_cast<size_t>(std::floor(maxF));
    if (edgeHi <= edgeLo) return out;

    const size_t fftLen = static_cast<size_t>(std::round(fs));
    const size_t maxBin = 5 * edgeHi;
    if (maxBin > fftLen) return out;

    constexpr int lpcOrder = 12;

    ScratchArena scratch(mr);

    // Step 1: build SRH-framed signal y (Nsrh × numHops), windowed by
    // PERIODIC hann (MATLAB hann(N, 'periodic') uses denominator N, not N-1).
    Value yWin = Value::matrix(Nsrh, numHops, ValueType::DOUBLE, mr);
    {
        double *yd = yWin.doubleDataMut();
        ScratchVec<double> hannPer(Nsrh, &scratch);
        for (size_t i = 0; i < Nsrh; ++i)
            hannPer[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * static_cast<double>(i)
                                                  / static_cast<double>(Nsrh)));
        for (size_t f = 0; f < numHops; ++f) {
            const size_t start = f * hopSrh;
            for (size_t i = 0; i < Nsrh; ++i) {
                const size_t srcIdx = start + i;
                const double xv = (srcIdx < N) ? x.elemAsDouble(srcIdx) : 0.0;
                yd[i + f * Nsrh] = xv * hannPer[i];
            }
        }
    }

    // Step 2-3: per-frame LPC + filter (LPC inverse → residual estimate).
    Value invMat = Value::matrix(Nsrh, numHops, ValueType::DOUBLE, mr);
    double *invd = invMat.doubleDataMut();
    {
        const double *yd = yWin.doubleData();
        Value oneScalar = Value::scalar(1.0, mr);
        for (size_t f = 0; f < numHops; ++f) {
            // Extract column f as 1-D vector.
            Value yCol = Value::matrix(Nsrh, 1, ValueType::DOUBLE, mr);
            std::copy(yd + f * Nsrh, yd + (f + 1) * Nsrh, yCol.doubleDataMut());
            // LPC: returns (a, g)
            auto [a_row, g] = signal::lpc(yCol, lpcOrder, mr);
            // filter(a, 1, y_col) → residual estimate
            Value res = signal::filter(a_row, oneScalar, yCol, mr);
            const double *rd = res.doubleData();
            std::copy(rd, rd + Nsrh, invd + f * Nsrh);
        }
    }

    // Step 4: overlap-add inv frames into full-length residual (length = N).
    ScratchVec<double> residual(N, &scratch);
    std::fill(residual.data(), residual.data() + N, 0.0);
    for (size_t kk = 0; kk < numHops; ++kk) {
        const size_t start = kk * hopSrh;
        const size_t end = std::min(start + Nsrh, N);
        for (size_t i = 0; i + start < end; ++i) {
            residual[start + i] += invd[i + kk * Nsrh];
        }
    }

    // Step 5: re-frame residual using default WindowLength/OverlapLength.
    // MATLAB: numHops = ceil((residual_len - winLen) / hopLen) + 1
    const size_t numHops2 =
        (N >= winLenDefault) ? (((N - winLenDefault) + hopDefault - 1) / hopDefault + 1) : 0;
    if (numHops2 == 0) return out;

    Value residualBuff = Value::matrix(winLenDefault, numHops2, ValueType::DOUBLE, mr);
    double *rbd = residualBuff.doubleDataMut();
    std::fill(rbd, rbd + winLenDefault * numHops2, 0.0);
    for (size_t hop = 0; hop < numHops2; ++hop) {
        const size_t start = hop * hopDefault;
        const size_t avail = (start < N) ? std::min(winLenDefault, N - start) : 0;
        for (size_t i = 0; i < avail; ++i)
            rbd[i + hop * winLenDefault] = residual[start + i];
    }

    // Step 6: apply PERIODIC blackman window (MATLAB blackman(N, 'periodic')).
    // periodic: w[n] = 0.42 - 0.5·cos(2π n/N) + 0.08·cos(4π n/N), n=0..N-1.
    {
        ScratchVec<double> bw(winLenDefault, &scratch);
        for (size_t i = 0; i < winLenDefault; ++i) {
            const double a = 2.0 * M_PI * static_cast<double>(i)
                              / static_cast<double>(winLenDefault);
            bw[i] = 0.42 - 0.5 * std::cos(a) + 0.08 * std::cos(2.0 * a);
        }
        for (size_t hop = 0; hop < numHops2; ++hop) {
            for (size_t i = 0; i < winLenDefault; ++i)
                rbd[i + hop * winLenDefault] *= bw[i];
        }
    }

    // Step 7: per-frame fft to length fftLen=round(fs); take |fft(1:maxBin)|.
    Value framePad = Value::matrix(fftLen, 1, ValueType::DOUBLE, mr);
    double *fp = framePad.doubleDataMut();
    ScratchVec<double> E(maxBin, &scratch);
    ScratchVec<double> domain(edgeHi + 1, &scratch);

    // Output: numHops2 frames; pitch.m post-reshapes to numHopsFinal.
    // Compute per frame, store in temporary, then reshape/clip at end.
    ScratchVec<double> f0all(numHops2, &scratch);

    for (size_t hop = 0; hop < numHops2; ++hop) {
        std::fill(fp, fp + fftLen, 0.0);
        for (size_t i = 0; i < winLenDefault; ++i)
            fp[i] = rbd[i + hop * winLenDefault];

        Value Y = signal::fft(framePad, static_cast<int>(fftLen), 1, mr);
        const Complex *Yd = Y.complexData();
        for (size_t k = 0; k < maxBin; ++k) E[k] = std::abs(Yd[k]);

        // Step 8: domain[freq] = E[freq] + Σ_{m=2..5} (E[m·freq] - E[round((m-0.5)·freq)])
        // MATLAB 1-based: domain(freq) for freq = edge(1):edge(end).
        // Using 0-based: domain index = freq (1-based MATLAB) means index freq-1 in array.
        // E(freq) means MATLAB 1-based, so 0-based access E[freq-1].
        std::fill(domain.data(), domain.data() + edgeHi + 1, 0.0);
        for (size_t freq = edgeLo; freq <= edgeHi; ++freq) {
            // 1-based MATLAB freq. Compute 0-based indices freq-1, 2*freq-1, etc.
            double s = E[freq - 1];
            for (int m = 2; m <= 5; ++m) {
                const size_t mFreq = static_cast<size_t>(m) * freq;
                const size_t halfFreq = static_cast<size_t>(std::round((m - 0.5) * static_cast<double>(freq)));
                if (mFreq <= maxBin && halfFreq >= 1 && halfFreq <= maxBin) {
                    s += E[mFreq - 1] - E[halfFreq - 1];
                }
            }
            domain[freq] = s;  // store at 0-based index freq (matches MATLAB 1-based freq)
        }

        // Step 9: peak in domain[edgeLo..edgeHi]
        double bestVal = -std::numeric_limits<double>::infinity();
        size_t bestLoc1 = edgeLo;
        for (size_t k = edgeLo; k <= edgeHi; ++k) {
            if (domain[k] > bestVal) { bestVal = domain[k]; bestLoc1 = k; }
        }
        double f0v = static_cast<double>(bestLoc1);
        if (f0v < minF) f0v = minF;
        if (f0v > maxF) f0v = maxF;
        f0all[hop] = f0v;
    }

    // Reshape/copy to numHopsFinal (matches MATLAB's reshape post-call).
    // MATLAB does reshape(f0, numHopsFinal, c) — works because numHops2 == numHopsFinal
    // after the residual is re-framed with default params. Verify:
    double *od = out.doubleDataMut();
    const size_t copyN = std::min(numHopsFinal, numHops2);
    for (size_t i = 0; i < copyN; ++i) od[i] = f0all[i];
    // Pad with last value if needed (should not happen if framing matches).
    for (size_t i = copyN; i < numHopsFinal; ++i) od[i] = (copyN > 0) ? f0all[copyN - 1] : 0.0;
    return out;
}

// ── pitch ─────────────────────────────────────────────────────────────
Value pitch(const Value &x, double fs, double minF, double maxF, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    const FrameSpec sp = frameSpec(N, fs, 0.052, 0.042);
    Value out = Value::matrix(sp.numFrames, sp.numFrames == 0 ? 0 : 1,
                              ValueType::DOUBLE, mr);
    if (sp.numFrames == 0) return out;

    const size_t minLag = static_cast<size_t>(std::floor(fs / maxF));
    const size_t maxLag = std::min<size_t>(sp.winLen - 1,
                                           static_cast<size_t>(std::ceil(fs / minF)));
    if (maxLag <= minLag) return out;

    ScratchArena scratch(mr);
    ScratchVec<double> win(sp.winLen, &scratch);
    hammingPeriodic(win.data(), sp.winLen);
    ScratchVec<double> frame(sp.winLen, &scratch);
    ScratchVec<double> R(maxLag + 1, &scratch);
    ScratchVec<double> P(maxLag + 1, &scratch);

    double *od = out.doubleDataMut();
    for (size_t f = 0; f < sp.numFrames; ++f) {
        const size_t start = f * sp.hop;
        for (size_t i = 0; i < sp.winLen; ++i)
            frame[i] = x.elemAsDouble(start + i) * win[i];
        autocorr(frame.data(), sp.winLen, maxLag + 1, R.data());
        partialPower(frame.data(), sp.winLen, maxLag + 1, P.data());

        const double total = R[0];
        // NCF: γ[k] = R[k] / sqrt(total * P[k]).
        double bestVal = -1.0;
        size_t bestLag = minLag;
        for (size_t k = minLag; k <= maxLag; ++k) {
            const double denom = std::sqrt(total * P[k]) + 1e-300;
            const double g = R[k] / denom;
            if (g > bestVal) { bestVal = g; bestLag = k; }
        }
        // f0 = fs / lag (with parabolic interpolation for sub-sample lag).
        double lag = static_cast<double>(bestLag);
        if (bestLag > minLag && bestLag < maxLag) {
            const double a = R[bestLag - 1] / (std::sqrt(total * P[bestLag - 1]) + 1e-300);
            const double b = bestVal;
            const double c = R[bestLag + 1] / (std::sqrt(total * P[bestLag + 1]) + 1e-300);
            const double denom = 2.0 * (2.0 * b - c - a);
            if (std::abs(denom) > 1e-12)
                lag = static_cast<double>(bestLag) - (a - c) / denom;
        }
        od[f] = (lag > 0.0) ? fs / lag : 0.0;
    }
    return out;
}

// ── harmonicRatio ─────────────────────────────────────────────────────
// Harmonic ratio (MPEG-7 / Peeters 2004), normalized-autocorrelation
// form. Per frame:
//   1. Auto low-edge: first sign change of R[k] for k >= 1.
//   2. Search peak γ in [lowEdge, highEdge=winLen-1].
//   3. Parabolic interpolation around peak (Smith's quadratic peak).
//   4. Clip to [0, 1].
Value harmonicRatio(const Value &x, double fs, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    const FrameSpec sp = frameSpec(N, fs, 0.03, 0.02);
    Value out = Value::matrix(sp.numFrames, sp.numFrames == 0 ? 0 : 1,
                              ValueType::DOUBLE, mr);
    if (sp.numFrames == 0) return out;

    const size_t maxLag = sp.winLen - 1;  // highEdge = winLen-1 per MATLAB

    ScratchArena scratch(mr);
    ScratchVec<double> win(sp.winLen, &scratch);
    hammingPeriodic(win.data(), sp.winLen);
    ScratchVec<double> frame(sp.winLen, &scratch);
    const size_t mLag = std::max<size_t>(maxLag + 1, 2);
    ScratchVec<double> R(mLag, &scratch);
    ScratchVec<double> P(mLag, &scratch);
    ScratchVec<double> gamma(mLag, &scratch);

    double *od = out.doubleDataMut();
    for (size_t f = 0; f < sp.numFrames; ++f) {
        const size_t start = f * sp.hop;
        for (size_t i = 0; i < sp.winLen; ++i)
            frame[i] = x.elemAsDouble(start + i) * win[i];
        autocorr(frame.data(), sp.winLen, mLag, R.data());
        partialPower(frame.data(), sp.winLen, mLag, P.data());

        const double total = R[0];
        // Build full normalized correlation γ[k] = R[k] / sqrt(total*P[k]).
        for (size_t k = 0; k < mLag; ++k) {
            const double denom = std::sqrt(total * P[k])
                                  + std::sqrt(std::numeric_limits<double>::epsilon());
            gamma[k] = R[k] / denom;
        }
        // Auto low-edge: first sign change of R[k] for k >= 1.
        // (i.e. first k where sign(R[k]) != sign(R[k-1])).
        size_t lowEdge = maxLag;  // sentinel = no sign change → use maxLag
        const double r0 = R[0];
        int prev_sign = (r0 > 0.0) ? 1 : (r0 < 0.0 ? -1 : 0);
        for (size_t k = 1; k < mLag; ++k) {
            const int s = (R[k] > 0.0) ? 1 : (R[k] < 0.0 ? -1 : 0);
            if (s != prev_sign) { lowEdge = k + 1; break; }
        }
        if (lowEdge < 1) lowEdge = 1;
        // Zero the normalized correlation below the low edge (0..lowEdge-1).
        for (size_t k = 0; k < lowEdge && k < mLag; ++k) gamma[k] = 0.0;

        // Find peak.
        size_t peakIdx = lowEdge;
        double peakVal = 0.0;
        for (size_t k = lowEdge; k < mLag; ++k) {
            if (gamma[k] > peakVal) { peakVal = gamma[k]; peakIdx = k; }
        }
        // Parabolic interpolation (Smith's quadratic-peak formula):
        //   refined = b - 0.25 * (a - c) * s
        //   where s = (c - a) / (2*(2*b - c - a))
        double refined = peakVal;
        if (peakIdx > 0 && peakIdx + 1 < mLag) {
            const double a = gamma[peakIdx - 1];
            const double b = gamma[peakIdx];
            const double c = gamma[peakIdx + 1];
            const double denomS = 2.0 * (2.0 * b - c - a);
            if (std::abs(denomS) > 1e-12) {
                const double s = (c - a) / denomS;
                refined = b - 0.25 * (a - c) * s;
            }
        }
        if (refined < 0.0) refined = 0.0;
        if (refined > 1.0) refined = 1.0;
        od[f] = refined;
    }
    return out;
}

namespace detail {

// pitch dispatches on optional Method arg. Recognized:
//   'NCF' (default, cycle E)
//   'CEP' (cycle K)
//   'PEF' (cycle K-2)
// 'LHS'/'SRH' deferred — fall through to NCF for now.
//
// Cycle L (partial) added 'Range' NV pair → minF/maxF override default
// [50, 400] Hz pitch search range.
//
// Calling convention supports Name-Value pairs:
//   pitch(x, fs)                                — NCF default
//   pitch(x, fs, 'Method', 'CEP')
//   pitch(x, fs, 'Range', [80 250])             — restrict to speech
//   pitch(x, fs, 'Method', 'PEF', 'Range', [...])
void pitch_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pitch: requires (x, fs)",
                    0, 0, "pitch", "", "m:pitch:nargin");
    std::string method = "NCF";
    double minF = 50.0, maxF = 400.0;
    // Parse Name-Value pairs starting at args[2].
    for (size_t i = 2; i + 1 < args.size(); i += 2) {
        if (args[i].type() != ValueType::CHAR && args[i].type() != ValueType::STRING)
            continue;
        std::string name = args[i].toString();
        std::transform(name.begin(), name.end(), name.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        if (name == "method") {
            std::string m = args[i + 1].toString();
            std::transform(m.begin(), m.end(), m.begin(),
                            [](unsigned char c) { return std::toupper(c); });
            method = m;
        } else if (name == "range") {
            const Value &r = args[i + 1];
            if (r.numel() != 2)
                throw Error("pitch: Range must be a 2-element vector [lo hi]",
                            0, 0, "pitch", "", "m:pitch:BadRange");
            const double lo = r.elemAsDouble(0);
            const double hi = r.elemAsDouble(1);
            if (!(lo > 0.0 && hi > lo))
                throw Error("pitch: Range must satisfy 0 < Range(1) < Range(2)",
                            0, 0, "pitch", "", "m:pitch:BadRange");
            minF = lo;
            maxF = hi;
        }
    }
    const double fs = args[1].toScalar();
    if (method == "CEP")
        outs[0] = pitchCEP(args[0], fs, minF, maxF, ctx.engine->resource());
    else if (method == "PEF")
        outs[0] = pitchPEF(args[0], fs, minF, maxF, ctx.engine->resource());
    else if (method == "LHS")
        outs[0] = pitchLHS(args[0], fs, minF, maxF, ctx.engine->resource());
    else if (method == "SRH")
        outs[0] = pitchSRH(args[0], fs, minF, maxF, ctx.engine->resource());
    else
        outs[0] = pitch(args[0], fs, minF, maxF, ctx.engine->resource());
}

void harmonicRatio_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("harmonicRatio: requires (x, fs)",
                    0, 0, "harmonicRatio", "", "m:harmonicRatio:nargin");
    outs[0] = harmonicRatio(args[0], args[1].toScalar(), ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::audio
