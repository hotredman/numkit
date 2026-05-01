// libs/signal/src/windows/windows.cpp

#include <numkit/signal/windows/windows.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "dsp_helpers.hpp"  // fillFftTwiddles, fftRadix2, nextPow2

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

// Modified Bessel function of the first kind, order 0, via series expansion.
// Converges quickly for beta values typical of Kaiser window (0–20 range).
double besseli0(double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k <= 25; ++k) {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < 1e-16 * sum)
            break;
    }
    return sum;
}

// Chebyshev polynomial T_n(x) — uses cos for |x|<=1, cosh for |x|>1.
double chebyT(int n, double x)
{
    if (std::abs(x) <= 1.0)
        return std::cos(n * std::acos(x));
    if (x > 1.0)
        return std::cosh(n * std::acosh(x));
    // x < -1: T_n(-x) = (-1)^n T_n(x)
    const double v = std::cosh(n * std::acosh(-x));
    return (n & 1) ? -v : v;
}

} // anonymous namespace

// ── hamming ───────────────────────────────────────────────────────────
Value hamming(std::pmr::memory_resource *mr, size_t N)
{
    auto r = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        r.doubleDataMut()[0] = 1.0;
        return r;
    }
    for (size_t i = 0; i < N; ++i)
        r.doubleDataMut()[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (N - 1));
    return r;
}

// ── hann ──────────────────────────────────────────────────────────────
Value hann(std::pmr::memory_resource *mr, size_t N)
{
    auto r = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        r.doubleDataMut()[0] = 1.0;
        return r;
    }
    for (size_t i = 0; i < N; ++i)
        r.doubleDataMut()[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (N - 1)));
    return r;
}

// ── blackman ──────────────────────────────────────────────────────────
Value blackman(std::pmr::memory_resource *mr, size_t N)
{
    auto r = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        r.doubleDataMut()[0] = 1.0;
        return r;
    }
    for (size_t i = 0; i < N; ++i) {
        const double x = 2.0 * M_PI * i / (N - 1);
        r.doubleDataMut()[i] = 0.42 - 0.5 * std::cos(x) + 0.08 * std::cos(2.0 * x);
    }
    return r;
}

// ── kaiser ────────────────────────────────────────────────────────────
Value kaiser(std::pmr::memory_resource *mr, size_t N, double beta)
{
    auto r = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        r.doubleDataMut()[0] = 1.0;
        return r;
    }
    const double denom = besseli0(beta);
    for (size_t i = 0; i < N; ++i) {
        const double alpha = 2.0 * i / (N - 1) - 1.0;
        r.doubleDataMut()[i] = besseli0(beta * std::sqrt(1.0 - alpha * alpha)) / denom;
    }
    return r;
}

// ── rectwin ───────────────────────────────────────────────────────────
Value rectwin(std::pmr::memory_resource *mr, size_t N)
{
    auto r = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < N; ++i)
        r.doubleDataMut()[i] = 1.0;
    return r;
}

// ── bartlett ──────────────────────────────────────────────────────────
Value bartlett(std::pmr::memory_resource *mr, size_t N)
{
    auto r = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        r.doubleDataMut()[0] = 1.0;
        return r;
    }
    const double half = (N - 1) / 2.0;
    for (size_t i = 0; i < N; ++i)
        r.doubleDataMut()[i] = 1.0 - std::abs((i - half) / half);
    return r;
}

// ── triang ────────────────────────────────────────────────────────────
// Differs from bartlett: endpoints are non-zero. Even-N formula uses
// 1 - |2k - (N-1)| / N; odd-N uses 1 - |2k - (N-1)| / (N+1) (matches
// MATLAB's `triang` reference output).
Value triang(std::pmr::memory_resource *mr, size_t N)
{
    auto r = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        r.doubleDataMut()[0] = 1.0;
        return r;
    }
    double *dst = r.doubleDataMut();
    const double mid = (static_cast<double>(N) - 1.0) / 2.0;
    if (N % 2 == 0) {
        const double denom = static_cast<double>(N);
        for (size_t i = 0; i < N; ++i)
            dst[i] = 1.0 - std::abs((2.0 * i - (N - 1)) / denom);
    } else {
        const double denom = static_cast<double>(N + 1);
        for (size_t i = 0; i < N; ++i)
            dst[i] = 1.0 - std::abs(2.0 * (i - mid)) / denom;
    }
    return r;
}

// ── tukeywin ──────────────────────────────────────────────────────────
Value tukeywin(std::pmr::memory_resource *mr, size_t N, double r)
{
    if (r < 0.0) r = 0.0;
    if (r > 1.0) r = 1.0;
    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        out.doubleDataMut()[0] = 1.0;
        return out;
    }
    double *dst = out.doubleDataMut();
    if (r == 0.0) {
        for (size_t i = 0; i < N; ++i)
            dst[i] = 1.0;
        return out;
    }
    const double Nm1 = static_cast<double>(N - 1);
    const double tEdge = r * Nm1 * 0.5;       // taper region width on each side
    for (size_t i = 0; i < N; ++i) {
        const double x = static_cast<double>(i);
        if (x < tEdge) {
            dst[i] = 0.5 * (1.0 + std::cos(M_PI * (x / tEdge - 1.0)));
        } else if (x > Nm1 - tEdge) {
            dst[i] = 0.5 * (1.0 + std::cos(M_PI * ((x - Nm1) / tEdge + 1.0)));
        } else {
            dst[i] = 1.0;
        }
    }
    return out;
}

// ── flattopwin ────────────────────────────────────────────────────────
// Matlab "symmetric" 5-term coefficients (R2018b reference).
Value flattopwin(std::pmr::memory_resource *mr, size_t N)
{
    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        out.doubleDataMut()[0] = 1.0;
        return out;
    }
    constexpr double a0 = 0.21557895;
    constexpr double a1 = 0.41663158;
    constexpr double a2 = 0.277263158;
    constexpr double a3 = 0.083578947;
    constexpr double a4 = 0.006947368;
    double *dst = out.doubleDataMut();
    const double Nm1 = static_cast<double>(N - 1);
    for (size_t i = 0; i < N; ++i) {
        const double x = 2.0 * M_PI * i / Nm1;
        dst[i] = a0 - a1 * std::cos(x) + a2 * std::cos(2.0 * x)
                    - a3 * std::cos(3.0 * x) + a4 * std::cos(4.0 * x);
    }
    return out;
}

// ── gausswin ──────────────────────────────────────────────────────────
Value gausswin(std::pmr::memory_resource *mr, size_t N, double alpha)
{
    if (alpha <= 0)
        throw Error("gausswin: alpha must be positive",
                     0, 0, "gausswin", "", "m:gausswin:badAlpha");
    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        out.doubleDataMut()[0] = 1.0;
        return out;
    }
    double *dst = out.doubleDataMut();
    const double half = static_cast<double>(N - 1) / 2.0;
    for (size_t i = 0; i < N; ++i) {
        const double n = (static_cast<double>(i) - half) / half;
        dst[i] = std::exp(-0.5 * (alpha * n) * (alpha * n));
    }
    return out;
}

// ── chebwin ───────────────────────────────────────────────────────────
// Dolph-Chebyshev: equiripple sidelobes at `at` dB.
// Frequency-domain construction → IDFT → real part → fftshift →
// normalise to peak 1. `at` must be > 0.
Value chebwin(std::pmr::memory_resource *mr, size_t N, double at)
{
    if (at <= 0)
        throw Error("chebwin: at must be positive (sidelobe attenuation in dB)",
                     0, 0, "chebwin", "", "m:chebwin:badAt");
    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        out.doubleDataMut()[0] = 1.0;
        return out;
    }

    ScratchArena scratch(mr);
    const int order = static_cast<int>(N) - 1;
    const double R = std::pow(10.0, at / 20.0);                    // ripple ratio
    const double beta = std::cosh(std::acosh(R) / order);
    const size_t fftLen = nextPow2(N);

    auto W = ScratchVec<Complex>(fftLen, &scratch);
    // Build symmetric spectrum |W[k]| = T_M(beta·cos(πk/N)) / R, k=0..N-1.
    // For odd N use real samples; for even N alternate sign across k to
    // align the IFFT phase so the time-domain window comes out real.
    const bool nodd = (N % 2 != 0);
    for (size_t k = 0; k < N; ++k) {
        const double ck = beta * std::cos(M_PI * static_cast<double>(k) /
                                          static_cast<double>(N));
        double mag = chebyT(order, ck) / R;
        if (!nodd && (k & 1))
            mag = -mag;
        W[k] = Complex(mag, 0.0);
    }
    // Hermitian-mirror — W[N-k] = conj(W[k]) (real spectrum here).
    for (size_t k = N; k < fftLen; ++k)
        W[k] = Complex(0.0, 0.0);

    // Inverse FFT via conjugate trick: ifft(x) = conj(fft(conj(x))) / N
    auto Wt = ScratchVec<Complex>(fftLen / 2, &scratch);
    fillFftTwiddles(Wt.data(), fftLen, +1);
    for (size_t i = 0; i < fftLen; ++i)
        W[i] = std::conj(W[i]);
    fftRadix2(W.data(), fftLen, Wt.data());
    const double invN = 1.0 / static_cast<double>(fftLen);
    for (size_t i = 0; i < fftLen; ++i)
        W[i] = std::conj(W[i]) * invN;

    // The window is centered around 0 in the IFFT output. For odd N pull
    // the symmetric N samples as W[fftLen-N/2 .. fftLen-1] then W[0 .. N/2].
    // For even N the samples come from W[fftLen-N/2 .. fftLen-1], W[0 .. N/2 - 1].
    double *dst = out.doubleDataMut();
    if (nodd) {
        const size_t M = N / 2;          // = (N-1)/2
        for (size_t i = 0; i < M; ++i)
            dst[i] = W[fftLen - M + i].real();
        for (size_t i = 0; i <= M; ++i)
            dst[M + i] = W[i].real();
    } else {
        const size_t M = N / 2;
        for (size_t i = 0; i < M; ++i)
            dst[i] = W[fftLen - M + i].real();
        for (size_t i = 0; i < M; ++i)
            dst[M + i] = W[i].real();
    }

    // Normalise peak to 1.
    double peak = 0.0;
    for (size_t i = 0; i < N; ++i)
        peak = std::max(peak, std::abs(dst[i]));
    if (peak > 0.0) {
        const double inv = 1.0 / peak;
        for (size_t i = 0; i < N; ++i)
            dst[i] *= inv;
    }
    return out;
}

// ── parzenwin ─────────────────────────────────────────────────────────
// Piecewise cubic, defined symmetrically around the centre.
Value parzenwin(std::pmr::memory_resource *mr, size_t N)
{
    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        out.doubleDataMut()[0] = 1.0;
        return out;
    }
    double *dst = out.doubleDataMut();
    const double half = static_cast<double>(N) / 2.0;
    const double mid = (static_cast<double>(N) - 1.0) / 2.0;
    for (size_t i = 0; i < N; ++i) {
        const double n = std::abs(static_cast<double>(i) - mid);
        const double r = n / half;
        if (n <= half * 0.5) {
            dst[i] = 1.0 - 6.0 * r * r + 6.0 * r * r * r;
        } else {
            const double t = 1.0 - r;
            dst[i] = 2.0 * t * t * t;
        }
    }
    return out;
}

// ── nuttallwin ────────────────────────────────────────────────────────
// 4-term, "minimum 4-term" Nuttall coefficients (matches MATLAB default).
Value nuttallwin(std::pmr::memory_resource *mr, size_t N)
{
    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        out.doubleDataMut()[0] = 1.0;
        return out;
    }
    constexpr double a0 = 0.3635819;
    constexpr double a1 = 0.4891775;
    constexpr double a2 = 0.1365995;
    constexpr double a3 = 0.0106411;
    double *dst = out.doubleDataMut();
    const double Nm1 = static_cast<double>(N - 1);
    for (size_t i = 0; i < N; ++i) {
        const double x = 2.0 * M_PI * i / Nm1;
        dst[i] = a0 - a1 * std::cos(x) + a2 * std::cos(2.0 * x)
                    - a3 * std::cos(3.0 * x);
    }
    return out;
}

// ── taylorwin ─────────────────────────────────────────────────────────
Value taylorwin(std::pmr::memory_resource *mr, size_t N, int nbar, double sll)
{
    if (sll >= 0)
        throw Error("taylorwin: sll must be negative (peak sidelobe in dB)",
                     0, 0, "taylorwin", "", "m:taylorwin:badSll");
    if (nbar < 2)
        throw Error("taylorwin: nbar must be >= 2",
                     0, 0, "taylorwin", "", "m:taylorwin:badNbar");
    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        out.doubleDataMut()[0] = 1.0;
        return out;
    }
    const double R = std::pow(10.0, -sll / 20.0);
    const double A = std::log(R + std::sqrt(R * R - 1.0)) / M_PI;
    const double sigma2 = static_cast<double>(nbar) * nbar /
                          (A * A + (nbar - 0.5) * (nbar - 0.5));

    // Coefficients F_m, m = 1..nbar-1.
    auto Fm = std::vector<double>(static_cast<size_t>(nbar - 1), 0.0);
    for (int m = 1; m < nbar; ++m) {
        double num = 1.0, den = 1.0;
        for (int k = 1; k < nbar; ++k) {
            const double p = sigma2 * (A * A + (k - 0.5) * (k - 0.5));
            num *= 1.0 - static_cast<double>(m * m) / p;
        }
        for (int k = 1; k < nbar; ++k) {
            if (k == m) continue;
            den *= 1.0 - static_cast<double>(m * m) / static_cast<double>(k * k);
        }
        const double sign = (m & 1) ? -1.0 : 1.0;     // (-1)^m / 2
        Fm[m - 1] = sign * 0.5 * num / den;
    }

    double *dst = out.doubleDataMut();
    const double Nm1 = static_cast<double>(N - 1);
    for (size_t i = 0; i < N; ++i) {
        const double xn = (static_cast<double>(i) - Nm1 * 0.5);
        double w = 1.0;
        for (int m = 1; m < nbar; ++m)
            w += 2.0 * Fm[m - 1] * std::cos(2.0 * M_PI * m * xn / N);
        dst[i] = w;
    }

    // Normalise peak to 1.
    double peak = 0.0;
    for (size_t i = 0; i < N; ++i)
        peak = std::max(peak, std::abs(dst[i]));
    if (peak > 0.0) {
        const double inv = 1.0 / peak;
        for (size_t i = 0; i < N; ++i)
            dst[i] *= inv;
    }
    return out;
}

// ── blackmanharris ────────────────────────────────────────────────────
Value blackmanharris(std::pmr::memory_resource *mr, size_t N)
{
    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        out.doubleDataMut()[0] = 1.0;
        return out;
    }
    constexpr double a0 = 0.35875;
    constexpr double a1 = 0.48829;
    constexpr double a2 = 0.14128;
    constexpr double a3 = 0.01168;
    double *dst = out.doubleDataMut();
    const double Nm1 = static_cast<double>(N - 1);
    for (size_t i = 0; i < N; ++i) {
        const double x = 2.0 * M_PI * i / Nm1;
        dst[i] = a0 - a1 * std::cos(x) + a2 * std::cos(2.0 * x)
                    - a3 * std::cos(3.0 * x);
    }
    return out;
}

// ── bohmanwin ─────────────────────────────────────────────────────────
Value bohmanwin(std::pmr::memory_resource *mr, size_t N)
{
    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        out.doubleDataMut()[0] = 1.0;
        return out;
    }
    double *dst = out.doubleDataMut();
    const double half = (static_cast<double>(N) - 1.0) / 2.0;
    for (size_t i = 0; i < N; ++i) {
        const double x = std::abs((static_cast<double>(i) - half) / half);
        if (x >= 1.0) {
            dst[i] = 0.0;
        } else {
            dst[i] = (1.0 - x) * std::cos(M_PI * x)
                     + std::sin(M_PI * x) / M_PI;
        }
    }
    return out;
}

// ── barthannwin ───────────────────────────────────────────────────────
Value barthannwin(std::pmr::memory_resource *mr, size_t N)
{
    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        out.doubleDataMut()[0] = 1.0;
        return out;
    }
    double *dst = out.doubleDataMut();
    const double Nm1 = static_cast<double>(N - 1);
    for (size_t i = 0; i < N; ++i) {
        const double t = static_cast<double>(i) / Nm1 - 0.5;
        dst[i] = 0.62 - 0.48 * std::abs(t)
                 + 0.38 * std::cos(2.0 * M_PI * t);
    }
    return out;
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void hamming_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hamming: requires 1 argument",
                     0, 0, "hamming", "", "m:hamming:nargin");
    outs[0] = hamming(ctx.engine->resource(), static_cast<size_t>(args[0].toScalar()));
}

void hann_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hann: requires 1 argument",
                     0, 0, "hann", "", "m:hann:nargin");
    outs[0] = hann(ctx.engine->resource(), static_cast<size_t>(args[0].toScalar()));
}

void blackman_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("blackman: requires 1 argument",
                     0, 0, "blackman", "", "m:blackman:nargin");
    outs[0] = blackman(ctx.engine->resource(), static_cast<size_t>(args[0].toScalar()));
}

void kaiser_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("kaiser: requires at least 1 argument",
                     0, 0, "kaiser", "", "m:kaiser:nargin");
    const size_t N = static_cast<size_t>(args[0].toScalar());
    const double beta = (args.size() >= 2) ? args[1].toScalar() : 0.5;
    outs[0] = kaiser(ctx.engine->resource(), N, beta);
}

void rectwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rectwin: requires 1 argument",
                     0, 0, "rectwin", "", "m:rectwin:nargin");
    outs[0] = rectwin(ctx.engine->resource(), static_cast<size_t>(args[0].toScalar()));
}

void bartlett_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bartlett: requires 1 argument",
                     0, 0, "bartlett", "", "m:bartlett:nargin");
    outs[0] = bartlett(ctx.engine->resource(), static_cast<size_t>(args[0].toScalar()));
}

// Local helper: extract N from arg[0] with a `name` for error messages.
static size_t windowN(const Value &a, const char *name)
{
    return static_cast<size_t>(a.toScalar());
    (void)name; // reserved for future range checks
}

void triang_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("triang: requires 1 argument",
                     0, 0, "triang", "", "m:triang:nargin");
    outs[0] = triang(ctx.engine->resource(), windowN(args[0], "triang"));
}

void tukeywin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("tukeywin: requires at least 1 argument",
                     0, 0, "tukeywin", "", "m:tukeywin:nargin");
    const size_t N = windowN(args[0], "tukeywin");
    const double r = (args.size() >= 2) ? args[1].toScalar() : 0.5;
    outs[0] = tukeywin(ctx.engine->resource(), N, r);
}

void flattopwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("flattopwin: requires 1 argument",
                     0, 0, "flattopwin", "", "m:flattopwin:nargin");
    outs[0] = flattopwin(ctx.engine->resource(), windowN(args[0], "flattopwin"));
}

void gausswin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gausswin: requires at least 1 argument",
                     0, 0, "gausswin", "", "m:gausswin:nargin");
    const size_t N = windowN(args[0], "gausswin");
    const double alpha = (args.size() >= 2) ? args[1].toScalar() : 2.5;
    outs[0] = gausswin(ctx.engine->resource(), N, alpha);
}

void chebwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("chebwin: requires at least 1 argument",
                     0, 0, "chebwin", "", "m:chebwin:nargin");
    const size_t N = windowN(args[0], "chebwin");
    const double at = (args.size() >= 2) ? args[1].toScalar() : 100.0;
    outs[0] = chebwin(ctx.engine->resource(), N, at);
}

void parzenwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("parzenwin: requires 1 argument",
                     0, 0, "parzenwin", "", "m:parzenwin:nargin");
    outs[0] = parzenwin(ctx.engine->resource(), windowN(args[0], "parzenwin"));
}

void nuttallwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nuttallwin: requires 1 argument",
                     0, 0, "nuttallwin", "", "m:nuttallwin:nargin");
    outs[0] = nuttallwin(ctx.engine->resource(), windowN(args[0], "nuttallwin"));
}

void taylorwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("taylorwin: requires at least 1 argument",
                     0, 0, "taylorwin", "", "m:taylorwin:nargin");
    const size_t N = windowN(args[0], "taylorwin");
    const int nbar = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 4;
    const double sll = (args.size() >= 3) ? args[2].toScalar() : -30.0;
    outs[0] = taylorwin(ctx.engine->resource(), N, nbar, sll);
}

void blackmanharris_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("blackmanharris: requires 1 argument",
                     0, 0, "blackmanharris", "", "m:blackmanharris:nargin");
    outs[0] = blackmanharris(ctx.engine->resource(), windowN(args[0], "blackmanharris"));
}

void bohmanwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bohmanwin: requires 1 argument",
                     0, 0, "bohmanwin", "", "m:bohmanwin:nargin");
    outs[0] = bohmanwin(ctx.engine->resource(), windowN(args[0], "bohmanwin"));
}

void barthannwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("barthannwin: requires 1 argument",
                     0, 0, "barthannwin", "", "m:barthannwin:nargin");
    outs[0] = barthannwin(ctx.engine->resource(), windowN(args[0], "barthannwin"));
}

} // namespace detail

} // namespace numkit::signal
