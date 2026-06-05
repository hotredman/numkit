// libs/signal/src/convolution/convolution.cpp
//
// Public C++ API for convolution and friends. See convolution.hpp for
// contracts. Algorithms unchanged from the previous lambda form — only
// moved into named free functions that take std::pmr::memory_resource* explicitly.

#include <numkit/signal/convolution/convolution.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value_type.hpp>

#include "../dsp_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <memory_resource>

namespace numkit::signal {

// ── conv ──────────────────────────────────────────────────────────────
// Choose direct vs FFT convolution by ACTUAL cost, not na·nb alone. Direct
// costs na·nb MACs; FFT costs ~K transforms of the zero-padded length. The
// old `na·nb > T²` rule wrongly chose FFT for a long vector × tiny kernel
// (e.g. 1e6 × 3 → a 1e6-point FFT for a 3-tap conv), ~100× slower than direct.
inline bool conv_use_fft(size_t na, size_t nb)
{
    if (na == 0 || nb == 0) return false;
    const double direct = static_cast<double>(na) * static_cast<double>(nb);
    size_t need = na + nb - 1, nfft = 1;
    while (nfft < need) nfft <<= 1;
    const double fft = 6.0 * static_cast<double>(nfft) *
                       std::log2(static_cast<double>(nfft < 2 ? 2 : nfft));
    return direct > fft;
}

// MATLAB conv promotes integer/logical inputs to double — the result is
// always double, never the integer class (unlike kron/cross). Promote up
// front so the real fast-path's doubleData() accessors are valid.
static Value convPromoteToDouble(const Value &v, std::pmr::memory_resource *mr)
{
    const auto &d = v.dims();
    Value r = d.is3D() ? Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr)
                       : Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    const size_t n = v.numel();
    double *dst = r.doubleDataMut();
    for (size_t i = 0; i < n; ++i) dst[i] = v.elemAsDouble(i);
    return r;
}

Value conv(const Value &aIn, const Value &bIn, const std::string &shape, std::pmr::memory_resource *mr)
{
    auto needsPromote = [](const Value &v) {
        return !v.isComplex() && (v.isLogical() || isIntegerType(v.type()));
    };
    Value aHold, bHold;
    const bool aProm = needsPromote(aIn);
    const bool bProm = needsPromote(bIn);
    if (aProm) aHold = convPromoteToDouble(aIn, mr);
    if (bProm) bHold = convPromoteToDouble(bIn, mr);
    const Value &a = aProm ? aHold : aIn;
    const Value &b = bProm ? bHold : bIn;

    const size_t na = a.numel(), nb = b.numel();

    // Complex inputs: convolution is BILINEAR, so the real/imag split used for
    // the linear ops does NOT apply — do a genuine complex multiply-accumulate
    // full[n] = sum_k a[k]·b[n-k] (direct; correctness over an FFT path), then
    // the same shape trim. Handles complex×complex and complex×real.
    if (a.isComplex() || b.isComplex()) {
        ScratchArena cscratch(mr);
        const size_t nfull = (na == 0 || nb == 0) ? 0 : na + nb - 1;
        auto toC = [&](const Value &v, size_t n) {
            ScratchVec<Complex> out(n, &cscratch);
            if (v.isComplex()) {
                const Complex *cd = v.complexData();
                for (size_t i = 0; i < n; ++i) out[i] = cd[i];
            } else {
                for (size_t i = 0; i < n; ++i) out[i] = Complex(v.elemAsDouble(i), 0.0);
            }
            return out;
        };
        auto av = toC(a, na);
        auto bv = toC(b, nb);
        ScratchVec<Complex> full(nfull, &cscratch);
        for (size_t i = 0; i < nfull; ++i) full[i] = Complex(0.0, 0.0);
        for (size_t i = 0; i < na; ++i)
            for (size_t j = 0; j < nb; ++j)
                full[i + j] += av[i] * bv[j];

        size_t outStartC = 0, outLenC = nfull;
        if (shape == "same") { outLenC = na; outStartC = nb / 2; }
        else if (shape == "valid") {
            outLenC = (na >= nb) ? na - nb + 1 : nb - na + 1;
            outStartC = std::min(na, nb) - 1;
        } else if (shape != "full") {
            throw Error("conv: shape must be 'full', 'same', or 'valid'",
                         0, 0, "conv", "", "numkit:conv:badShape");
        }
        auto rc = Value::matrix(1, outLenC, ValueType::COMPLEX, mr);
        Complex *rcd = rc.complexDataMut();
        for (size_t i = 0; i < outLenC; ++i) rcd[i] = full[outStartC + i];
        return rc;
    }

    ScratchArena scratch(mr);
    auto c = conv_use_fft(na, nb)
        ? convFFT  (&scratch, a.doubleData(), na, b.doubleData(), nb)
        : convDirect(&scratch, a.doubleData(), na, b.doubleData(), nb);

    const size_t nc = c.size();
    size_t outStart = 0, outLen = nc;
    if (shape == "same") {
        // MATLAB: 'same' is the central part the SAME SIZE AS THE FIRST input
        // (length na), taken starting at floor(nb/2) (0-based) of the full
        // convolution. conv([1 2 3 4],[1 1],'same')=[3 5 7 4] (not [1 3 5 7]);
        // conv([1 2],[1 1 1 1 1],'same')=[3 3] (length 2, not 5).
        outLen = na;
        outStart = nb / 2;
    } else if (shape == "valid") {
        outLen = (na >= nb) ? na - nb + 1 : nb - na + 1;
        outStart = std::min(na, nb) - 1;
    } else if (shape != "full") {
        throw Error("conv: shape must be 'full', 'same', or 'valid'",
                     0, 0, "conv", "", "numkit:conv:badShape");
    }

    auto r = Value::matrix(1, outLen, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < outLen; ++i)
        r.doubleDataMut()[i] = c[outStart + i];
    return r;
}

// ── deconv ────────────────────────────────────────────────────────────
std::tuple<Value, Value>
deconv(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    const size_t nb = b.numel(), na = a.numel();
    if (na > nb) {
        // MATLAB: a divisor longer than the dividend can't be divided once —
        // quotient is the scalar 0, remainder is the numerator unchanged.
        auto qv = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        qv.doubleDataMut()[0] = 0.0;
        auto rv = Value::matrix(1, nb, ValueType::DOUBLE, mr);
        const double *bd = b.doubleData();
        for (size_t i = 0; i < nb; ++i) rv.doubleDataMut()[i] = bd[i];
        return std::make_tuple(std::move(qv), std::move(rv));
    }

    ScratchArena scratch(mr);
    ScratchVec<double> rem(b.doubleData(), b.doubleData() + nb, &scratch);
    const double *ad = a.doubleData();

    const size_t nq = nb - na + 1;
    auto q = ScratchVec<double>(nq, &scratch);

    const double a0 = ad[0];
    if (a0 == 0.0)
        throw Error("deconv: leading coefficient is zero",
                     0, 0, "deconv", "", "numkit:deconv:zeroLead");

    for (size_t i = 0; i < nq; ++i) {
        q[i] = rem[i] / a0;
        for (size_t j = 0; j < na; ++j)
            rem[i + j] -= q[i] * ad[j];
    }

    auto qv = Value::matrix(1, nq, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < nq; ++i)
        qv.doubleDataMut()[i] = q[i];

    auto rv = Value::matrix(1, nb, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < nb; ++i)
        rv.doubleDataMut()[i] = rem[i];

    return std::make_tuple(std::move(qv), std::move(rv));
}

// ── xcorr ─────────────────────────────────────────────────────────────
std::tuple<Value, Value>
xcorr(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    const double *xd = x.doubleData();
    const size_t nx = x.numel();
    const double *yd = y.doubleData();
    const size_t ny = y.numel();

    const size_t maxLen = std::max(nx, ny);
    const size_t nc = nx + ny - 1;

    ScratchArena scratch(mr);
    auto yRev = ScratchVec<double>(ny, &scratch);
    for (size_t i = 0; i < ny; ++i)
        yRev[i] = yd[ny - 1 - i];

    auto c = conv_use_fft(nx, ny)
        ? convFFT  (&scratch, xd, nx, yRev.data(), ny)
        : convDirect(&scratch, xd, nx, yRev.data(), ny);

    auto r = Value::matrix(1, nc, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < nc; ++i)
        r.doubleDataMut()[i] = c[i];

    const int maxLag = static_cast<int>(maxLen) - 1;
    auto lags = Value::matrix(1, nc, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < nc; ++i)
        lags.doubleDataMut()[i] = static_cast<double>(static_cast<int>(i) - maxLag);

    return std::make_tuple(std::move(r), std::move(lags));
}

// ── Pack 36: xcov ────────────────────────────────────────────────────
// Cross-covariance = xcorr of the mean-removed signals, with MATLAB's
// scaleopt and maxlag handling:
//   'none'     (default) raw sum
//   'biased'   divide every lag by N
//   'unbiased' divide lag m by (N - |m|)
//   'coeff'    divide by sqrt(Cxx(0)*Cyy(0)) so the auto-cov peak is 1
// where N = max(numel(x), numel(y)). maxlag < 0 means "full" (N-1);
// otherwise the result is cropped (or zero-padded) to lags -maxlag..maxlag.
std::tuple<Value, Value>
xcov(const Value &x, const Value &y, int maxlag,
     const std::string &scaleopt, std::pmr::memory_resource *mr)
{
    auto center = [mr](const Value &v) -> Value {
        const size_t n = v.numel();
        Value c = Value::matrix(1, n, ValueType::DOUBLE, mr);
        if (n == 0) return c;
        const double *vd = v.doubleData();
        double sum = 0.0;
        for (size_t i = 0; i < n; ++i) sum += vd[i];
        const double m = sum / static_cast<double>(n);
        double *cd = c.doubleDataMut();
        for (size_t i = 0; i < n; ++i) cd[i] = vd[i] - m;
        return c;
    };
    Value xc = center(x);
    Value yc = center(y);
    const size_t nx = xc.numel(), ny = yc.numel();
    const size_t N  = std::max(nx, ny);

    auto [cfull, lagsfull] = xcorr(xc, yc, mr);
    const size_t nc = cfull.numel();
    double *cd = cfull.doubleDataMut();
    const double *ld = lagsfull.doubleData();

    // Case-insensitive scaleopt.
    std::string opt = scaleopt;
    for (char &ch : opt) if (ch >= 'A' && ch <= 'Z') ch = char(ch + 32);

    if (opt == "biased") {
        const double inv = (N > 0) ? 1.0 / static_cast<double>(N) : 0.0;
        for (size_t i = 0; i < nc; ++i) cd[i] *= inv;
    } else if (opt == "unbiased") {
        for (size_t i = 0; i < nc; ++i) {
            const double div = static_cast<double>(N) - std::abs(ld[i]);
            cd[i] = (div > 0.0) ? cd[i] / div : 0.0;
        }
    } else if (opt == "coeff" || opt == "normalized") {
        double c0x = 0.0, c0y = 0.0;
        const double *xd = xc.doubleData();
        const double *yd = yc.doubleData();
        for (size_t i = 0; i < nx; ++i) c0x += xd[i] * xd[i];
        for (size_t i = 0; i < ny; ++i) c0y += yd[i] * yd[i];
        const double denom = std::sqrt(c0x * c0y);
        if (denom > 0.0)
            for (size_t i = 0; i < nc; ++i) cd[i] /= denom;
    } else if (!(opt.empty() || opt == "none")) {
        throw Error("xcov: scaleopt must be 'none', 'biased', 'unbiased', or 'coeff'",
                     0, 0, "xcov", "", "numkit:xcov:badScaleopt");
    }

    const int fullMaxLag = (N > 0) ? static_cast<int>(N) - 1 : 0;
    if (maxlag < 0) maxlag = fullMaxLag;
    if (maxlag == fullMaxLag)
        return std::make_tuple(std::move(cfull), std::move(lagsfull));

    // Crop (maxlag < full) or zero-pad (maxlag > full) about lag 0, which
    // sits at index fullMaxLag in the full vector.
    const int center0 = fullMaxLag;
    const int outLen = 2 * maxlag + 1;
    Value cOut = Value::matrix(1, outLen, ValueType::DOUBLE, mr);
    Value lOut = Value::matrix(1, outLen, ValueType::DOUBLE, mr);
    double *co = cOut.doubleDataMut();
    double *lo = lOut.doubleDataMut();
    for (int m = -maxlag; m <= maxlag; ++m) {
        const int src = center0 + m;
        const int dst = m + maxlag;
        lo[dst] = static_cast<double>(m);
        co[dst] = (src >= 0 && src < static_cast<int>(nc)) ? cd[src] : 0.0;
    }
    return std::make_tuple(std::move(cOut), std::move(lOut));
}

// ── Pack 36: conv2 / filter2 / convn ─────────────────────────────────
namespace {

// Direct 2-D convolution into a `(M+P-1) × (N+Q-1)` "full" output buffer.
// A is M×N (column-major, src1), B is P×Q (column-major, src2).
// Output `out` must be sized M+P-1 by N+Q-1, column-major.
void conv2Direct(const double *A, size_t M, size_t N,
                 const double *B, size_t P, size_t Q,
                 double *out)
{
    const size_t outR = M + P - 1;
    const size_t outC = N + Q - 1;
    std::fill_n(out, outR * outC, 0.0);
    for (size_t j = 0; j < Q; ++j) {
        for (size_t i = 0; i < P; ++i) {
            const double bij = B[j * P + i];
            if (bij == 0.0) continue;
            for (size_t cc = 0; cc < N; ++cc) {
                const size_t outCol = j + cc;
                for (size_t rr = 0; rr < M; ++rr) {
                    out[outCol * outR + (i + rr)] += A[cc * M + rr] * bij;
                }
            }
        }
    }
}

// Crop a "full" 2-D conv result to MATLAB's "same" or "valid" shape.
// Returns a fresh Value sized appropriately. fullR/fullC are dims of
// the full result; A is the first input (size M×N), B is the second
// (size P×Q).
Value cropConv2(const double *full, size_t fullR, size_t fullC, size_t M, size_t N, size_t P, size_t Q, const std::string &shape, std::pmr::memory_resource *mr)
{
    if (shape == "full") {
        auto out = Value::matrix(fullR, fullC, ValueType::DOUBLE, mr);
        std::memcpy(out.doubleDataMut(), full, fullR * fullC * sizeof(double));
        return out;
    }
    if (shape == "same") {
        // Center-crop to size of A. MATLAB picks r0 = floor(P/2),
        // c0 = floor(Q/2) (verified empirically against R2025b for
        // both even and odd P, Q).
        const size_t outR = M, outC = N;
        const size_t r0 = P / 2;
        const size_t c0 = Q / 2;
        auto out = Value::matrix(outR, outC, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t cc = 0; cc < outC; ++cc)
            for (size_t rr = 0; rr < outR; ++rr)
                od[cc * outR + rr] = full[(cc + c0) * fullR + (rr + r0)];
        return out;
    }
    if (shape == "valid") {
        // Output is max(M-P+1, 0) × max(N-Q+1, 0).
        const size_t outR = (M >= P) ? M - P + 1 : 0;
        const size_t outC = (N >= Q) ? N - Q + 1 : 0;
        auto out = Value::matrix(outR, outC, ValueType::DOUBLE, mr);
        if (outR == 0 || outC == 0) return out;
        double *od = out.doubleDataMut();
        const size_t r0 = P - 1;
        const size_t c0 = Q - 1;
        for (size_t cc = 0; cc < outC; ++cc)
            for (size_t rr = 0; rr < outR; ++rr)
                od[cc * outR + rr] = full[(cc + c0) * fullR + (rr + r0)];
        return out;
    }
    throw Error("conv2: shape must be 'full', 'same', or 'valid'",
                 0, 0, "conv2", "", "numkit:conv2:badShape");
}

void requireDouble2D(const Value &v, const char *name)
{
    if (v.type() != ValueType::DOUBLE)
        throw Error(std::string(name) + ": only DOUBLE inputs are supported",
                     0, 0, name, "", std::string("numkit:") + name + ":notDouble");
    if (v.dims().ndim() > 2)
        throw Error(std::string(name) + ": input must be 1-D or 2-D",
                     0, 0, name, "", std::string("numkit:") + name + ":nd");
}

} // namespace

Value conv2(const Value &A, const Value &B, const std::string &shape, std::pmr::memory_resource *mr)
{
    requireDouble2D(A, "conv2");
    requireDouble2D(B, "conv2");

    const size_t M = A.dims().rows(), N = A.dims().cols();
    const size_t P = B.dims().rows(), Q = B.dims().cols();
    if (M == 0 || N == 0 || P == 0 || Q == 0) {
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    }

    const size_t fullR = M + P - 1, fullC = N + Q - 1;
    ScratchArena scratch(mr);
    ScratchVec<double> full(fullR * fullC, &scratch);
    conv2Direct(A.doubleData(), M, N, B.doubleData(), P, Q, full.data());

    return cropConv2(full.data(), fullR, fullC, M, N, P, Q, shape, mr);
}

Value filter2(const Value &h, const Value &X, const std::string &shape, std::pmr::memory_resource *mr)
{
    requireDouble2D(h, "filter2");
    requireDouble2D(X, "filter2");

    // filter2(h, X, shape) = conv2(X, rot90(h, 2), shape).
    // rot90(h, 2) flips rows + cols: out[i,j] = h[P-1-i, Q-1-j].
    const size_t P = h.dims().rows(), Q = h.dims().cols();
    auto hf = Value::matrix(P, Q, ValueType::DOUBLE, mr);
    const double *src = h.doubleData();
    double *dst       = hf.doubleDataMut();
    for (size_t j = 0; j < Q; ++j)
        for (size_t i = 0; i < P; ++i)
            dst[j * P + i] = src[(Q - 1 - j) * P + (P - 1 - i)];
    return conv2(X, hf, shape, mr);
}

Value convn(const Value &A, const Value &B, const std::string &shape, std::pmr::memory_resource *mr)
{
    if (A.type() != ValueType::DOUBLE || B.type() != ValueType::DOUBLE)
        throw Error("convn: only DOUBLE inputs are supported",
                     0, 0, "convn", "", "numkit:convn:notDouble");
    const int da = A.dims().ndim(), db = B.dims().ndim();
    const int nd = std::max(da, db);
    if (nd <= 1) {
        return conv(A, B, shape, mr);
    }
    if (nd == 2) {
        return conv2(A, B, shape, mr);
    }
    if (nd != 3) {
        throw Error("convn: only 1-D, 2-D, 3-D inputs supported",
                     0, 0, "convn", "", "numkit:convn:nd");
    }
    // 3-D: direct nested-loop convolution.
    const size_t M = A.dims().rows(), N = A.dims().cols();
    const size_t Mp = (A.dims().ndim() == 3) ? A.dims().pages() : 1;
    const size_t P = B.dims().rows(), Q = B.dims().cols();
    const size_t Pp = (B.dims().ndim() == 3) ? B.dims().pages() : 1;
    const size_t outR = M + P - 1, outC = N + Q - 1, outP = Mp + Pp - 1;
    if (M == 0 || N == 0 || P == 0 || Q == 0 || Mp == 0 || Pp == 0) {
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    }

    auto outV = Value::matrix3d(outR, outC, outP, ValueType::DOUBLE, mr);
    double *out = outV.doubleDataMut();
    const size_t pageStrideOut = outR * outC;
    const size_t pageStrideA   = M * N;
    const size_t pageStrideB   = P * Q;
    const double *Ad = A.doubleData();
    const double *Bd = B.doubleData();
    std::fill_n(out, outR * outC * outP, 0.0);

    for (size_t bp = 0; bp < Pp; ++bp) {
        for (size_t ap = 0; ap < Mp; ++ap) {
            const size_t op = ap + bp;
            for (size_t bj = 0; bj < Q; ++bj) {
                for (size_t bi = 0; bi < P; ++bi) {
                    const double bv = Bd[bp * pageStrideB + bj * P + bi];
                    if (bv == 0.0) continue;
                    for (size_t ac = 0; ac < N; ++ac) {
                        for (size_t ar = 0; ar < M; ++ar) {
                            const double av = Ad[ap * pageStrideA + ac * M + ar];
                            out[op * pageStrideOut
                                + (ac + bj) * outR
                                + (ar + bi)] += av * bv;
                        }
                    }
                }
            }
        }
    }

    if (shape == "full") return outV;
    if (shape == "same") {
        // Center-crop to A's shape. floor(P/2) offset matches MATLAB's
        // conv2 / convn 'same' rule for both even and odd kernel sizes.
        const size_t r0 = P  / 2;
        const size_t c0 = Q  / 2;
        const size_t p0 = Pp / 2;
        auto crop = Value::matrix3d(M, N, Mp, ValueType::DOUBLE, mr);
        double *cd = crop.doubleDataMut();
        for (size_t pp = 0; pp < Mp; ++pp)
            for (size_t cc = 0; cc < N; ++cc)
                for (size_t rr = 0; rr < M; ++rr)
                    cd[pp * M * N + cc * M + rr] =
                        out[(pp + p0) * pageStrideOut
                            + (cc + c0) * outR
                            + (rr + r0)];
        return crop;
    }
    if (shape == "valid") {
        const size_t outR2 = (M >= P)  ? M  - P  + 1 : 0;
        const size_t outC2 = (N >= Q)  ? N  - Q  + 1 : 0;
        const size_t outP2 = (Mp >= Pp)? Mp - Pp + 1 : 0;
        if (outR2 == 0 || outC2 == 0 || outP2 == 0)
            return Value::matrix(0, 0, ValueType::DOUBLE, mr);
        auto crop = Value::matrix3d(outR2, outC2, outP2, ValueType::DOUBLE, mr);
        double *cd = crop.doubleDataMut();
        for (size_t pp = 0; pp < outP2; ++pp)
            for (size_t cc = 0; cc < outC2; ++cc)
                for (size_t rr = 0; rr < outR2; ++rr)
                    cd[pp * outR2 * outC2 + cc * outR2 + rr] =
                        out[(pp + Pp - 1) * pageStrideOut
                            + (cc + Q - 1) * outR
                            + (rr + P - 1)];
        return crop;
    }
    throw Error("convn: shape must be 'full', 'same', or 'valid'",
                 0, 0, "convn", "", "numkit:convn:badShape");
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void conv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("conv: requires at least 2 arguments",
                     0, 0, "conv", "", "numkit:conv:nargin");

    std::string shape = "full";
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString()))
        shape = args[2].toString();

    outs[0] = conv(args[0], args[1], shape, ctx.engine->resource());
}

void deconv_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("deconv: requires 2 arguments",
                     0, 0, "deconv", "", "numkit:deconv:nargin");

    auto [q, r] = deconv(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(q);
    if (nargout > 1)
        outs[1] = std::move(r);
}

void xcorr_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("xcorr: requires at least 1 argument",
                     0, 0, "xcorr", "", "numkit:xcorr:nargin");
    auto *mr = ctx.engine->resource();
    const Value &x = args[0];
    Value y = x;                 // default: autocorrelation
    bool haveY = false;
    int maxlag = -1;             // -1 => full
    std::string scaleopt = "none";

    // Disambiguate (MATLAB): a string is scaleopt; a scalar numeric in the
    // y-slot is maxlag (autocorr); a vector numeric is y. Trailing args:
    // numeric => maxlag, string => scaleopt.
    size_t idx = 1;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].isChar() || args[1].isString())
            scaleopt = args[1].toString();
        else if (args[1].numel() == 1)
            maxlag = static_cast<int>(args[1].toScalar());
        else { y = args[1]; haveY = true; }
        idx = 2;
    }
    for (; idx < args.size(); ++idx) {
        if (args[idx].isEmpty()) continue;
        if (args[idx].isChar() || args[idx].isString())
            scaleopt = args[idx].toString();
        else
            maxlag = static_cast<int>(args[idx].toScalar());
    }

    auto [cfull, lagsfull] = haveY ? xcorr(x, y, mr) : xcorr(x, mr);
    const size_t nc = cfull.numel();
    double *cd = cfull.doubleDataMut();
    const double *ld = lagsfull.doubleData();
    const size_t nx = x.numel(), ny = y.numel();
    const size_t N  = std::max(nx, ny);

    // scaleopt (case-insensitive). Previously accepted-and-ignored.
    std::string opt = scaleopt;
    for (char &ch : opt) if (ch >= 'A' && ch <= 'Z') ch = char(ch + 32);

    if (opt == "biased") {
        const double inv = (N > 0) ? 1.0 / static_cast<double>(N) : 0.0;
        for (size_t i = 0; i < nc; ++i) cd[i] *= inv;
    } else if (opt == "unbiased") {
        for (size_t i = 0; i < nc; ++i) {
            const double div = static_cast<double>(N) - std::abs(ld[i]);
            cd[i] = (div > 0.0) ? cd[i] / div : 0.0;
        }
    } else if (opt == "coeff" || opt == "normalized") {
        // Normalize so an autocorrelation has 1.0 at lag 0:
        // divide by sqrt(Rxx(0) * Ryy(0)) = sqrt(sum x^2 * sum y^2).
        double c0x = 0.0, c0y = 0.0;
        const double *xd = x.doubleData();
        const double *yd = y.doubleData();
        for (size_t i = 0; i < nx; ++i) c0x += xd[i] * xd[i];
        for (size_t i = 0; i < ny; ++i) c0y += yd[i] * yd[i];
        const double denom = std::sqrt(c0x * c0y);
        if (denom > 0.0)
            for (size_t i = 0; i < nc; ++i) cd[i] /= denom;
    } else if (!(opt.empty() || opt == "none")) {
        throw Error("xcorr: scaleopt must be 'none', 'biased', 'unbiased', or 'coeff'",
                     0, 0, "xcorr", "", "numkit:xcorr:badScaleopt");
    }

    // maxlag crop (or zero-pad) about lag 0 at index fullMaxLag.
    const int fullMaxLag = (N > 0) ? static_cast<int>(N) - 1 : 0;
    if (maxlag < 0) maxlag = fullMaxLag;
    if (maxlag != fullMaxLag) {
        const int center0 = fullMaxLag;
        const int outLen = 2 * maxlag + 1;
        Value cOut = Value::matrix(1, outLen, ValueType::DOUBLE, mr);
        Value lOut = Value::matrix(1, outLen, ValueType::DOUBLE, mr);
        double *co = cOut.doubleDataMut();
        double *lo = lOut.doubleDataMut();
        for (int m = -maxlag; m <= maxlag; ++m) {
            const int src = center0 + m;
            const int dst = m + maxlag;
            lo[dst] = static_cast<double>(m);
            co[dst] = (src >= 0 && src < static_cast<int>(nc)) ? cd[src] : 0.0;
        }
        cfull = std::move(cOut);
        lagsfull = std::move(lOut);
    }

    outs[0] = std::move(cfull);
    if (nargout > 1) outs[1] = std::move(lagsfull);
}

void xcov_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("xcov: requires at least 1 argument",
                     0, 0, "xcov", "", "numkit:xcov:nargin");
    auto *mr = ctx.engine->resource();
    const Value &x = args[0];
    Value y = x;                 // default: auto-covariance
    int maxlag = -1;             // -1 => full
    std::string scaleopt = "none";

    // MATLAB disambiguation: a string arg is scaleopt; a scalar numeric
    // arg in the y-slot is maxlag (auto-cov); a vector numeric is y.
    size_t idx = 1;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].isChar() || args[1].isString()) {
            scaleopt = args[1].toString();
        } else if (args[1].numel() == 1) {
            maxlag = static_cast<int>(args[1].toScalar());
        } else {
            y = args[1];
        }
        idx = 2;
    }
    // Trailing args: numeric => maxlag, string => scaleopt.
    for (; idx < args.size(); ++idx) {
        if (args[idx].isEmpty()) continue;
        if (args[idx].isChar() || args[idx].isString())
            scaleopt = args[idx].toString();
        else
            maxlag = static_cast<int>(args[idx].toScalar());
    }

    auto result = xcov(x, y, maxlag, scaleopt, mr);
    outs[0] = std::move(std::get<0>(result));
    if (nargout > 1) outs[1] = std::move(std::get<1>(result));
}

void conv2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("conv2: requires at least 2 arguments",
                     0, 0, "conv2", "", "numkit:conv2:nargin");
    std::string shape = "full";
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString()))
        shape = args[2].toString();
    outs[0] = conv2(args[0], args[1], shape, ctx.engine->resource());
}

void filter2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("filter2: requires at least 2 arguments (h, X)",
                     0, 0, "filter2", "", "numkit:filter2:nargin");
    std::string shape = "same";
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString()))
        shape = args[2].toString();
    outs[0] = filter2(args[0], args[1], shape, ctx.engine->resource());
}

void convn_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("convn: requires at least 2 arguments",
                     0, 0, "convn", "", "numkit:convn:nargin");
    std::string shape = "full";
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString()))
        shape = args[2].toString();
    outs[0] = convn(args[0], args[1], shape, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
