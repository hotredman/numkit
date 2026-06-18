// toolboxes/signal/src/transforms/extras.cpp
// dftmtx / bitrevorder / dst / idst / rceps / cceps / icceps.

#include <numkit/signal/transforms/extras.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>
#include <numkit/signal/filter_analysis/unwrap.hpp>      // (used externally; not needed here)
#include <numkit/signal/transforms/fft.hpp>

#include "../dsp_helpers.hpp"   // fftRadix2, fillFftTwiddles, nextPow2, Complex
#include "extras_detail.hpp"    // bitReverse (shared compute/register)

#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

// External linkage (declared in extras_detail.hpp): bit-reversal permutation
// index, shared by the engine-free compute (bitrevorder) and the CallContext
// register half (bitrevorder_reg's 2nd index output) in extras_reg.cpp.
size_t bitReverse(size_t v, size_t bits)
{
    size_t r = 0;
    for (size_t i = 0; i < bits; ++i) {
        r = (r << 1) | (v & 1);
        v >>= 1;
    }
    return r;
}

namespace {

bool isPow2(size_t n) { return n > 0 && (n & (n - 1)) == 0; }

// In-place DFT at the EXACT length n, matching fftRadix2's twiddle
// convention W[k] = exp(dir·2πi·k/N) (dir=-1 forward, dir=+1 inverse and
// unscaled). Radix-2 when n is a power of two; otherwise a direct O(n²)
// DFT. The cepstrum MUST transform at the signal length — zero-padding to
// a power of two corrupts log|X| at the interpolated near-zero bins (a
// padded spectral null sends log|X| to ~-690 and smears it across every
// output sample), so the historical nextPow2 path produced garbage for
// non-power-of-two lengths.
void dftExact(Complex *buf, size_t n, int dir, std::pmr::memory_resource *mr)
{
    if (n <= 1) return;
    if (isPow2(n)) { fftRadix2(mr, buf, n, dir); return; }
    ScratchArena arena(mr);
    auto out = ScratchVec<Complex>(n, &arena);
    const double base = static_cast<double>(dir) * 2.0 * M_PI / static_cast<double>(n);
    for (size_t k = 0; k < n; ++k) {
        Complex s(0.0, 0.0);
        for (size_t m = 0; m < n; ++m) {
            const double a = base * static_cast<double>(k) * static_cast<double>(m);
            s += buf[m] * Complex(std::cos(a), std::sin(a));
        }
        out[k] = s;
    }
    for (size_t i = 0; i < n; ++i) buf[i] = out[i];
}

// A real column (or row, to match the input orientation) of length n.
Value realLike(const Value &x, size_t n, std::pmr::memory_resource *mr)
{
    const bool isRow = (x.dims().rows() == 1 && x.dims().cols() > 1);
    return isRow ? Value::matrix(1, n, ValueType::DOUBLE, mr)
                 : Value::matrix(n, 1, ValueType::DOUBLE, mr);
}

} // namespace

// ── dftmtx ────────────────────────────────────────────────────────────
Value dftmtx(size_t N, std::pmr::memory_resource *mr)
{
    if (N == 0)
        throw Error("dftmtx: N must be positive",
                     0, 0, "dftmtx", "", "numkit:dftmtx:badN");
    auto out = Value::complexMatrix(N, N, mr);
    Complex *dst = out.complexDataMut();
    const double base = -2.0 * M_PI / static_cast<double>(N);
    for (size_t k = 0; k < N; ++k)
        for (size_t n = 0; n < N; ++n) {
            const double a = base * static_cast<double>(k) * static_cast<double>(n);
            dst[n * N + k] = Complex(std::cos(a), std::sin(a));   // column-major
        }
    return out;
}

// ── bitrevorder ───────────────────────────────────────────────────────
Value bitrevorder(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t n = x.numel();
    if (!isPow2(n))
        throw Error("bitrevorder: input length must be a power of two",
                     0, 0, "bitrevorder", "", "numkit:bitrevorder:badLength");
    size_t bits = 0;
    for (size_t v = n; v > 1; v >>= 1) ++bits;

    const bool isRow = (x.dims().rows() == 1);
    auto out = isRow
                ? (x.isComplex()
                    ? Value::complexMatrix(1, n, mr)
                    : Value::matrix(1, n, ValueType::DOUBLE, mr))
                : (x.isComplex()
                    ? Value::complexMatrix(n, 1, mr)
                    : Value::matrix(n, 1, ValueType::DOUBLE, mr));
    if (x.isComplex()) {
        const Complex *src = x.complexData();
        Complex *dst = out.complexDataMut();
        for (size_t i = 0; i < n; ++i)
            dst[bitReverse(i, bits)] = src[i];
    } else {
        const double *src = x.doubleData();
        double *dst = out.doubleDataMut();
        for (size_t i = 0; i < n; ++i)
            dst[bitReverse(i, bits)] = src[i];
    }
    return out;
}

// ── dst / idst (Type-II) ──────────────────────────────────────────────
// Y[k] = sum x[n] sin(π (n+1) (k+1) / (N+1)),  k=0..N-1.
// Direct O(N²) — acceptable for N up to a few thousand.
Value dst(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 0) return out;
    double *dst = out.doubleDataMut();
    const double scale = M_PI / static_cast<double>(N + 1);
    for (size_t k = 0; k < N; ++k) {
        double s = 0.0;
        for (size_t n = 0; n < N; ++n)
            s += x.elemAsDouble(n) * std::sin(scale * (n + 1) * (k + 1));
        dst[k] = s;
    }
    return out;
}

Value idst(const Value &y, std::pmr::memory_resource *mr)
{
    // Type-II DST is self-inverse up to a factor of 2/(N+1).
    auto x = dst(y, mr);
    const size_t N = x.numel();
    if (N == 0) return x;
    const double scale = 2.0 / static_cast<double>(N + 1);
    double *p = x.doubleDataMut();
    for (size_t i = 0; i < N; ++i) p[i] *= scale;
    return x;
}

// ── rceps ─────────────────────────────────────────────────────────────
// Real cepstrum xhat = real(ifft(log|fft(x)|)), transformed at the exact
// signal length n (see dftExact).
Value rceps(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t n = x.numel();
    if (n == 0) return realLike(x, 0, mr);
    ScratchArena arena(mr);
    auto X = ScratchVec<Complex>(n, &arena);
    for (size_t i = 0; i < n; ++i) X[i] = Complex(x.elemAsDouble(i), 0.0);
    dftExact(X.data(), n, +1, mr);
    for (size_t i = 0; i < n; ++i) {
        const double mag = std::abs(X[i]);
        X[i] = Complex(std::log(std::max(mag, 1e-300)), 0.0);
    }
    dftExact(X.data(), n, -1, mr);
    const double invN = 1.0 / static_cast<double>(n);
    auto out = realLike(x, n, mr);
    double *dst = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i) dst[i] = X[i].real() * invN;
    return out;
}

// ── rceps (2-output) ───────────────────────────────────────────────────
// Also returns the minimum-phase reconstruction
//   ym = real(ifft(exp(fft(w .* xhat))))
// where w is the cepstral folding window [1, 2,…,2, (1 if n even), 0,…,0].
std::pair<Value, Value> rcepsMinPhase(const Value &x, std::pmr::memory_resource *mr)
{
    Value y = rceps(x, mr);
    const size_t n = x.numel();
    Value ym = realLike(x, n, mr);
    if (n == 0) return {std::move(y), std::move(ym)};
    const double *yd = y.doubleData();
    ScratchArena arena(mr);
    auto W = ScratchVec<Complex>(n, &arena);
    const size_t khalf = (n - 1) / 2;
    for (size_t i = 0; i < n; ++i) {
        double w;
        if (i == 0)                              w = 1.0;
        else if (i <= khalf)                     w = 2.0;
        else if (n % 2 == 0 && i == n / 2)       w = 1.0;   // Nyquist (even n)
        else                                     w = 0.0;
        W[i] = Complex(w * yd[i], 0.0);
    }
    dftExact(W.data(), n, -1, mr);                // fft
    for (size_t i = 0; i < n; ++i) W[i] = std::exp(W[i]);
    dftExact(W.data(), n, +1, mr);                // ifft (unscaled)
    const double invN = 1.0 / static_cast<double>(n);
    double *md = ym.doubleDataMut();
    for (size_t i = 0; i < n; ++i) md[i] = W[i].real() * invN;
    return {std::move(y), std::move(ym)};
}

// ── cceps ─────────────────────────────────────────────────────────────
// Complex cepstrum: ifft(log(fft(x))) with simple phase unwrapping along
// the frequency axis. Only the real part of x is used (matches MATLAB
// when input is real).
// Sign-convention note (numkit's fftRadix2 dir argument):
//   dir = -1  →  W[k] = exp(-2πj k/N)  →  FORWARD DFT
//   dir = +1  →  W[k] = exp(+2πj k/N)  →  INVERSE DFT (caller scales by 1/N)
// The inverse-step here MUST be dir=+1; using -1 produces a forward DFT
// which time-reverses the output .
Value cceps(const Value &x, std::pmr::memory_resource *mr, double *ndOut)
{
    const size_t n = x.numel();
    if (ndOut) *ndOut = 0.0;
    if (n == 0) return realLike(x, 0, mr);
    ScratchArena arena(mr);
    auto X = ScratchVec<Complex>(n, &arena);
    for (size_t i = 0; i < n; ++i) X[i] = Complex(x.elemAsDouble(i), 0.0);
    dftExact(X.data(), n, -1, mr);                  // FORWARD fft (h); the inverse
                                                    // step below is dir=+1. (Using
                                                    // +1 here too time-reverses the
                                                    // cepstrum and flips nd's sign.)
    // log magnitude + UNWRAPPED phase (relative to the previous bin = standard
    // unwrap), then apply MATLAB's rcunwrap linear-phase removal: the cepstrum
    // needs no phase discontinuity at ±pi, so subtract the integer-lag term
    //   nd = round(unwrapped(nh+1)/pi);  phase -= pi*nd*(0:n-1)/nh
    // with nh = fix((n+1)/2). `nd` is the (circular) delay removed (2nd output).
    // Without this term the per-sample result diverges past DC for non-2^n n
    // (see bugs/signal/cceps-nd-phase).
    auto ph   = ScratchVec<double>(n, &arena);
    auto lmag = ScratchVec<double>(n, &arena);
    double prevPhase = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double mag = std::abs(X[i]);
        double phase = std::arg(X[i]);
        while (phase - prevPhase > M_PI)  phase -= 2.0 * M_PI;
        while (phase - prevPhase < -M_PI) phase += 2.0 * M_PI;
        prevPhase = phase;
        ph[i]   = phase;
        lmag[i] = std::log(std::max(mag, 1e-300));
    }
    const size_t nh  = (n + 1) / 2;                 // fix((n+1)/2)
    const size_t idx = (n == 1) ? 0 : nh;           // MATLAB's 1-based nh+1
    const double nd  = std::round(ph[idx] / M_PI);
    for (size_t i = 0; i < n; ++i)
        ph[i] -= M_PI * nd * static_cast<double>(i) / static_cast<double>(nh);
    if (ndOut) *ndOut = nd;
    for (size_t i = 0; i < n; ++i) X[i] = Complex(lmag[i], ph[i]);
    dftExact(X.data(), n, +1, mr);
    const double invN = 1.0 / static_cast<double>(n);
    auto out = realLike(x, n, mr);
    double *dst = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i) dst[i] = X[i].real() * invN;
    return out;
}

// ── icceps ────────────────────────────────────────────────────────────
// Inverse complex cepstrum: real(ifft(exp(fft(c)))). FORWARD fft is dir=-1,
// the inverse pass dir=+1 — mirroring cceps. (Using +1 for both time-reverses
// the result, which the cceps fix exposed via the round-trip test. Without an
// `nd` argument this is MATLAB's icceps(xhat) with nd=0; it recovers cceps's
// input up to the circular shift cceps removed.)
Value icceps(const Value &c, std::pmr::memory_resource *mr)
{
    const size_t n = c.numel();
    if (n == 0) return realLike(c, 0, mr);
    ScratchArena arena(mr);
    auto X = ScratchVec<Complex>(n, &arena);
    for (size_t i = 0; i < n; ++i) X[i] = Complex(c.elemAsDouble(i), 0.0);
    dftExact(X.data(), n, -1, mr);                  // FORWARD fft
    for (size_t i = 0; i < n; ++i) X[i] = std::exp(X[i]);
    dftExact(X.data(), n, +1, mr);                  // inverse
    const double invN = 1.0 / static_cast<double>(n);
    auto out = realLike(c, n, mr);
    double *dst = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i) dst[i] = X[i].real() * invN;
    return out;
}

} // namespace numkit::signal
