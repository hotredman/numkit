// libs/builtin/src/MStdManip.cpp
//
// Phase-5 array manipulation kernels. All input/output is column-major
// double; 3D layout stored as page-stride R*C. Most ops are pure data
// movement (memcpy where possible) — no SIMD opportunity beyond what
// the compiler auto-vectorises in the inner copy loops.

#include <numkit/m/builtin/MStdManip.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace numkit::m::builtin {

// ────────────────────────────────────────────────────────────────────
// repmat
// ────────────────────────────────────────────────────────────────────
//
// Tile the input matrix m × n times (and optionally p times along
// pages for the 3D form). Output dims: (R*m) × (C*n) × (P*p).
MValue repmat(Allocator &alloc, const MValue &x, size_t m, size_t n, size_t p)
{
    const auto &dd = x.dims();
    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    const size_t outR = R * m, outC = C * n, outP = P * p;
    const bool out3D = (outP > 1) || dd.is3D();

    auto r = out3D ? MValue::matrix3d(outR, outC, outP, MType::DOUBLE, &alloc)
                   : MValue::matrix(outR, outC, MType::DOUBLE, &alloc);
    if (R == 0 || C == 0 || P == 0 || m == 0 || n == 0 || p == 0)
        return r;

    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();

    // Build one full output page first (outR × outC), then memcpy
    // it across the page dimension. For each (tilRow, tilCol) of the
    // m×n grid, copy the source's columns into the corresponding
    // block of the output page.
    for (size_t srcPage = 0; srcPage < P; ++srcPage) {
        for (size_t pTile = 0; pTile < p; ++pTile) {
            const size_t outPage = pTile * P + srcPage;
            double *page = dst + outPage * outR * outC;

            for (size_t cTile = 0; cTile < n; ++cTile) {
                for (size_t c = 0; c < C; ++c) {
                    const double *colSrc = src + srcPage * R * C + c * R;
                    for (size_t rTile = 0; rTile < m; ++rTile) {
                        double *colDst = page + (cTile * C + c) * outR + rTile * R;
                        std::memcpy(colDst, colSrc, R * sizeof(double));
                    }
                }
            }
        }
    }
    return r;
}

// ────────────────────────────────────────────────────────────────────
// fliplr / flipud
// ────────────────────────────────────────────────────────────────────

MValue fliplr(Allocator &alloc, const MValue &x)
{
    const auto &dd = x.dims();
    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc)
                       : MValue::matrix(R, C, MType::DOUBLE, &alloc);
    if (x.numel() == 0) return r;

    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    for (size_t pp = 0; pp < P; ++pp)
        for (size_t c = 0; c < C; ++c) {
            const double *colSrc = src + pp * R * C + (C - 1 - c) * R;
            double *colDst = dst + pp * R * C + c * R;
            std::memcpy(colDst, colSrc, R * sizeof(double));
        }
    return r;
}

MValue flipud(Allocator &alloc, const MValue &x)
{
    const auto &dd = x.dims();
    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc)
                       : MValue::matrix(R, C, MType::DOUBLE, &alloc);
    if (x.numel() == 0) return r;

    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    for (size_t pp = 0; pp < P; ++pp)
        for (size_t c = 0; c < C; ++c) {
            const double *colSrc = src + pp * R * C + c * R;
            double *colDst = dst + pp * R * C + c * R;
            for (size_t rr = 0; rr < R; ++rr)
                colDst[rr] = colSrc[R - 1 - rr];
        }
    return r;
}

// ────────────────────────────────────────────────────────────────────
// rot90
// ────────────────────────────────────────────────────────────────────
//
// rot90(A) rotates 90° counter-clockwise. For 2D matrix R×C, output
// is C×R with element (rNew, cNew) = A(cNew, C-1-rNew). The k-arg
// generalisation: k mod 4 selects 0/90/180/270° rotation. Negative k
// is clockwise.
namespace {

// Per-page rotation kernels. Each takes a single page (R*C contiguous
// elements, column-major) of the input and writes the rotated page to
// the output buffer. Output strides depend on the rotation: rot90/270
// swap (R, C); rot180 keeps (R, C). 3D dispatch in rot90() loops these
// over all pages.

inline void rot90OncePage(const double *src, double *dst, size_t R, size_t C)
{
    // Output shape (C, R): out[(C-1-c)*1 + r*C] = src[c*R + r]
    for (size_t c = 0; c < C; ++c)
        for (size_t rr = 0; rr < R; ++rr)
            dst[rr * C + (C - 1 - c)] = src[c * R + rr];
}

inline void rot180Page(const double *src, double *dst, size_t R, size_t C)
{
    for (size_t c = 0; c < C; ++c)
        for (size_t rr = 0; rr < R; ++rr)
            dst[(C - 1 - c) * R + (R - 1 - rr)] = src[c * R + rr];
}

inline void rot270Page(const double *src, double *dst, size_t R, size_t C)
{
    // Output shape (C, R): out[c + (R-1-r)*C] = src[c*R + r]
    for (size_t c = 0; c < C; ++c)
        for (size_t rr = 0; rr < R; ++rr)
            dst[(R - 1 - rr) * C + c] = src[c * R + rr];
}

} // namespace

MValue rot90(Allocator &alloc, const MValue &x, int k)
{
    int kMod = k % 4;
    if (kMod < 0) kMod += 4;

    const auto &dd = x.dims();
    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    const bool is3D = dd.is3D();

    // k mod 4 == 0 → identity (just copy). Same shape as input.
    if (kMod == 0) {
        auto r = is3D ? MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc)
                      : MValue::matrix(R, C, MType::DOUBLE, &alloc);
        if (x.numel() > 0)
            std::memcpy(r.doubleDataMut(), x.doubleData(),
                        x.numel() * sizeof(double));
        return r;
    }

    // k mod 4 == 2 → same shape (R, C) but elements reflected.
    if (kMod == 2) {
        auto r = is3D ? MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc)
                      : MValue::matrix(R, C, MType::DOUBLE, &alloc);
        if (x.numel() == 0) return r;
        const double *src = x.doubleData();
        double *dst = r.doubleDataMut();
        for (size_t pp = 0; pp < P; ++pp)
            rot180Page(src + pp * R * C, dst + pp * R * C, R, C);
        return r;
    }

    // k mod 4 == 1 (90° CCW) or == 3 (90° CW = 270°): output shape is
    // (C, R, P) — the per-page rows and cols swap.
    auto r = is3D ? MValue::matrix3d(C, R, P, MType::DOUBLE, &alloc)
                  : MValue::matrix(C, R, MType::DOUBLE, &alloc);
    if (x.numel() == 0) return r;
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    if (kMod == 1) {
        for (size_t pp = 0; pp < P; ++pp)
            rot90OncePage(src + pp * R * C, dst + pp * C * R, R, C);
    } else { // kMod == 3
        for (size_t pp = 0; pp < P; ++pp)
            rot270Page(src + pp * R * C, dst + pp * C * R, R, C);
    }
    return r;
}

// ────────────────────────────────────────────────────────────────────
// circshift
// ────────────────────────────────────────────────────────────────────
//
// MATLAB:
//   circshift(V, k)        — vector: rotate by k (positive = right/down).
//   circshift(M, k)        — matrix: shift along first non-singleton dim.
//   circshift(M, [r c])    — shift rows by r, cols by c.
// Modulo arithmetic ensures shifts >= dim length wrap correctly.
// Negative shifts work via the same modulo (k%n then add n if negative).
namespace {

inline size_t wrap(int64_t k, size_t n)
{
    if (n == 0) return 0;
    int64_t m = k % static_cast<int64_t>(n);
    if (m < 0) m += static_cast<int64_t>(n);
    return static_cast<size_t>(m);
}

void shift2D(const double *src, double *dst, size_t R, size_t C,
             int64_t kRow, int64_t kCol)
{
    const size_t shR = wrap(kRow, R);
    const size_t shC = wrap(kCol, C);
    for (size_t c = 0; c < C; ++c) {
        const size_t srcC = (c + C - shC) % C;
        for (size_t rr = 0; rr < R; ++rr) {
            const size_t srcR = (rr + R - shR) % R;
            dst[c * R + rr] = src[srcC * R + srcR];
        }
    }
}

} // namespace

MValue circshift(Allocator &alloc, const MValue &x, int64_t k)
{
    const auto &dd = x.dims();
    if (x.isScalar()) return MValue::scalar(x.toScalar(), &alloc);
    if (dd.isVector()) {
        const size_t n = x.numel();
        auto r = MValue::matrix(dd.rows(), dd.cols(), MType::DOUBLE, &alloc);
        const size_t sh = wrap(k, n);
        for (size_t i = 0; i < n; ++i)
            r.doubleDataMut()[i] = x.doubleData()[(i + n - sh) % n];
        return r;
    }
    // 2D / 3D matrix: scalar k shifts along first non-singleton dim.
    // For 2D matrices that's dim=1 (rows). For 3D, dim 1 too (since
    // rows is typically > 1).
    return circshift(alloc, x, k, 0);
}

MValue circshift(Allocator &alloc, const MValue &x, int64_t kRow, int64_t kCol)
{
    const auto &dd = x.dims();
    if (x.isScalar()) return MValue::scalar(x.toScalar(), &alloc);
    if (dd.is3D()) {
        const size_t R = dd.rows(), C = dd.cols(), P = dd.pages();
        auto r = MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc);
        for (size_t pp = 0; pp < P; ++pp)
            shift2D(x.doubleData() + pp * R * C,
                    r.doubleDataMut() + pp * R * C,
                    R, C, kRow, kCol);
        return r;
    }
    const size_t R = dd.rows(), C = dd.cols();
    auto r = MValue::matrix(R, C, MType::DOUBLE, &alloc);
    shift2D(x.doubleData(), r.doubleDataMut(), R, C, kRow, kCol);
    return r;
}

// ────────────────────────────────────────────────────────────────────
// tril / triu
// ────────────────────────────────────────────────────────────────────
//
// k is the diagonal offset:
//   k =  0  → main diagonal
//   k =  1  → first super-diagonal (above main)
//   k = -1  → first sub-diagonal (below main)
// tril keeps elements where col - row <= k. triu keeps col - row >= k.

namespace {

// Per-page lower-triangular mask: zero entries where col - row > k.
inline void trilPage(const double *src, double *dst, size_t R, size_t C, int k)
{
    for (size_t c = 0; c < C; ++c)
        for (size_t rr = 0; rr < R; ++rr) {
            const int diff = static_cast<int>(c) - static_cast<int>(rr);
            dst[c * R + rr] = (diff <= k) ? src[c * R + rr] : 0.0;
        }
}

inline void triuPage(const double *src, double *dst, size_t R, size_t C, int k)
{
    for (size_t c = 0; c < C; ++c)
        for (size_t rr = 0; rr < R; ++rr) {
            const int diff = static_cast<int>(c) - static_cast<int>(rr);
            dst[c * R + rr] = (diff >= k) ? src[c * R + rr] : 0.0;
        }
}

} // namespace

MValue tril(Allocator &alloc, const MValue &x, int k)
{
    const auto &dd = x.dims();
    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc)
                       : MValue::matrix(R, C, MType::DOUBLE, &alloc);
    if (x.numel() == 0) return r;
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    for (size_t pp = 0; pp < P; ++pp)
        trilPage(src + pp * R * C, dst + pp * R * C, R, C, k);
    return r;
}

MValue triu(Allocator &alloc, const MValue &x, int k)
{
    const auto &dd = x.dims();
    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc)
                       : MValue::matrix(R, C, MType::DOUBLE, &alloc);
    if (x.numel() == 0) return r;
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    for (size_t pp = 0; pp < P; ++pp)
        triuPage(src + pp * R * C, dst + pp * R * C, R, C, k);
    return r;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════
namespace detail {

void repmat_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw MError("repmat: requires at least 2 arguments",
                     0, 0, "repmat", "", "m:repmat:nargin");
    auto &alloc = ctx.engine->allocator();

    // Forms:
    //   repmat(A, n)            → m=n=arg
    //   repmat(A, [m n])        → vector
    //   repmat(A, [m n p])      → vector
    //   repmat(A, m, n)         → two scalars
    //   repmat(A, m, n, p)      → three scalars
    size_t m = 1, n = 1, p = 1;
    if (args.size() == 2) {
        const MValue &v = args[1];
        if (v.numel() == 1) {
            m = n = static_cast<size_t>(v.toScalar());
        } else if (v.numel() == 2) {
            m = static_cast<size_t>(v.doubleData()[0]);
            n = static_cast<size_t>(v.doubleData()[1]);
        } else if (v.numel() == 3) {
            m = static_cast<size_t>(v.doubleData()[0]);
            n = static_cast<size_t>(v.doubleData()[1]);
            p = static_cast<size_t>(v.doubleData()[2]);
        } else {
            throw MError("repmat: tile vector must have 1, 2, or 3 elements",
                         0, 0, "repmat", "", "m:repmat:badTileVec");
        }
    } else {
        m = static_cast<size_t>(args[1].toScalar());
        n = static_cast<size_t>(args[2].toScalar());
        if (args.size() >= 4) p = static_cast<size_t>(args[3].toScalar());
    }
    outs[0] = repmat(alloc, args[0], m, n, p);
}

#define NK_FLIP_REG(name)                                                      \
    void name##_reg(Span<const MValue> args, size_t /*nargout*/,               \
                    Span<MValue> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.empty())                                                      \
            throw MError(#name ": requires 1 argument",                        \
                         0, 0, #name, "", "m:" #name ":nargin");               \
        outs[0] = name(ctx.engine->allocator(), args[0]);                      \
    }

NK_FLIP_REG(fliplr)
NK_FLIP_REG(flipud)

#undef NK_FLIP_REG

void rot90_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw MError("rot90: requires at least 1 argument",
                     0, 0, "rot90", "", "m:rot90:nargin");
    int k = (args.size() >= 2 && !args[1].isEmpty())
                ? static_cast<int>(args[1].toScalar())
                : 1;
    outs[0] = rot90(ctx.engine->allocator(), args[0], k);
}

void circshift_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                   CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("circshift: requires (X, k) or (X, [r c])",
                     0, 0, "circshift", "", "m:circshift:nargin");
    const MValue &k = args[1];
    auto &alloc = ctx.engine->allocator();
    if (k.numel() == 1) {
        outs[0] = circshift(alloc, args[0], static_cast<int64_t>(k.toScalar()));
    } else if (k.numel() == 2) {
        outs[0] = circshift(alloc, args[0],
                            static_cast<int64_t>(k.doubleData()[0]),
                            static_cast<int64_t>(k.doubleData()[1]));
    } else {
        throw MError("circshift: shift vector must have 1 or 2 elements",
                     0, 0, "circshift", "", "m:circshift:badShift");
    }
}

#define NK_TRI_REG(name)                                                       \
    void name##_reg(Span<const MValue> args, size_t /*nargout*/,               \
                    Span<MValue> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.empty())                                                      \
            throw MError(#name ": requires at least 1 argument",               \
                         0, 0, #name, "", "m:" #name ":nargin");               \
        int k = (args.size() >= 2 && !args[1].isEmpty())                       \
                    ? static_cast<int>(args[1].toScalar())                     \
                    : 0;                                                        \
        outs[0] = name(ctx.engine->allocator(), args[0], k);                   \
    }

NK_TRI_REG(tril)
NK_TRI_REG(triu)

#undef NK_TRI_REG

} // namespace detail

} // namespace numkit::m::builtin
