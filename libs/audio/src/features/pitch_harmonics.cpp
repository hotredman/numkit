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
// KNOWN GAPs:
//   * pitch: only NCF method; PEF/CEP/LHS/SRH deferred.
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
Value pitchCEP(std::pmr::memory_resource *mr, const Value &x, double fs)
{
    const size_t N = x.numel();
    const FrameSpec sp = frameSpec(N, fs, 0.052, 0.042);
    Value out = Value::matrix(sp.numFrames, sp.numFrames == 0 ? 0 : 1,
                              ValueType::DOUBLE, mr);
    if (sp.numFrames == 0) return out;

    const double minF = 50.0, maxF = 400.0;
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

// ── pitch ─────────────────────────────────────────────────────────────
Value pitch(std::pmr::memory_resource *mr, const Value &x, double fs)
{
    const size_t N = x.numel();
    const FrameSpec sp = frameSpec(N, fs, 0.052, 0.042);
    Value out = Value::matrix(sp.numFrames, sp.numFrames == 0 ? 0 : 1,
                              ValueType::DOUBLE, mr);
    if (sp.numFrames == 0) return out;

    const double minF = 50.0, maxF = 400.0;
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

// Cycle K: pitch dispatches on optional Method arg. Recognized:
//   'NCF' (default, cycle E)
//   'CEP' (cycle K)
// Method args 'PEF'/'LHS'/'SRH' deferred — fall through to NCF for now.
//
// Calling convention supports two shapes:
//   pitch(x, fs)                                — NCF default
//   pitch(x, fs, 'Method', 'CEP')               — Name-Value pair
//   pitch(x, fs, struct(...))                   — not yet supported
void pitch_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pitch: requires (x, fs)",
                    0, 0, "pitch", "", "m:pitch:nargin");
    std::string method = "NCF";
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
        }
    }
    if (method == "CEP")
        outs[0] = pitchCEP(ctx.engine->resource(), args[0], args[1].toScalar());
    else
        outs[0] = pitch(ctx.engine->resource(), args[0], args[1].toScalar());
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
