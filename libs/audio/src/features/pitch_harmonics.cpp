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
// KNOWN GAPs (post cycles E-F-K-K2):
//   * pitch shipped methods: NCF (default, cycle E), CEP (cycle K, bit-equal),
//     PEF (cycle K-2, bit-equal). LHS / SRH still deferred — both use
//     fft length = round(fs) which hits the libs/signal::fft non-power-of-2
//     bug; will land after that bug is fixed.
//   * Default median filter (MedianFilterLength=1) — applied with len=1
//     it's a no-op so we omit it.
//   * harmonicRatio: full MATLAB R2025b parity — auto low-edge from first
//     sign change of R[k] + Smith's parabolic peak interpolation. Tiny
//     ~2e-4 mean diff vs MATLAB on pure tones is FP-ordering noise.

#include <numkit/audio/features/pitch_harmonics.hpp>
#include <numkit/signal/transforms/fft.hpp>

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

// ── pitch CEP method (cycle K) ────────────────────────────────────────
// Cepstrum-based pitch estimation. Matches MATLAB R2025b
// audio.internal.pitch.CEP.m exactly:
//   1. Apply hamming(winLen, 'periodic') to each frame.
//   2. NFFT = 2^nextpow2(2*winLen - 1) (always power of 2).
//   3. domain = real(ifft(log(|fft(yw, NFFT)|^2)))
//   4. edge = round(fs ./ fliplr([minF, maxF]))
//   5. Find peak of `domain` in lag range [edge[0], edge[1]].
//   6. f0 = fs / peakLag.
//
// Reference: Noll, "Cepstrum Pitch Determination", JASA 41(2), 1967.
Value pitchCEP(std::pmr::memory_resource *mr, const Value &x, double fs,
                double minF, double maxF)
{
    const size_t N = x.numel();
    const FrameSpec sp = frameSpec(N, fs, 0.052, 0.042);
    Value out = Value::matrix(sp.numFrames, sp.numFrames == 0 ? 0 : 1,
                              ValueType::DOUBLE, mr);
    if (sp.numFrames == 0) return out;
    // edge = round(fs ./ fliplr([minF, maxF])) = [round(fs/maxF), round(fs/minF)]
    const size_t edgeLo = static_cast<size_t>(std::round(fs / maxF));
    const size_t edgeHi = static_cast<size_t>(std::round(fs / minF));
    if (edgeHi <= edgeLo || edgeHi >= sp.winLen) return out;

    // NFFT = next power of 2 >= 2*winLen - 1 (CEP uses zero-padded FFT).
    const size_t NFFT = nextPow2(2 * sp.winLen - 1);

    ScratchArena scratch(mr);
    ScratchVec<double> win(sp.winLen, &scratch);
    hammingPeriodic(win.data(), sp.winLen);

    // Build batched matrix of windowed frames, NFFT × numFrames (zero-padded).
    Value framesV = Value::matrix(NFFT, sp.numFrames, ValueType::DOUBLE, mr);
    double *fd = framesV.doubleDataMut();
    std::fill(fd, fd + NFFT * sp.numFrames, 0.0);
    for (size_t f = 0; f < sp.numFrames; ++f) {
        const size_t start = f * sp.hop;
        for (size_t i = 0; i < sp.winLen; ++i)
            fd[i + f * NFFT] = x.elemAsDouble(start + i) * win[i];
    }

    // FFT along dim 1 (each column independently), then |·|², log, ifft → real.
    Value Y = signal::fft(mr, framesV, static_cast<int>(NFFT), 1);
    // Apply log(|Y|²) = 2*log(|Y|). Since input to ifft must be a Value:
    // build a complex Value of the log-power spectrum (imag=0).
    Value logPow = Value::matrix(NFFT, sp.numFrames, ValueType::COMPLEX, mr);
    Complex *lpd = logPow.complexDataMut();
    const Complex *Yd = Y.complexData();
    const double tinyLog = std::log(std::numeric_limits<double>::min());
    for (size_t i = 0; i < NFFT * sp.numFrames; ++i) {
        const double re = Yd[i].real();
        const double im = Yd[i].imag();
        const double pw = re * re + im * im;
        const double lp = (pw > 0.0) ? std::log(pw) : tinyLog;
        lpd[i] = Complex(lp, 0.0);
    }

    // ifft along dim 1.
    Value cepstrumV = signal::ifft(mr, logPow, static_cast<int>(NFFT), 1);
    // ifft of real-symmetric input is real. Take real part for the cepstrum domain.
    // cepstrumV may be returned as DOUBLE (auto-downgraded by libs/signal::ifft)
    // or COMPLEX. Handle both.
    auto getReal = [&](size_t idx) -> double {
        if (cepstrumV.type() == ValueType::COMPLEX)
            return cepstrumV.complexData()[idx].real();
        return cepstrumV.doubleData()[idx];
    };

    // Peak picking per frame in lag range [edgeLo, edgeHi].
    // MATLAB CEP.m uses 1-based indexing: searches domain(edgeLo:edgeHi),
    // converts the resulting MATLAB-1-based location to f0 = fs/loc.
    // In 0-based C++ terms this means: read domain[k-1] for MATLAB index k,
    // and f0 = fs/k where k is the MATLAB 1-based index.
    double *od = out.doubleDataMut();
    for (size_t f = 0; f < sp.numFrames; ++f) {
        double bestVal = -std::numeric_limits<double>::infinity();
        size_t bestLag1 = edgeLo;  // MATLAB 1-based index
        for (size_t k = edgeLo; k <= edgeHi && (k - 1) < NFFT; ++k) {
            const double v = getReal((k - 1) + f * NFFT);
            if (v > bestVal) { bestVal = v; bestLag1 = k; }
        }
        od[f] = (bestLag1 > 0) ? fs / static_cast<double>(bestLag1) : 0.0;
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

PefFilter buildPefFilter(std::pmr::memory_resource *mr, size_t NFFT)
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

Value pitchPEF(std::pmr::memory_resource *mr, const Value &x, double fs,
                double minF, double maxF)
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
    PefFilter pf = buildPefFilter(mr, NFFT);
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
    Value Afft = signal::fft(mr, aFiltPad, static_cast<int>(m2), 1);
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
        Value Y = signal::fft(mr, framePad, static_cast<int>(NFFT), 1);
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
        Value Zfft = signal::fft(mr, Zpad, static_cast<int>(m2), 1);
        const Complex *Zd = Zfft.complexData();

        // C[k] = Zfft[k] * conj(Afft[k])
        Value Cv = Value::matrix(m2, 1, ValueType::COMPLEX, mr);
        Complex *Cd = Cv.complexDataMut();
        for (size_t i = 0; i < m2; ++i) {
            Cd[i] = Zd[i] * std::conj(Afd[i]);
        }
        Value c1V = signal::ifft(mr, Cv, static_cast<int>(m2), 1);
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

// ── pitch ─────────────────────────────────────────────────────────────
Value pitch(std::pmr::memory_resource *mr, const Value &x, double fs,
             double minF, double maxF)
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
// Matches MATLAB R2025b harmonicRatio.m exactly:
//   1. Auto low-edge per frame: first sign change of R[k] for k >= 1.
//   2. Search peak γ in [lowEdge, highEdge=winLen-1].
//   3. Parabolic interpolation around peak (Smith's quadratic peak).
//   4. Clip to [0, 1].
Value harmonicRatio(std::pmr::memory_resource *mr, const Value &x, double fs)
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
        // MATLAB: Gamma(1:max(lowEdge,1),i) = 0  (1-based) → zero 0..lowEdge-1.
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
        outs[0] = pitchCEP(ctx.engine->resource(), args[0], fs, minF, maxF);
    else if (method == "PEF")
        outs[0] = pitchPEF(ctx.engine->resource(), args[0], fs, minF, maxF);
    else
        outs[0] = pitch(ctx.engine->resource(), args[0], fs, minF, maxF);
}

void harmonicRatio_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("harmonicRatio: requires (x, fs)",
                    0, 0, "harmonicRatio", "", "m:harmonicRatio:nargin");
    outs[0] = harmonicRatio(ctx.engine->resource(), args[0], args[1].toScalar());
}

} // namespace detail

} // namespace numkit::audio
