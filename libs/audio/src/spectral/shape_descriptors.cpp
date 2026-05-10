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

// Naive DFT. O(N²) per frame; acceptable for winLen ≤ 1024 typical of
// Audio Toolbox defaults (winLen = round(0.03*fs) = 240 for fs=8 kHz,
// 1323 for fs=44.1 kHz). For larger windows, swap to libs/signal FFT.
void naiveDFTPower(const double *x, size_t N, double *out_pow_half)
{
    // out_pow_half is length N/2+1 (one-sided power spectrum).
    const size_t H = N / 2 + 1;
    for (size_t k = 0; k < H; ++k) {
        double re = 0.0, im = 0.0;
        const double w = -2.0 * M_PI * static_cast<double>(k) / static_cast<double>(N);
        for (size_t n = 0; n < N; ++n) {
            const double a = w * static_cast<double>(n);
            re += x[n] * std::cos(a);
            im += x[n] * std::sin(a);
        }
        out_pow_half[k] = re * re + im * im;
    }
}

struct Stft {
    Value X;   // M × N, column-major; M = winLen/2+1, N = numFrames
    Value F;   // M × 1, frequency axis in Hz
};

// Build STFT power matrix using MATLAB Audio Toolbox defaults:
//   window=rectwin(round(0.03*fs)), overlap=round(0.02*fs), FFTLen=winLen.
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

    ScratchArena scratch(mr);
    ScratchVec<double> frame(winLen, &scratch);
    double *Xd = s.X.doubleDataMut();
    for (size_t f = 0; f < numFrames; ++f) {
        const size_t start = f * hop;
        for (size_t i = 0; i < winLen; ++i) frame[i] = x.elemAsDouble(start + i);
        naiveDFTPower(frame.data(), winLen, Xd + f * M);
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
// Linear regression slope of mean-power-magnitude X vs frequency.
// MATLAB R2025b spectralSlope formula (see source spectralSlope.m):
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

// ── Flux ──────────────────────────────────────────────────────────────
// MATLAB R2025b: per-frame metric of frame-to-frame spectrum change.
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
                        0, 0, #FN, "", "m:" #FN ":nargin");                      \
        outs[0] = FN(ctx.engine->resource(), args[0], args[1]);                  \
    }

NK_SPEC_REG(spectralCentroid)
NK_SPEC_REG(spectralSpread)
NK_SPEC_REG(spectralDecrease)
NK_SPEC_REG(spectralSlope)

#undef NK_SPEC_REG

void spectralRolloffPoint_reg(Span<const Value> args, size_t /*nargout*/,
                              Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("spectralRolloffPoint: requires (x, fs [, threshold])",
                    0, 0, "spectralRolloffPoint", "",
                    "m:spectralRolloffPoint:nargin");
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
                    0, 0, "spectralFlux", "", "m:spectralFlux:nargin");
    double p = 2.0;
    if (args.size() >= 3) p = args[2].toScalar();
    outs[0] = spectralFlux(ctx.engine->resource(), args[0], args[1], p);
}

} // namespace detail

} // namespace numkit::audio
