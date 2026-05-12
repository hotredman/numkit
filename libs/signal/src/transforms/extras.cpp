// libs/signal/src/transforms/extras.cpp
//
// dftmtx / bitrevorder / dst / idst / rceps / cceps / icceps.

#include <numkit/signal/transforms/extras.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/signal/filter_analysis/unwrap.hpp>      // (used externally; not needed here)
#include <numkit/signal/transforms/fft.hpp>

#include "../dsp_helpers.hpp"   // fftRadix2, fillFftTwiddles, nextPow2, Complex

#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

bool isPow2(size_t n) { return n > 0 && (n & (n - 1)) == 0; }

size_t bitReverse(size_t v, size_t bits)
{
    size_t r = 0;
    for (size_t i = 0; i < bits; ++i) {
        r = (r << 1) | (v & 1);
        v >>= 1;
    }
    return r;
}

// fft of a real vector → complex column. Convenience wrapper that
// zero-pads x to nfft and runs the in-place radix-2 over a scratch
// arena. nfft must be a power of two.
ScratchVec<Complex> fftReal(const Value &x, size_t nfft, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto buf = ScratchVec<Complex>(nfft, &scratch);
    const size_t n = x.numel();
    for (size_t i = 0; i < n && i < nfft; ++i)
        buf[i] = Complex(x.elemAsDouble(i), 0.0);
    fftRadix2(mr, buf.data(), nfft, +1);
    // Move out of scratch into a heap vector tied to caller's mr so
    // the result outlives this function. (We can't return a ScratchVec
    // backed by a stack-local arena.)
    ScratchVec<Complex> out(nfft, mr);
    for (size_t i = 0; i < nfft; ++i) out[i] = buf[i];
    return out;
}

} // namespace

// ── dftmtx ────────────────────────────────────────────────────────────
Value dftmtx(size_t N, std::pmr::memory_resource *mr)
{
    if (N == 0)
        throw Error("dftmtx: N must be positive",
                     0, 0, "dftmtx", "", "m:dftmtx:badN");
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
                     0, 0, "bitrevorder", "", "m:bitrevorder:badLength");
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
Value rceps(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t n = x.numel();
    if (n == 0) return Value::matrix(0, 1, ValueType::DOUBLE, mr);
    const size_t nfft = nextPow2(n);
    auto X = fftReal(x, nfft, mr);
    // log |X|, set imag part to 0 so the inverse transform produces a real cepstrum.
    for (size_t i = 0; i < nfft; ++i) {
        const double mag = std::abs(X[i]);
        X[i] = Complex(std::log(std::max(mag, 1e-300)), 0.0);
    }
    fftRadix2(mr, X.data(), nfft, -1);    // inverse FFT (conjugate trick handled by sign)
    const double invN = 1.0 / static_cast<double>(nfft);
    auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i)
        dst[i] = X[i].real() * invN;
    return out;
}

// ── cceps ─────────────────────────────────────────────────────────────
// Complex cepstrum: ifft(log(fft(x))) with simple phase unwrapping along
// the frequency axis. Only the real part of x is used (matches MATLAB
// when input is real).
//
// Sign-convention note (numkit's fftRadix2 dir argument):
//   dir = -1  →  W[k] = exp(-2πj k/N)  →  FORWARD DFT
//   dir = +1  →  W[k] = exp(+2πj k/N)  →  INVERSE DFT (caller scales by 1/N)
// The inverse-step here MUST be dir=+1; using -1 produces a forward DFT
// which time-reverses the output (audit ТЗ signal/cceps, 2026-05-09).
Value cceps(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t n = x.numel();
    if (n == 0) return Value::matrix(0, 1, ValueType::DOUBLE, mr);
    const size_t nfft = nextPow2(n);
    auto X = fftReal(x, nfft, mr);
    // log + unwrap phase.
    double prevPhase = 0.0;
    for (size_t i = 0; i < nfft; ++i) {
        const double mag = std::abs(X[i]);
        double phase = std::arg(X[i]);
        // Unwrap relative to previous bin.
        while (phase - prevPhase > M_PI)  phase -= 2.0 * M_PI;
        while (phase - prevPhase < -M_PI) phase += 2.0 * M_PI;
        prevPhase = phase;
        X[i] = Complex(std::log(std::max(mag, 1e-300)), phase);
    }
    fftRadix2(mr, X.data(), nfft, +1);
    const double invN = 1.0 / static_cast<double>(nfft);
    auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i)
        dst[i] = X[i].real() * invN;
    return out;
}

// ── icceps ────────────────────────────────────────────────────────────
// Inverse complex cepstrum: ifft(exp(fft(c))). Same sign-convention fix
// as cceps — the second pass MUST be inverse (dir=+1).
Value icceps(const Value &c, std::pmr::memory_resource *mr)
{
    const size_t n = c.numel();
    if (n == 0) return Value::matrix(0, 1, ValueType::DOUBLE, mr);
    const size_t nfft = nextPow2(n);
    auto X = fftReal(c, nfft, mr);
    for (size_t i = 0; i < nfft; ++i)
        X[i] = std::exp(X[i]);
    fftRadix2(mr, X.data(), nfft, +1);
    const double invN = 1.0 / static_cast<double>(nfft);
    auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i)
        dst[i] = X[i].real() * invN;
    return out;
}

namespace detail {

void dftmtx_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dftmtx: requires N",
                     0, 0, "dftmtx", "", "m:dftmtx:nargin");
    outs[0] = dftmtx(static_cast<size_t>(args[0].toScalar()), ctx.engine->resource());
}

void bitrevorder_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bitrevorder: requires x",
                     0, 0, "bitrevorder", "", "m:bitrevorder:nargin");
    auto *mr = ctx.engine->resource();
    outs[0] = bitrevorder(args[0], mr);
    // 2nd output: 1-based index vector I such that Y(k) = X(I(k)).
    // Same permutation applied to (1:N), preserving the input shape.
    if (nargout > 1) {
        const size_t n = args[0].numel();
        if (n == 0) {
            outs[1] = Value::matrix(0, 0, ValueType::DOUBLE, mr);
            return;
        }
        size_t bits = 0;
        for (size_t v = n; v > 1; v >>= 1) ++bits;
        const bool isRow = (args[0].dims().rows() == 1);
        Value I = isRow
                    ? Value::matrix(1, n, ValueType::DOUBLE, mr)
                    : Value::matrix(n, 1, ValueType::DOUBLE, mr);
        double *id = I.doubleDataMut();
        for (size_t i = 0; i < n; ++i) {
            // dst[bitReverse(i, bits)] = i+1 (1-based MATLAB index).
            id[bitReverse(i, bits)] = static_cast<double>(i + 1);
        }
        outs[1] = std::move(I);
    }
}

void dst_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dst: requires x",
                     0, 0, "dst", "", "m:dst:nargin");
    outs[0] = dst(args[0], ctx.engine->resource());
}

void idst_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("idst: requires y",
                     0, 0, "idst", "", "m:idst:nargin");
    outs[0] = idst(args[0], ctx.engine->resource());
}

void rceps_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rceps: requires x",
                     0, 0, "rceps", "", "m:rceps:nargin");
    outs[0] = rceps(args[0], ctx.engine->resource());
}

void cceps_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cceps: requires x",
                     0, 0, "cceps", "", "m:cceps:nargin");
    outs[0] = cceps(args[0], ctx.engine->resource());
}

void icceps_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("icceps: requires c",
                     0, 0, "icceps", "", "m:icceps:nargin");
    outs[0] = icceps(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
