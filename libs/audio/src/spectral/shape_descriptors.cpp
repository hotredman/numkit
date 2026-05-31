// libs/audio/src/spectral/shape_descriptors.cpp
//
// Audio Toolbox spectral shape descriptors. Audio Cycle B.
//
//   spectralCentroid       Σ(f·X) / Σ(X) — first moment
//   spectralSpread         sqrt(Σ((f-c)²·X) / Σ(X)) — std around centroid
//   spectralRolloffPoint   freq below which P% of energy contained (P=0.95)
//   spectralDecrease       (1/Σk≥2 X(k)) · Σk≥2 (X(k)-X(1))/(k-1)
//   spectralSlope          regression slope of log10(X+eps) vs f
//   spectralFlux           (Σ |X_t - X_{t-1}|^p)^(1/p)  (p default 2)
//
// Each function accepts either:
//   (x, fs)  — x is real column vector signal, fs is scalar sample rate.
//              Internal STFT: window=rectwin(round(0.03*fs)), overlap
//              =round(0.02*fs), FFTLength=winLen. Returns per-frame col.
//   (X, F)   — X is power-spectrum matrix (M-by-N, one column per frame),
//              F is M-element frequency vector. Returns 1-by-N row of
//              per-column metric values.
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.

#include <numkit/audio/spectral/shape_descriptors.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "fft_one_sided.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::audio {

namespace {

struct Stft {
    Value X;   // M × N, column-major; M = winLen/2+1, N = numFrames
    Value F;   // M × 1, frequency axis in Hz
};

// Build the STFT (short-time Fourier transform) power matrix. Default
// parameters (numerical parity checked against MATLAB R2025b):
//   - window  = rectwin(round(0.03*fs)) (default for fs s.t. winLen > 120)
//   - overlap = round(0.02*fs)
//   - FFTLen  = winLen
//   - SpectrumType = 'power': Yb = |Y|² / (0.5 · sum(win)²)
//   - DC bin halved (binLow == 1)
//   - Nyquist bin halved when fftLength is even
Stft computeStft(std::pmr::memory_resource *mr, const Value &x, double fs)
{
    const size_t N = x.numel();
    const size_t winLen  = static_cast<size_t>(std::round(fs * 0.03));
    const size_t overlap = static_cast<size_t>(std::round(fs * 0.02));
    const size_t hop = winLen - overlap;
    const size_t M   = winLen / 2 + 1;
    const size_t numFrames = (N >= winLen) ? ((N - winLen) / hop + 1) : 0;

    Stft s;
    s.X = Value::matrix(M, numFrames, ValueType::DOUBLE, mr);
    s.F = Value::matrix(M, 1, ValueType::DOUBLE, mr);
    if (numFrames == 0 || winLen == 0) return s;

    // Frequency axis: 0 .. fs/2 in M points.
    double *fd = s.F.doubleDataMut();
    for (size_t k = 0; k < M; ++k)
        fd[k] = static_cast<double>(k) * fs / static_cast<double>(winLen);

    // Window normalization factor: 0.5 · sum(win)². For rectwin(N), sum=N → N²/2.
    const double winSum = static_cast<double>(winLen);  // sum(rectwin(N)) = N
    const double normPow = 0.5 * winSum * winSum;

    ScratchArena scratch(mr);
    ScratchVec<double> frame(winLen, &scratch);
    double *Xd = s.X.doubleDataMut();
    const double inv = (normPow > 0.0) ? 1.0 / normPow : 0.0;
    const bool nyquistHalve = ((winLen % 2) == 0) && (M > 0);
    for (size_t f = 0; f < numFrames; ++f) {
        const size_t start = f * hop;
        for (size_t i = 0; i < winLen; ++i) frame[i] = x.elemAsDouble(start + i);
        double *col = Xd + f * M;
        detail::fftPowerHalf(mr, frame.data(), winLen, col);
        for (size_t k = 0; k < M; ++k) col[k] *= inv;
        col[0] *= 0.5;
        if (nyquistHalve) col[M - 1] *= 0.5;
    }
    return s;
}

// Detect (x, fs) vs (X, F). MATLAB rule: if f is scalar, x is time-domain
// + f is sample rate. If f is a vector, x is frequency-domain (one
// spectrum per column) + f is the frequency axis.
struct InputData {
    Value X;        // M × N power spectrum
    Value F;        // M × 1 frequency vector
    bool   isTime;  // true when input was (x, fs)
};

InputData prepareInput(std::pmr::memory_resource *mr,
                       const Value &x, const Value &f)
{
    InputData d;
    if (f.numel() == 1) {
        // Time-domain: STFT internally.
        const double fs = f.toScalar();
        Stft s = computeStft(mr, x, fs);
        d.X = std::move(s.X);
        d.F = std::move(s.F);
        d.isTime = true;
    } else {
        d.X = x;
        d.F = f;
        d.isTime = false;
    }
    return d;
}

// Iterate columns of an M×N matrix; call cb(colPtr) for each.
template <typename Cb>
Value perColumn(std::pmr::memory_resource *mr, const Value &X, Cb cb)
{
    const size_t M = X.dims().rows();
    const size_t N = X.dims().cols();
    Value out = Value::matrix(N, N == 0 ? 0 : 1, ValueType::DOUBLE, mr);
    if (M == 0 || N == 0) return out;
    double *od = out.doubleDataMut();
    const double *Xd = X.doubleData();
    ScratchArena scratch(mr);
    ScratchVec<double> col(M, &scratch);
    for (size_t n = 0; n < N; ++n) {
        for (size_t i = 0; i < M; ++i) col[i] = Xd[i + n * M];
        od[n] = cb(col.data(), M);
    }
    return out;
}

// Same but returns a row vector (audio-toolbox convention for some).
template <typename Cb>
Value perColumnRow(std::pmr::memory_resource *mr, const Value &X, Cb cb)
{
    const size_t M = X.dims().rows();
    const size_t N = std::max<size_t>(1, X.dims().cols());
    Value out = Value::matrix(1, N, ValueType::DOUBLE, mr);
    if (M == 0) return out;
    double *od = out.doubleDataMut();
    const double *Xd = X.doubleData();
    ScratchArena scratch(mr);
    ScratchVec<double> col(M, &scratch);
    for (size_t n = 0; n < N; ++n) {
        for (size_t i = 0; i < M; ++i) col[i] = Xd[i + n * M];
        od[n] = cb(col.data(), M);
    }
    return out;
}

double centroidOf(const double *X, const double *F, size_t M)
{
    double sumX = 0.0, sumFX = 0.0;
    for (size_t i = 0; i < M; ++i) { sumX += X[i]; sumFX += F[i] * X[i]; }
    return (sumX > 0.0) ? sumFX / sumX : 0.0;
}

} // anon

// ── Centroid ──────────────────────────────────────────────────────────
Value spectralCentroid(std::pmr::memory_resource *mr, const Value &x, const Value &f)
{
    auto d = prepareInput(mr, x, f);
    const double *Fd = d.F.doubleData();
    return perColumn(mr, d.X, [Fd](const double *col, size_t M) {
        return centroidOf(col, Fd, M);
    });
}

// ── Spread ────────────────────────────────────────────────────────────
Value spectralSpread(std::pmr::memory_resource *mr, const Value &x, const Value &f)
{
    auto d = prepareInput(mr, x, f);
    const double *Fd = d.F.doubleData();
    return perColumn(mr, d.X, [Fd](const double *col, size_t M) {
        const double c = centroidOf(col, Fd, M);
        double sumX = 0.0, sumDX = 0.0;
        for (size_t i = 0; i < M; ++i) {
            const double dF = Fd[i] - c;
            sumX  += col[i];
            sumDX += dF * dF * col[i];
        }
        return (sumX > 0.0) ? std::sqrt(sumDX / sumX) : 0.0;
    });
}

// ── Rolloff point ─────────────────────────────────────────────────────
Value spectralRolloffPoint(std::pmr::memory_resource *mr, const Value &x,
                           const Value &f, double percentile)
{
    auto d = prepareInput(mr, x, f);
    const double *Fd = d.F.doubleData();
    return perColumn(mr, d.X, [Fd, percentile](const double *col, size_t M) {
        double total = 0.0;
        for (size_t i = 0; i < M; ++i) total += col[i];
        const double thresh = percentile * total;
        if (total <= 0.0 || M == 0) return 0.0;
        double accum = 0.0;
        for (size_t i = 0; i < M; ++i) {
            accum += col[i];
            if (accum >= thresh) return Fd[i];
        }
        return Fd[M - 1];
    });
}

// ── Decrease ──────────────────────────────────────────────────────────
Value spectralDecrease(std::pmr::memory_resource *mr, const Value &x, const Value &f)
{
    auto d = prepareInput(mr, x, f);
    return perColumn(mr, d.X, [](const double *col, size_t M) {
        if (M < 2) return 0.0;
        const double X1 = col[0];
        double num = 0.0, den = 0.0;
        for (size_t k = 1; k < M; ++k) {
            num += (col[k] - X1) / static_cast<double>(k);
            den += col[k];
        }
        return (den > 0.0) ? num / den : 0.0;
    });
}

// ── Slope ─────────────────────────────────────────────────────────────
// Ordinary-least-squares regression slope of mean-power-magnitude X
// vs frequency:
//   slope = sum((F - mean(F)) .* (X - mean(X))) / sum((F - mean(F))^2)
Value spectralSlope(std::pmr::memory_resource *mr, const Value &x, const Value &f)
{
    auto d = prepareInput(mr, x, f);
    const double *Fd = d.F.doubleData();
    return perColumn(mr, d.X, [Fd](const double *col, size_t M) {
        if (M < 2) return 0.0;
        double meanF = 0.0, meanX = 0.0;
        for (size_t i = 0; i < M; ++i) { meanF += Fd[i]; meanX += col[i]; }
        meanF /= static_cast<double>(M);
        meanX /= static_cast<double>(M);
        double num = 0.0, den = 0.0;
        for (size_t i = 0; i < M; ++i) {
            const double df = Fd[i] - meanF;
            num += df * (col[i] - meanX);
            den += df * df;
        }
        return (den > 0.0) ? num / den : 0.0;
    });
}

// ── Cycle I: Crest / Entropy / Flatness / Kurtosis / Skewness ─────────
//
// Match MATLAB R2025b Signal Toolbox semantics (per-frame for time-domain
// input via shared computeStft + prepareInput).

// spectralCrest = max(X) / mean(X) per column.
Value spectralCrest(std::pmr::memory_resource *mr, const Value &x, const Value &f)
{
    auto d = prepareInput(mr, x, f);
    return perColumn(mr, d.X, [](const double *col, size_t M) {
        if (M == 0) return 0.0;
        double mx = col[0], sm = 0.0;
        for (size_t i = 0; i < M; ++i) { if (col[i] > mx) mx = col[i]; sm += col[i]; }
        const double mean = sm / static_cast<double>(M);
        return (mean > 0.0) ? mx / mean : 0.0;
    });
}

// spectralEntropy = -Σ P log2(P) / log2(M) per column, where P = X / Σ X.
// Matches MATLAB R2025b spectralEntropy with default Scaled=true,
// Instantaneous=true.
Value spectralEntropy(std::pmr::memory_resource *mr, const Value &x, const Value &f)
{
    auto d = prepareInput(mr, x, f);
    return perColumn(mr, d.X, [](const double *col, size_t M) {
        if (M < 2) return 0.0;
        double sm = 0.0;
        for (size_t i = 0; i < M; ++i) sm += col[i];
        if (sm <= 0.0) return 0.0;
        double H = 0.0;
        const double inv_log2 = 1.0 / std::log(2.0);
        for (size_t i = 0; i < M; ++i) {
            const double q = col[i] / sm;
            if (q > 0.0) H -= q * std::log(q) * inv_log2;
        }
        return H / (std::log(static_cast<double>(M)) * inv_log2);
    });
}

// spectralFlatness = exp(mean(log(X+eps))) / mean(X) per column —
// geometric mean / arithmetic mean (Peeters 2004), with eps
// regularization inside the geometric-mean log.
Value spectralFlatness(std::pmr::memory_resource *mr, const Value &x, const Value &f)
{
    auto d = prepareInput(mr, x, f);
    return perColumn(mr, d.X, [](const double *col, size_t M) {
        if (M == 0) return 0.0;
        const double eps = std::numeric_limits<double>::epsilon();
        double sumLog = 0.0, sumX = 0.0;
        for (size_t i = 0; i < M; ++i) {
            sumLog += std::log(col[i] + eps);
            sumX   += col[i];
        }
        const double geom  = std::exp(sumLog / static_cast<double>(M));
        const double arith = sumX / static_cast<double>(M);
        return (arith > 0.0) ? geom / arith : 0.0;
    });
}

// spectralKurtosis = Σ((F-c)⁴ X) / (spread⁴ ΣX) per column — X-weighted
// 4th central frequency moment normalized by the 4th power of spread
// (Peeters 2004). NOT the Pearson form — no -3 subtraction.
Value spectralKurtosis(std::pmr::memory_resource *mr, const Value &x, const Value &f)
{
    auto d = prepareInput(mr, x, f);
    const double *Fd = d.F.doubleData();
    return perColumn(mr, d.X, [Fd](const double *col, size_t M) {
        if (M < 2) return 0.0;
        double sumX = 0.0, sumFX = 0.0;
        for (size_t i = 0; i < M; ++i) { sumX += col[i]; sumFX += Fd[i] * col[i]; }
        if (sumX <= 0.0) return 0.0;
        const double centroid = sumFX / sumX;
        double m2 = 0.0, m4 = 0.0;
        for (size_t i = 0; i < M; ++i) {
            const double dF = Fd[i] - centroid;
            const double dF2 = dF * dF;
            m2 += dF2 * col[i];
            m4 += dF2 * dF2 * col[i];
        }
        const double spread2 = m2 / sumX;
        if (spread2 <= 0.0) return 0.0;
        const double spread4 = spread2 * spread2;
        return m4 / (spread4 * sumX);
    });
}

// spectralSkewness = Σ((F-c)³ X) / (spread³ ΣX) per column — X-weighted
// 3rd central frequency moment normalized by spread³ (Peeters 2004).
Value spectralSkewness(std::pmr::memory_resource *mr, const Value &x, const Value &f)
{
    auto d = prepareInput(mr, x, f);
    const double *Fd = d.F.doubleData();
    return perColumn(mr, d.X, [Fd](const double *col, size_t M) {
        if (M < 2) return 0.0;
        double sumX = 0.0, sumFX = 0.0;
        for (size_t i = 0; i < M; ++i) { sumX += col[i]; sumFX += Fd[i] * col[i]; }
        if (sumX <= 0.0) return 0.0;
        const double centroid = sumFX / sumX;
        double m2 = 0.0, m3 = 0.0;
        for (size_t i = 0; i < M; ++i) {
            const double dF = Fd[i] - centroid;
            m2 += dF * dF * col[i];
            m3 += dF * dF * dF * col[i];
        }
        const double spread2 = m2 / sumX;
        if (spread2 <= 0.0) return 0.0;
        const double spread3 = spread2 * std::sqrt(spread2);
        return m3 / (spread3 * sumX);
    });
}

// ── Flux ──────────────────────────────────────────────────────────────
// Spectral flux (Peeters 2004) — per-frame metric of spectrum change.
// flux(t) = (Σ|X_t(k) - X_{t-1}(k)|^p)^(1/p) for k = 1..M.
// First frame compared against zero (=> result = ||X_1||_p).
Value spectralFlux(std::pmr::memory_resource *mr, const Value &x,
                   const Value &f, double p)
{
    auto d = prepareInput(mr, x, f);
    const size_t M = d.X.dims().rows();
    const size_t N = d.X.dims().cols();
    Value out = Value::matrix(N, N == 0 ? 0 : 1, ValueType::DOUBLE, mr);
    if (M == 0 || N == 0) return out;
    const double *Xd = d.X.doubleData();
    double *od = out.doubleDataMut();
    // MATLAB convention: first-frame flux = 0 (no previous frame).
    od[0] = 0.0;
    for (size_t n = 1; n < N; ++n) {
        double s = 0.0;
        for (size_t i = 0; i < M; ++i) {
            const double cur = Xd[i + n * M];
            const double prv = Xd[i + (n - 1) * M];
            const double diff = std::abs(cur - prv);
            s += std::pow(diff, p);
        }
        od[n] = std::pow(s, 1.0 / p);
    }
    return out;
}

namespace detail {

// Per-fn registration adapters share the (x, f) argument shape.
#define NK_SPEC_REG(FN)                                                          \
    void FN##_reg(Span<const Value> args, size_t /*nargout*/,                    \
                  Span<Value> outs, CallContext &ctx)                            \
    {                                                                            \
        if (args.size() < 2)                                                     \
            throw Error(#FN ": requires (x, fs) or (X, F)",                      \
                        0, 0, #FN, "", "numkit:" #FN ":nargin");                      \
        outs[0] = FN(ctx.engine->resource(), args[0], args[1]);                  \
    }

NK_SPEC_REG(spectralCentroid)
NK_SPEC_REG(spectralSpread)
NK_SPEC_REG(spectralDecrease)
NK_SPEC_REG(spectralSlope)
NK_SPEC_REG(spectralCrest)
NK_SPEC_REG(spectralEntropy)
NK_SPEC_REG(spectralFlatness)
NK_SPEC_REG(spectralKurtosis)
NK_SPEC_REG(spectralSkewness)

#undef NK_SPEC_REG

void spectralRolloffPoint_reg(Span<const Value> args, size_t /*nargout*/,
                              Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("spectralRolloffPoint: requires (x, fs [, threshold])",
                    0, 0, "spectralRolloffPoint", "",
                    "numkit:spectralRolloffPoint:nargin");
    double pct = 0.95;
    if (args.size() >= 3) pct = args[2].toScalar();
    outs[0] = spectralRolloffPoint(ctx.engine->resource(),
                                    args[0], args[1], pct);
}

void spectralFlux_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("spectralFlux: requires (x, fs [, p])",
                    0, 0, "spectralFlux", "", "numkit:spectralFlux:nargin");
    double p = 2.0;
    if (args.size() >= 3) p = args[2].toScalar();
    outs[0] = spectralFlux(ctx.engine->resource(), args[0], args[1], p);
}

} // namespace detail

} // namespace numkit::audio
