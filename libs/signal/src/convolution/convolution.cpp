// libs/signal/src/convolution/convolution.cpp
//
// Public C++ API for convolution and friends. See convolution.hpp for
// contracts. Algorithms unchanged from the previous lambda form — only
// moved into named free functions that take std::pmr::memory_resource* explicitly.

#include <numkit/signal/convolution/convolution.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "../dsp_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <memory_resource>

namespace numkit::signal {

// ── conv ──────────────────────────────────────────────────────────────
Value conv(std::pmr::memory_resource *mr, const Value &a, const Value &b, const std::string &shape)
{
    const size_t na = a.numel(), nb = b.numel();

    ScratchArena scratch(mr);
    auto c = (na * nb > CONV_FFT_THRESHOLD * CONV_FFT_THRESHOLD)
        ? convFFT  (&scratch, a.doubleData(), na, b.doubleData(), nb)
        : convDirect(&scratch, a.doubleData(), na, b.doubleData(), nb);

    const size_t nc = c.size();
    size_t outStart = 0, outLen = nc;
    if (shape == "same") {
        outLen = std::max(na, nb);
        outStart = (nc - outLen) / 2;
    } else if (shape == "valid") {
        outLen = (na >= nb) ? na - nb + 1 : nb - na + 1;
        outStart = std::min(na, nb) - 1;
    } else if (shape != "full") {
        throw Error("conv: shape must be 'full', 'same', or 'valid'",
                     0, 0, "conv", "", "m:conv:badShape");
    }

    auto r = Value::matrix(1, outLen, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < outLen; ++i)
        r.doubleDataMut()[i] = c[outStart + i];
    return r;
}

// ── deconv ────────────────────────────────────────────────────────────
std::tuple<Value, Value>
deconv(std::pmr::memory_resource *mr, const Value &b, const Value &a)
{
    const size_t nb = b.numel(), na = a.numel();
    if (na > nb)
        throw Error("deconv: denominator longer than numerator",
                     0, 0, "deconv", "", "m:deconv:denomTooLong");

    ScratchArena scratch(mr);
    ScratchVec<double> rem(b.doubleData(), b.doubleData() + nb, &scratch);
    const double *ad = a.doubleData();

    const size_t nq = nb - na + 1;
    auto q = ScratchVec<double>(nq, &scratch);

    const double a0 = ad[0];
    if (a0 == 0.0)
        throw Error("deconv: leading coefficient is zero",
                     0, 0, "deconv", "", "m:deconv:zeroLead");

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
xcorr(std::pmr::memory_resource *mr, const Value &x, const Value &y)
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

    auto c = (nx * ny > CONV_FFT_THRESHOLD * CONV_FFT_THRESHOLD)
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
std::tuple<Value, Value>
xcov(std::pmr::memory_resource *mr, const Value &x, const Value &y)
{
    // xcov = xcorr on the centered signals.
    auto centerInPlace = [mr](const Value &v) -> Value {
        const size_t n = v.numel();
        if (n == 0) return v;
        const double *vd = v.doubleData();
        double sum = 0.0;
        for (size_t i = 0; i < n; ++i) sum += vd[i];
        const double m = sum / static_cast<double>(n);
        Value c = Value::matrix(1, n, ValueType::DOUBLE, mr);
        double *cd = c.doubleDataMut();
        for (size_t i = 0; i < n; ++i) cd[i] = vd[i] - m;
        return c;
    };
    Value xc = centerInPlace(x);
    Value yc = centerInPlace(y);
    return xcorr(mr, xc, yc);
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
Value cropConv2(std::pmr::memory_resource *mr, const double *full,
                size_t fullR, size_t fullC,
                size_t M, size_t N, size_t P, size_t Q,
                const std::string &shape)
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
                 0, 0, "conv2", "", "m:conv2:badShape");
}

void requireDouble2D(const Value &v, const char *name)
{
    if (v.type() != ValueType::DOUBLE)
        throw Error(std::string(name) + ": only DOUBLE inputs are supported",
                     0, 0, name, "", std::string("m:") + name + ":notDouble");
    if (v.dims().ndim() > 2)
        throw Error(std::string(name) + ": input must be 1-D or 2-D",
                     0, 0, name, "", std::string("m:") + name + ":nd");
}

} // namespace

Value conv2(std::pmr::memory_resource *mr,
            const Value &A, const Value &B,
            const std::string &shape)
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

    return cropConv2(mr, full.data(), fullR, fullC, M, N, P, Q, shape);
}

Value filter2(std::pmr::memory_resource *mr,
              const Value &h, const Value &X,
              const std::string &shape)
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
    return conv2(mr, X, hf, shape);
}

Value convn(std::pmr::memory_resource *mr,
            const Value &A, const Value &B,
            const std::string &shape)
{
    if (A.type() != ValueType::DOUBLE || B.type() != ValueType::DOUBLE)
        throw Error("convn: only DOUBLE inputs are supported",
                     0, 0, "convn", "", "m:convn:notDouble");
    const int da = A.dims().ndim(), db = B.dims().ndim();
    const int nd = std::max(da, db);
    if (nd <= 1) {
        return conv(mr, A, B, shape);
    }
    if (nd == 2) {
        return conv2(mr, A, B, shape);
    }
    if (nd != 3) {
        throw Error("convn: only 1-D, 2-D, 3-D inputs supported",
                     0, 0, "convn", "", "m:convn:nd");
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
                 0, 0, "convn", "", "m:convn:badShape");
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void conv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("conv: requires at least 2 arguments",
                     0, 0, "conv", "", "m:conv:nargin");

    std::string shape = "full";
    if (args.size() >= 3 && args[2].isChar())
        shape = args[2].toString();

    outs[0] = conv(ctx.engine->resource(), args[0], args[1], shape);
}

void deconv_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("deconv: requires 2 arguments",
                     0, 0, "deconv", "", "m:deconv:nargin");

    auto [q, r] = deconv(ctx.engine->resource(), args[0], args[1]);
    outs[0] = std::move(q);
    if (nargout > 1)
        outs[1] = std::move(r);
}

void xcorr_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("xcorr: requires at least 1 argument",
                     0, 0, "xcorr", "", "m:xcorr:nargin");

    // Autocorrelation when called with a single arg, or when second
    // arg is a char flag like 'unbiased' (MATLAB compat: flag is accepted
    // but currently ignored — scaling mode not implemented).
    const bool autoCorr = (args.size() < 2 || args[1].isChar());

    std::tuple<Value, Value> result = autoCorr
        ? xcorr(ctx.engine->resource(), args[0])
        : xcorr(ctx.engine->resource(), args[0], args[1]);

    outs[0] = std::move(std::get<0>(result));
    if (nargout > 1)
        outs[1] = std::move(std::get<1>(result));
}

void xcov_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("xcov: requires at least 1 argument",
                     0, 0, "xcov", "", "m:xcov:nargin");
    auto *mr = ctx.engine->resource();
    auto result = (args.size() >= 2)
        ? xcov(mr, args[0], args[1])
        : xcov(mr, args[0]);
    outs[0] = std::move(std::get<0>(result));
    if (nargout > 1) outs[1] = std::move(std::get<1>(result));
}

void conv2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("conv2: requires at least 2 arguments",
                     0, 0, "conv2", "", "m:conv2:nargin");
    std::string shape = "full";
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString()))
        shape = args[2].toString();
    outs[0] = conv2(ctx.engine->resource(), args[0], args[1], shape);
}

void filter2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("filter2: requires at least 2 arguments (h, X)",
                     0, 0, "filter2", "", "m:filter2:nargin");
    std::string shape = "same";
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString()))
        shape = args[2].toString();
    outs[0] = filter2(ctx.engine->resource(), args[0], args[1], shape);
}

void convn_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("convn: requires at least 2 arguments",
                     0, 0, "convn", "", "m:convn:nargin");
    std::string shape = "full";
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString()))
        shape = args[2].toString();
    outs[0] = convn(ctx.engine->resource(), args[0], args[1], shape);
}

} // namespace detail

} // namespace numkit::signal
