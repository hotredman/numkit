// libs/signal/src/transforms/dct.cpp
//
// Orthonormal Type-II DCT + inverse (MATLAB default). FFT-based
// O(N log N) implementation using the "double-length mirror" trick:
// extend x to a length-2N mirror y, take FFT(y), recover the DCT
// coefficients from Re(Y * exp(-jπk/(2N))). Replaces the prior
// O(N²) direct path (closed gap to MATLAB; bench reports ~50× win
// vs prior numkit and beats Octave on small N).

#include <numkit/signal/transforms/dct.hpp>
#include <numkit/signal/transforms/fft.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

using Cd = std::complex<double>;

inline bool isPowerOfTwo(size_t n) { return n != 0 && (n & (n - 1)) == 0; }

// Direct O(N²) reference implementation. Used as a fallback when N
// isn't a power of two (numkit's fft pads non-pow2 inputs to the next
// pow2 internally and slices the result, which doesn't match a true
// length-N DFT — needed for the FFT-trick DCT to be exact).
Value dctDirect(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    auto r = createLike(x, ValueType::DOUBLE, mr);
    if (N == 0) return r;
    const double *xd = x.doubleData();
    double *X  = r.doubleDataMut();
    const double w0 = std::sqrt(1.0 / static_cast<double>(N));
    const double wk = std::sqrt(2.0 / static_cast<double>(N));
    const double piOver2N = M_PI / (2.0 * static_cast<double>(N));
    for (size_t k = 0; k < N; ++k) {
        double acc = 0.0;
        const double angK = piOver2N * static_cast<double>(k);
        for (size_t n = 0; n < N; ++n)
            acc += xd[n] * std::cos(angK * static_cast<double>(2 * n + 1));
        X[k] = (k == 0 ? w0 : wk) * acc;
    }
    return r;
}

Value idctDirect(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    auto r = createLike(x, ValueType::DOUBLE, mr);
    if (N == 0) return r;
    const double *Xd = x.doubleData();
    double *xt = r.doubleDataMut();
    const double w0 = std::sqrt(1.0 / static_cast<double>(N));
    const double wk = std::sqrt(2.0 / static_cast<double>(N));
    const double piOver2N = M_PI / (2.0 * static_cast<double>(N));
    for (size_t n = 0; n < N; ++n) {
        double acc = w0 * Xd[0];
        const double angN = piOver2N * static_cast<double>(2 * n + 1);
        for (size_t k = 1; k < N; ++k)
            acc += wk * Xd[k] * std::cos(angN * static_cast<double>(k));
        xt[n] = acc;
    }
    return r;
}

} // anonymous

// dct:  X[k] = w[k] * sum_n x[n] * cos(pi (2n+1) k / (2N))
// idct: x[n] = sum_k w[k] * X[k] * cos(pi (2n+1) k / (2N))
//   where w[0] = sqrt(1/N), w[k>0] = sqrt(2/N).
//
// Algorithm: build y of length 2N by mirroring x (y[n] = x[n] for
// n < N, y[2N-1-n] = x[n] for n < N), take FFT(y), then
// X[k] = Re(Y[k] · exp(-jπk/(2N))) / 2 with the orthonormal weights.
Value dct(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    auto r = createLike(x, ValueType::DOUBLE, mr);
    if (N == 0) return r;
    const double *xd = x.doubleData();
    double *X  = r.doubleDataMut();

    if (N == 1) {
        X[0] = xd[0];  // sqrt(1/1) * x[0]
        return r;
    }
    // numkit's fft pads non-pow2 inputs to nextPow2 internally, so the
    // length-N output isn't a true length-N DFT for N≠pow2. Fall back
    // to the direct path in that case; modern bench inputs (image
    // tiling, audio frames, etc.) are typically pow2 so the FFT fast
    // path covers the common case.
    if (!isPowerOfTwo(N)) return dctDirect(x, mr);

    // Build the length-2N mirrored signal y as a numkit Value so we
    // can call the existing FFT.
    const size_t M = 2 * N;
    auto y = Value::matrix(M, 1, ValueType::DOUBLE, mr);
    double *yd = y.doubleDataMut();
    for (size_t n = 0; n < N; ++n) {
        yd[n]         = xd[n];
        yd[M - 1 - n] = xd[n];
    }
    Value Y = fft(y, /*n=*/-1, /*dim=*/0, mr);
    const Cd *Yc = Y.complexData();

    const double w0 = std::sqrt(1.0 / static_cast<double>(N));
    const double wk = std::sqrt(2.0 / static_cast<double>(N));
    const double piOver2N = M_PI / (2.0 * static_cast<double>(N));

    // Standard DCT-II from FFT: X[k] = w[k] · Re(Y[k] · exp(-jπk/(2N))) / 2
    // where Y is the FFT of the length-2N mirrored signal. Uses the
    // textbook sign convention (matches MATLAB's fft).
    for (size_t k = 0; k < N; ++k) {
        const double phase = -piOver2N * static_cast<double>(k);
        const Cd rot(std::cos(phase), std::sin(phase));
        const double v = (Yc[k] * rot).real() * 0.5;
        X[k] = (k == 0 ? w0 : wk) * v;
    }
    return r;
}

// Inverse DCT-II. Reuse the dct → ifft trick: build a length-2N
// complex spectrum Z from the orthonormalised coefficients X such that
// Z[k] = X[k]·exp(jπk/(2N)) for k = 0..N-1, Z[N] = 0, and the
// remaining bins are conjugate-symmetric. Inverse FFT gives the
// length-2N mirrored signal whose first N samples are x.
Value idct(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    auto r = createLike(x, ValueType::DOUBLE, mr);
    if (N == 0) return r;
    const double *Xd = x.doubleData();
    double *xt = r.doubleDataMut();

    if (N == 1) {
        xt[0] = Xd[0];
        return r;
    }
    if (!isPowerOfTwo(N)) return idctDirect(x, mr);

    const double w0 = std::sqrt(1.0 / static_cast<double>(N));
    const double wk = std::sqrt(2.0 / static_cast<double>(N));
    const double piOver2N = M_PI / (2.0 * static_cast<double>(N));

    // Build length-2N complex spectrum.
    const size_t M = 2 * N;
    auto Zv = Value::complexMatrix(M, 1, mr);
    Cd *Z = Zv.complexDataMut();
    for (size_t i = 0; i < M; ++i) Z[i] = Cd(0.0, 0.0);

    // Build the conjugate-symmetric length-2N spectrum for ifft:
    //   Z[k]      =  Vk · exp(+jπk/(2N))           , k = 0..N-1
    //   Z[N]      =  0                              (zero by symmetry)
    //   Z[2N - k] = conj(Z[k])                     , k = 1..N-1
    // (textbook DCT inverse derivation; matches MATLAB's ifft).
    for (size_t k = 0; k < N; ++k) {
        const double Vk = ((k == 0) ? Xd[0] / w0 : Xd[k] / wk) * 2.0;
        const double phase = piOver2N * static_cast<double>(k);
        const Cd rot(std::cos(phase), std::sin(phase));
        Z[k] = Vk * rot;
    }
    for (size_t k = 1; k < N; ++k) Z[M - k] = std::conj(Z[k]);

    Value Y = ifft(Zv, /*n=*/-1, /*dim=*/0, mr);
    // y is real; first N samples are the recovered signal x.
    const double *yd = (Y.type() == ValueType::COMPLEX)
                       ? nullptr  // need real path below
                       : Y.doubleData();
    if (yd) {
        std::memcpy(xt, yd, N * sizeof(double));
    } else {
        const Cd *Yc = Y.complexData();
        for (size_t n = 0; n < N; ++n) xt[n] = Yc[n].real();
    }
    return r;
}

// ── Matrix / length-override / dim wrappers ──────────────────────────
//
// Strategy: extract one "line" (column when dim=1, row when dim=2),
// optionally pad/truncate to length n, run the existing 1-D dct/idct
// core, then write the result back at the appropriate stride.

namespace {

// Extract a column (`dim`=1) or row (`dim`=2) into a contiguous length-N
// 1-D Value, padding with zeros / truncating to `nOut`.
Value extractLine(const Value &x, int dim, size_t lineIdx, size_t nNative, size_t nOut, std::pmr::memory_resource *mr)
{
    auto col = Value::matrix(nOut, 1, ValueType::DOUBLE, mr);
    double *dst = col.doubleDataMut();
    const double *src = x.doubleData();
    const size_t R = x.dims().rows();
    const size_t copyN = std::min(nOut, nNative);
    if (dim == 1) {
        // Column-major: column lineIdx starts at offset lineIdx*R.
        const double *colSrc = src + lineIdx * R;
        for (size_t i = 0; i < copyN; ++i) dst[i] = colSrc[i];
    } else {  // dim == 2
        // Row lineIdx: stride is R between consecutive elements.
        for (size_t i = 0; i < copyN; ++i) dst[i] = src[lineIdx + i * R];
    }
    for (size_t i = copyN; i < nOut; ++i) dst[i] = 0.0;
    return col;
}

// Write a transformed 1-D length-`nOut` line into the (dim, lineIdx)
// slot of the destination matrix.
void writeLine(Value &dst, const Value &line, int dim, size_t lineIdx)
{
    double *outd = dst.doubleDataMut();
    const double *src = line.doubleData();
    const size_t R = dst.dims().rows();
    const size_t nOut = line.numel();
    if (dim == 1) {
        double *colDst = outd + lineIdx * R;
        for (size_t i = 0; i < nOut; ++i) colDst[i] = src[i];
    } else {
        for (size_t i = 0; i < nOut; ++i) outd[lineIdx + i * R] = src[i];
    }
}

// Resolve dim: 0 means "first non-singleton".
int resolveDim(const Value &x, int dim)
{
    if (dim != 0) return dim;
    return (x.dims().rows() > 1) ? 1 : 2;  // matches MATLAB default
}

} // anonymous

Value dct(const Value &x, int n, int dim, std::pmr::memory_resource *mr)
{
    if (x.numel() == 0) return createLike(x, ValueType::DOUBLE, mr);
    const int d = resolveDim(x, dim);
    const size_t R = x.dims().rows();
    const size_t C = x.dims().cols();
    const size_t nNative = (d == 1) ? R : C;
    const size_t nOut    = (n > 0) ? static_cast<size_t>(n) : nNative;
    const size_t nLines  = (d == 1) ? C : R;
    Value out;
    if (d == 1) out = Value::matrix(nOut, C, ValueType::DOUBLE, mr);
    else        out = Value::matrix(R, nOut, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < nLines; ++i) {
        Value line = extractLine(x, d, i, nNative, nOut, mr);
        Value tr   = dct(line, mr);  // 1-D core
        writeLine(out, tr, d, i);
    }
    return out;
}

Value idct(const Value &x, int n, int dim, std::pmr::memory_resource *mr)
{
    if (x.numel() == 0) return createLike(x, ValueType::DOUBLE, mr);
    const int d = resolveDim(x, dim);
    const size_t R = x.dims().rows();
    const size_t C = x.dims().cols();
    const size_t nNative = (d == 1) ? R : C;
    const size_t nOut    = (n > 0) ? static_cast<size_t>(n) : nNative;
    const size_t nLines  = (d == 1) ? C : R;
    Value out;
    if (d == 1) out = Value::matrix(nOut, C, ValueType::DOUBLE, mr);
    else        out = Value::matrix(R, nOut, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < nLines; ++i) {
        Value line = extractLine(x, d, i, nNative, nOut, mr);
        Value tr   = idct(line, mr);
        writeLine(out, tr, d, i);
    }
    return out;
}

namespace detail {

// Helper: detect whether the user passed a length override / dim arg.
// MATLAB syntax: dct(X), dct(X, n), dct(X, n, dim). 'Type' name-value
// is parsed separately (currently not implemented; explicit error to
// surface the unsupported branch instead of silently doing Type-II).
struct DctArgs { int n; int dim; bool hasType; double typeVal; };
static DctArgs parseDctArgs(Span<const Value> args, const char *fn)
{
    DctArgs a{0, 0, false, 2.0};
    size_t pos = 1;
    while (pos < args.size()) {
        if (args[pos].isChar() || args[pos].isString()) {
            if (pos + 1 >= args.size())
                throw Error(std::string(fn) + ": missing value after name",
                             0, 0, fn, "", "numkit:dct:nv");
            const std::string key = args[pos].toString();
            if (key == "Type" || key == "type") {
                a.hasType = true;
                a.typeVal = args[pos + 1].toScalar();
            }
            // Unknown N-V keys are silently ignored.
            pos += 2;
            continue;
        }
        if (pos == 1) a.n = static_cast<int>(args[pos].toScalar());
        else if (pos == 2) a.dim = static_cast<int>(args[pos].toScalar());
        else
            throw Error(std::string(fn) + ": too many positional arguments",
                         0, 0, fn, "", "numkit:dct:nargin");
        ++pos;
    }
    return a;
}

void dct_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw Error("dct: requires at least 1 argument",
                     0, 0, "dct", "", "numkit:dct:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        const auto &x = args[0];
        if (x.dims().rows() > 1 && x.dims().cols() > 1) {
            outs[0] = dct(x, /*n=*/0, /*dim=*/0, mr);  // matrix column-wise
        } else {
            outs[0] = dct(x, mr);  // 1-D fast path
        }
        return;
    }
    auto a = parseDctArgs(args, "dct");
    if (a.hasType && a.typeVal != 2.0)
        throw Error("dct: 'Type' values other than 2 are not yet implemented",
                     0, 0, "dct", "", "numkit:dct:type");
    outs[0] = dct(args[0], a.n, a.dim, mr);
}

void idct_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("idct: requires at least 1 argument",
                     0, 0, "idct", "", "numkit:idct:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        const auto &x = args[0];
        if (x.dims().rows() > 1 && x.dims().cols() > 1) {
            outs[0] = idct(x, /*n=*/0, /*dim=*/0, mr);
        } else {
            outs[0] = idct(x, mr);
        }
        return;
    }
    auto a = parseDctArgs(args, "idct");
    if (a.hasType && a.typeVal != 2.0)
        throw Error("idct: 'Type' values other than 2 are not yet implemented",
                     0, 0, "idct", "", "numkit:idct:type");
    outs[0] = idct(args[0], a.n, a.dim, mr);
}

} // namespace detail

} // namespace numkit::signal
