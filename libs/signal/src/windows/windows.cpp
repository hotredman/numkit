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
Value hamming(size_t N, std::pmr::memory_resource *mr)
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
Value hann(size_t N, std::pmr::memory_resource *mr)
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
Value blackman(size_t N, std::pmr::memory_resource *mr)
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
Value kaiser(size_t N, double beta, std::pmr::memory_resource *mr)
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
Value rectwin(size_t N, std::pmr::memory_resource *mr)
{
    auto r = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < N; ++i)
        r.doubleDataMut()[i] = 1.0;
    return r;
}

// ── bartlett ──────────────────────────────────────────────────────────
Value bartlett(size_t N, std::pmr::memory_resource *mr)
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
Value triang(size_t N, std::pmr::memory_resource *mr)
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
Value tukeywin(size_t N, double r, std::pmr::memory_resource *mr)
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
Value flattopwin(size_t N, std::pmr::memory_resource *mr)
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
Value gausswin(size_t N, double alpha, std::pmr::memory_resource *mr)
{
    if (alpha <= 0)
        throw Error("gausswin: alpha must be positive",
                     0, 0, "gausswin", "", "numkit:gausswin:badAlpha");
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
//
// Algorithm (matches MATLAB R2025b — direct cosine-IDFT form):
//   1. r    = 10^(at/20)                — ripple ratio
//   2. M    = N - 1                     — Chebyshev polynomial order
//   3. β    = cosh(acosh(r) / M)
//   4. Spectrum samples W(k) = T_M(β·cos(πk/N)) for k = 0..floor(N/2),
//      where T_n is computed branch-wise:
//        T_n(x) = cos (n·acos x)         for |x| ≤ 1
//        T_n(x) = cosh(n·acosh|x|)·sign(x)^n  for |x| > 1
//   5. Time-domain coefficients via the real cosine inverse:
//        w(n) = (1/N) · [W(0) + 2 · Σ_{k=1}^{K} W(k) · cos(2π·k·(n-N₀)/N)]
//      with K = floor((N-1)/2) and N₀ = (N-1)/2.
//      For even N, the k = N/2 term has T_M(0) = 0 (M is odd) so it is
//      naturally absent. The cosine basis with the (n-N₀) offset
//      produces a window symmetric about index N₀ (peak in the middle).
//   6. Normalise peak to 1.
//
// Direct O(N²) — windows are small (N ≤ ~few thousand). Replaces the
// previous FFT-based path that was numerically fragile on even N
// (degenerate all-ones output) due to a half-bin offset bug.
Value chebwin(size_t N, double at, std::pmr::memory_resource *mr)
{
    if (at <= 0)
        throw Error("chebwin: at must be positive (sidelobe attenuation in dB)",
                     0, 0, "chebwin", "", "numkit:chebwin:badAt");
    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 1) {
        out.doubleDataMut()[0] = 1.0;
        return out;
    }

    ScratchArena scratch(mr);
    const int M = static_cast<int>(N) - 1;
    const double r_lin = std::pow(10.0, at / 20.0);
    const double beta  = std::cosh(std::acosh(r_lin) / static_cast<double>(M));

    auto cheb_T_M = [M](double x) -> double {
        if (std::abs(x) <= 1.0)
            return std::cos(static_cast<double>(M) * std::acos(x));
        const double a = std::cosh(static_cast<double>(M) * std::acosh(std::abs(x)));
        return (x < 0 && (M & 1)) ? -a : a;
    };

    // Build positive-frequency spectrum W(k) for k = 0..K.
    const size_t K = (N - 1) / 2;  // floor((N-1)/2)
    auto Wp = ScratchVec<double>(K + 1, &scratch);
    for (size_t k = 0; k <= K; ++k) {
        const double ck = beta * std::cos(M_PI * static_cast<double>(k)
                                          / static_cast<double>(N));
        Wp[k] = cheb_T_M(ck);
    }

    // Real cosine-IDFT centered on N₀ = (N-1)/2.
    double *dst = out.doubleDataMut();
    const double N0 = 0.5 * static_cast<double>(N - 1);
    const double two_pi_over_N = 2.0 * M_PI / static_cast<double>(N);
    for (size_t n = 0; n < N; ++n) {
        double s = Wp[0];
        const double phase_n = (static_cast<double>(n) - N0);
        for (size_t k = 1; k <= K; ++k) {
            s += 2.0 * Wp[k] * std::cos(two_pi_over_N * static_cast<double>(k) * phase_n);
        }
        dst[n] = s / static_cast<double>(N);
    }

    // Normalise peak to 1.
    double peak = 0.0;
    for (size_t i = 0; i < N; ++i) peak = std::max(peak, std::abs(dst[i]));
    if (peak > 0.0) {
        const double inv = 1.0 / peak;
        for (size_t i = 0; i < N; ++i) dst[i] *= inv;
    }
    return out;
}

// ── parzenwin ─────────────────────────────────────────────────────────
// Piecewise cubic, defined symmetrically around the centre.
Value parzenwin(size_t N, std::pmr::memory_resource *mr)
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
Value nuttallwin(size_t N, std::pmr::memory_resource *mr)
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
//
// Taylor window — tapered weighting used in radar pulse-compression.
// Algorithm (matches MATLAB R2025b):
//   R     = 10^(-sll/20)
//   A     = (1/π) · acosh(R)
//   σ²    = nbar² / (A² + (nbar - 0.5)²)
//   F_m   = (-1)^(m+1) · prod_{n=1..nbar-1} (1 − m²/(σ²·(A² + (n-0.5)²)))
//                       ───────────────────────────────────────────────────
//                                    2 · prod_{n=1..nbar-1, n≠m} (1 − m²/n²)
//   w(i)  = 1 + 2 · Σ_{m=1..nbar-1} F_m · cos(2π·m·(i - (N-1)/2)/N)
// MATLAB does NOT normalise to peak=1 — peak depends on (nbar, sll).
//
// Bug fix 2026-05-08: previous impl used (-1)^m sign instead of
// (-1)^(m+1), inverting the window (peak at edges, dip at centre);
// also wrongly normalised peak to 1.
Value taylorwin(size_t N, int nbar, double sll, std::pmr::memory_resource *mr)
{
    if (sll >= 0)
        throw Error("taylorwin: sll must be negative (peak sidelobe in dB)",
                     0, 0, "taylorwin", "", "numkit:taylorwin:badSll");
    if (nbar < 2)
        throw Error("taylorwin: nbar must be >= 2",
                     0, 0, "taylorwin", "", "numkit:taylorwin:badNbar");
    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 0) return out;
    // Note: N=1 takes the full formula path — MATLAB returns
    // 1 + 2·Σ F_m for the single sample, NOT just 1.
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
        // (-1)^(m+1): m=1 -> +1, m=2 -> -1, m=3 -> +1, ...
        const double sign = (m & 1) ? +1.0 : -1.0;
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

    // (No peak normalisation — MATLAB taylorwin keeps the natural amplitude
    // determined by (nbar, sll). Peak ≈ 1.52 for the default (4, -30).)
    return out;
}

// ── blackmanharris ────────────────────────────────────────────────────
Value blackmanharris(size_t N, std::pmr::memory_resource *mr)
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
Value bohmanwin(size_t N, std::pmr::memory_resource *mr)
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
Value barthannwin(size_t N, std::pmr::memory_resource *mr)
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

namespace {

// Detect MATLAB's `'symmetric'` (default) / `'periodic'` window flag at
// the trailing argument position. Returns the effective positional arg
// count; sets `periodic = true` for the periodic form.
size_t parseSflag(Span<const Value> args, bool &periodic)
{
    periodic = false;
    if (args.empty()) return 0;
    const Value &last = args[args.size() - 1];
    if (!last.isChar() && !last.isString()) return args.size();
    std::string s = last.toString();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (s == "symmetric") return args.size() - 1;
    if (s == "periodic") { periodic = true; return args.size() - 1; }
    return args.size();   // not an sflag — leave for caller
}

// Periodic-form trick: for any window function f(N) computing the
// symmetric variant, the MATLAB periodic variant is the first N samples
// of f(N+1). This avoids modifying every window's implementation.
template <typename Fn>
Value applySflag(size_t N, bool periodic, Fn impl, std::pmr::memory_resource *mr)
{
    if (!periodic) return impl(N);
    Value full = impl(N + 1);
    Value out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N > 0)
        std::copy(full.doubleData(), full.doubleData() + N, out.doubleDataMut());
    return out;
}

// Some windows (bartlett, triang, parzenwin, bohmanwin, barthannwin,
// rectwin) accept ONLY a `typeName` flag ('double' / 'single') — they
// reject 'periodic' explicitly with the documented MATLAB error.
// `single` output cast is currently a no-op: numkit emits double
// regardless of the requested typeName.
void parseTypeNameOnly(Span<const Value> args, const char *fn)
{
    if (args.size() < 2) return;
    const Value &last = args[args.size() - 1];
    if (!last.isChar() && !last.isString()) return;
    std::string s = last.toString();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (s != "double" && s != "single")
        throw Error(std::string(fn) + ": Expected TYPENAME to match one of "
                    "these values: 'double', 'single' (got '" + s + "')",
                    0, 0, fn, "", std::string("numkit:") + fn + ":typeName");
}

} // anonymous

void hamming_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hamming: requires (N[, sflag])",
                     0, 0, "hamming", "", "numkit:hamming:nargin");
    bool periodic = false; (void)parseSflag(args, periodic);
    auto *mr = ctx.engine->resource();
    outs[0] = applySflag(static_cast<size_t>(args[0].toScalar()), periodic, [&](size_t M){ return hamming(M, mr); }, mr);
}

void hann_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hann: requires (N[, sflag])",
                     0, 0, "hann", "", "numkit:hann:nargin");
    bool periodic = false; (void)parseSflag(args, periodic);
    auto *mr = ctx.engine->resource();
    outs[0] = applySflag(static_cast<size_t>(args[0].toScalar()), periodic, [&](size_t M){ return hann(M, mr); }, mr);
}

void blackman_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("blackman: requires (N[, sflag])",
                     0, 0, "blackman", "", "numkit:blackman:nargin");
    bool periodic = false; (void)parseSflag(args, periodic);
    auto *mr = ctx.engine->resource();
    outs[0] = applySflag(static_cast<size_t>(args[0].toScalar()), periodic, [&](size_t M){ return blackman(M, mr); }, mr);
}

void kaiser_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("kaiser: requires at least 1 argument",
                     0, 0, "kaiser", "", "numkit:kaiser:nargin");
    const size_t N = static_cast<size_t>(args[0].toScalar());
    const double beta = (args.size() >= 2) ? args[1].toScalar() : 0.5;
    outs[0] = kaiser(N, beta, ctx.engine->resource());
}

void rectwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rectwin: requires (N[, typeName])",
                     0, 0, "rectwin", "", "numkit:rectwin:nargin");
    parseTypeNameOnly(args, "rectwin");
    outs[0] = rectwin(static_cast<size_t>(args[0].toScalar()), ctx.engine->resource());
}

void bartlett_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bartlett: requires (N[, typeName])",
                     0, 0, "bartlett", "", "numkit:bartlett:nargin");
    parseTypeNameOnly(args, "bartlett");
    outs[0] = bartlett(static_cast<size_t>(args[0].toScalar()), ctx.engine->resource());
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
        throw Error("triang: requires (N[, typeName])",
                     0, 0, "triang", "", "numkit:triang:nargin");
    parseTypeNameOnly(args, "triang");
    outs[0] = triang(windowN(args[0], "triang"), ctx.engine->resource());
}

void tukeywin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("tukeywin: requires at least 1 argument",
                     0, 0, "tukeywin", "", "numkit:tukeywin:nargin");
    const size_t N = windowN(args[0], "tukeywin");
    const double r = (args.size() >= 2) ? args[1].toScalar() : 0.5;
    outs[0] = tukeywin(N, r, ctx.engine->resource());
}

void flattopwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("flattopwin: requires (N[, sflag])",
                     0, 0, "flattopwin", "", "numkit:flattopwin:nargin");
    bool periodic = false; (void)parseSflag(args, periodic);
    auto *mr = ctx.engine->resource();
    outs[0] = applySflag(windowN(args[0], "flattopwin"), periodic, [&](size_t M){ return flattopwin(M, mr); }, mr);
}

void gausswin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gausswin: requires at least 1 argument",
                     0, 0, "gausswin", "", "numkit:gausswin:nargin");
    const size_t N = windowN(args[0], "gausswin");
    const double alpha = (args.size() >= 2) ? args[1].toScalar() : 2.5;
    outs[0] = gausswin(N, alpha, ctx.engine->resource());
}

void chebwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("chebwin: requires at least 1 argument",
                     0, 0, "chebwin", "", "numkit:chebwin:nargin");
    const size_t N = windowN(args[0], "chebwin");
    const double at = (args.size() >= 2) ? args[1].toScalar() : 100.0;
    outs[0] = chebwin(N, at, ctx.engine->resource());
}

void parzenwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("parzenwin: requires (N[, typeName])",
                     0, 0, "parzenwin", "", "numkit:parzenwin:nargin");
    parseTypeNameOnly(args, "parzenwin");
    outs[0] = parzenwin(windowN(args[0], "parzenwin"), ctx.engine->resource());
}

void nuttallwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nuttallwin: requires (N[, sflag])",
                     0, 0, "nuttallwin", "", "numkit:nuttallwin:nargin");
    bool periodic = false; (void)parseSflag(args, periodic);
    auto *mr = ctx.engine->resource();
    outs[0] = applySflag(windowN(args[0], "nuttallwin"), periodic, [&](size_t M){ return nuttallwin(M, mr); }, mr);
}

void taylorwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("taylorwin: requires at least 1 argument",
                     0, 0, "taylorwin", "", "numkit:taylorwin:nargin");
    const size_t N = windowN(args[0], "taylorwin");
    const int nbar = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 4;
    const double sll = (args.size() >= 3) ? args[2].toScalar() : -30.0;
    outs[0] = taylorwin(N, nbar, sll, ctx.engine->resource());
}

void blackmanharris_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("blackmanharris: requires (N[, sflag])",
                     0, 0, "blackmanharris", "", "numkit:blackmanharris:nargin");
    bool periodic = false; (void)parseSflag(args, periodic);
    auto *mr = ctx.engine->resource();
    outs[0] = applySflag(windowN(args[0], "blackmanharris"), periodic, [&](size_t M){ return blackmanharris(M, mr); }, mr);
}

void bohmanwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bohmanwin: requires (N[, typeName])",
                     0, 0, "bohmanwin", "", "numkit:bohmanwin:nargin");
    parseTypeNameOnly(args, "bohmanwin");
    outs[0] = bohmanwin(windowN(args[0], "bohmanwin"), ctx.engine->resource());
}

void barthannwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("barthannwin: requires (N[, typeName])",
                     0, 0, "barthannwin", "", "numkit:barthannwin:nargin");
    parseTypeNameOnly(args, "barthannwin");
    outs[0] = barthannwin(windowN(args[0], "barthannwin"), ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
