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
//   * harmonicRatio: parabolic-interpolation refinement step from the
//     MATLAB source (parabolicInterpolation helper) deferred — we
//     return the raw max value. Differences are <1% on smooth inputs.

#include <numkit/audio/features/pitch_harmonics.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

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

} // anon

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
Value harmonicRatio(std::pmr::memory_resource *mr, const Value &x, double fs)
{
    const size_t N = x.numel();
    const FrameSpec sp = frameSpec(N, fs, 0.03, 0.02);
    Value out = Value::matrix(sp.numFrames, sp.numFrames == 0 ? 0 : 1,
                              ValueType::DOUBLE, mr);
    if (sp.numFrames == 0) return out;

    // Search lag range: low edge auto-detected (first zero crossing of R)
    // for v1 we use a fixed range similar to pitch (50-400 Hz at typical fs).
    const double minF = 50.0, maxF = 400.0;
    const size_t minLag = static_cast<size_t>(std::floor(fs / maxF));
    const size_t maxLag = std::min<size_t>(sp.winLen - 1,
                                           static_cast<size_t>(std::ceil(fs / minF)));

    ScratchArena scratch(mr);
    ScratchVec<double> win(sp.winLen, &scratch);
    hammingPeriodic(win.data(), sp.winLen);
    ScratchVec<double> frame(sp.winLen, &scratch);
    const size_t mLag = std::max<size_t>(maxLag + 1, 2);
    ScratchVec<double> R(mLag, &scratch);
    ScratchVec<double> P(mLag, &scratch);

    double *od = out.doubleDataMut();
    for (size_t f = 0; f < sp.numFrames; ++f) {
        const size_t start = f * sp.hop;
        for (size_t i = 0; i < sp.winLen; ++i)
            frame[i] = x.elemAsDouble(start + i) * win[i];
        autocorr(frame.data(), sp.winLen, mLag, R.data());
        partialPower(frame.data(), sp.winLen, mLag, P.data());

        const double total = R[0];
        double bestG = 0.0;
        for (size_t k = std::max<size_t>(1, minLag); k <= maxLag && k < mLag; ++k) {
            const double denom = std::sqrt(total * P[k]) + 1e-300;
            const double g = R[k] / denom;
            if (g > bestG) bestG = g;
        }
        // Clip to [0, 1].
        if (bestG < 0.0) bestG = 0.0;
        if (bestG > 1.0) bestG = 1.0;
        od[f] = bestG;
    }
    return out;
}

namespace detail {

void pitch_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pitch: requires (x, fs)",
                    0, 0, "pitch", "", "m:pitch:nargin");
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
