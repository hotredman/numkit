// libs/builtin/src/MStdManip.cpp
//
// Phase-5 array manipulation kernels. All input/output is column-major
// double; 3D layout stored as page-stride R*C. Most ops are pure data
// movement (memcpy where possible) — no SIMD opportunity beyond what
// the compiler auto-vectorises in the inner copy loops.

#include <numkit/m/builtin/MStdManip.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MShapeOps.hpp>
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

    // Phase P6: build the first output page once, then replicate via
    // page memcpy for the remaining p-1 page tiles. The within-page
    // copy is the same m*n*C tile-of-memcpy structure as before — the
    // total bytes written floor at outR*outC*outP, no algorithmic
    // shortcut on the per-page cost. The win here is ONLY for p > 1
    // (page replication) where one big memcpy per page beats redoing
    // the per-column tile loop.
    for (size_t srcPage = 0; srcPage < P; ++srcPage) {
        double *firstPage = dst + (0 * P + srcPage) * outR * outC;
        for (size_t cTile = 0; cTile < n; ++cTile) {
            for (size_t c = 0; c < C; ++c) {
                const double *colSrc = src + srcPage * R * C + c * R;
                for (size_t rTile = 0; rTile < m; ++rTile) {
                    double *colDst = firstPage + (cTile * C + c) * outR
                                                 + rTile * R;
                    std::memcpy(colDst, colSrc, R * sizeof(double));
                }
            }
        }
        for (size_t pTile = 1; pTile < p; ++pTile) {
            double *targetPage = dst + (pTile * P + srcPage) * outR * outC;
            std::memcpy(targetPage, firstPage, outR * outC * sizeof(double));
        }
    }
    return r;
}

// ND repmat — tile vector of arbitrary length. Outer-coord walk maps
// each output column-of-axis-0 back to its source column via per-axis
// modulo, then memcpys axis-0-bytes tilesPadded[0] times to fill the
// output column. Type-preserving via byte-copy (elementSize-based).
MValue repmatND(Allocator &alloc, const MValue &x,
                const size_t *tiles, int ntiles)
{
    const MType t = x.type();
    if (t == MType::CELL || t == MType::STRUCT || t == MType::STRING
        || t == MType::FUNC_HANDLE)
        throw MError(std::string("repmat: ND repmat does not support type '")
                     + mtypeName(t) + "'",
                     0, 0, "repmat", "", "m:repmat:typeND");

    const auto &inDims = x.dims();
    constexpr int kMaxNd = Dims::kMaxRank;
    int outNdim = std::max(inDims.ndim(), ntiles);
    if (outNdim > kMaxNd)
        throw MError("repmat: rank exceeds 32",
                     0, 0, "repmat", "", "m:repmat:tooManyDims");
    if (outNdim < 1) outNdim = 1;

    size_t inDimPadded[kMaxNd];
    size_t tilesPadded[kMaxNd];
    size_t outDim[kMaxNd];
    for (int i = 0; i < outNdim; ++i) {
        inDimPadded[i] = (i < inDims.ndim()) ? inDims.dim(i) : 1;
        tilesPadded[i] = (i < ntiles) ? tiles[i] : 1;
        outDim[i] = inDimPadded[i] * tilesPadded[i];
    }

    auto r = MValue::matrixND(outDim, outNdim, t, &alloc);
    if (r.numel() == 0 || x.numel() == 0) return r;

    const size_t es = elementSize(t);
    const char *src = static_cast<const char *>(x.rawData());
    char *dst = static_cast<char *>(r.rawDataMut());

    // 1D special case: input is a row/col, output is `tilesPadded[0]` copies.
    if (outNdim == 1) {
        for (size_t k = 0; k < tilesPadded[0]; ++k)
            std::memcpy(dst + k * inDimPadded[0] * es, src,
                        inDimPadded[0] * es);
        return r;
    }

    size_t outStrides[kMaxNd];
    computeStridesColMajor(r.dims(), outStrides);
    size_t srcStrides[kMaxNd];
    computeStridesColMajor(inDims, srcStrides);
    for (int i = inDims.ndim(); i < outNdim; ++i) srcStrides[i] = 0;

    size_t outerDimsArr[kMaxNd];
    for (int i = 1; i < outNdim; ++i) outerDimsArr[i - 1] = outDim[i];
    Dims outerIter(outerDimsArr, outNdim - 1);

    size_t outerCoords[kMaxNd] = {0};
    do {
        size_t srcOff = 0;
        size_t dstOff = 0;
        for (int i = 1; i < outNdim; ++i) {
            const size_t oc = outerCoords[i - 1];
            const size_t inDimI = inDimPadded[i];
            srcOff += (oc % inDimI) * srcStrides[i];
            dstOff += oc * outStrides[i];
        }
        for (size_t k = 0; k < tilesPadded[0]; ++k)
            std::memcpy(dst + (dstOff + k * inDimPadded[0]) * es,
                        src + srcOff * es,
                        inDimPadded[0] * es);
    } while (incrementCoords(outerCoords, outerIter));

    return r;
}

// ────────────────────────────────────────────────────────────────────
// fliplr / flipud
// ────────────────────────────────────────────────────────────────────

namespace {

// ND flip helper: reverses the order of slabs along `axis` (0-based).
// The slab stride is B = prod(dims[0..axis-1]) elements; outer count
// O = prod(dims[axis+1..N-1]). Used by the ND fallback for fliplr
// (axis=1) and flipud (axis=0). Type-preserving via byte-copy.
MValue flipNDAlongAxis(Allocator &alloc, const MValue &x, int axis,
                       const char *fn)
{
    const MType t = x.type();
    if (t == MType::CELL || t == MType::STRUCT || t == MType::STRING
        || t == MType::FUNC_HANDLE)
        throw MError(std::string(fn) + ": ND fallback does not support type '"
                     + mtypeName(t) + "'",
                     0, 0, fn, "", std::string("m:") + fn + ":typeND");
    const auto &d = x.dims();
    const int nd = d.ndim();
    constexpr int kMaxNd = Dims::kMaxRank;
    if (nd > kMaxNd)
        throw MError(std::string(fn) + ": rank exceeds 32",
                     0, 0, fn, "", std::string("m:") + fn + ":tooManyDims");

    size_t outDimArr[kMaxNd];
    for (int i = 0; i < nd; ++i) outDimArr[i] = d.dim(i);
    auto r = MValue::matrixND(outDimArr, nd, t, &alloc);
    if (x.numel() == 0) return r;

    const size_t es = elementSize(t);
    const char *src = static_cast<const char *>(x.rawData());
    char *dst = static_cast<char *>(r.rawDataMut());

    // axis past the actual rank, or singleton dim → identity copy.
    const size_t flipDim = (axis < nd) ? d.dim(axis) : 1;
    if (flipDim <= 1) {
        std::memcpy(dst, src, x.numel() * es);
        return r;
    }

    size_t B = 1;
    for (int i = 0; i < axis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = axis + 1; i < nd; ++i) O *= d.dim(i);

    for (size_t o = 0; o < O; ++o) {
        const size_t outerBase = o * flipDim * B;
        for (size_t i = 0; i < flipDim; ++i) {
            std::memcpy(dst + (outerBase + i * B) * es,
                        src + (outerBase + (flipDim - 1 - i) * B) * es,
                        B * es);
        }
    }
    return r;
}

} // namespace

MValue fliplr(Allocator &alloc, const MValue &x)
{
    const auto &dd = x.dims();
    if (dd.ndim() >= 4) return flipNDAlongAxis(alloc, x, 1, "fliplr");

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
    if (dd.ndim() >= 4) return flipNDAlongAxis(alloc, x, 0, "flipud");

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

// Type-agnostic byte-copy variants for ND fallback. Same index math as
// the DOUBLE kernels above, but each cell is es bytes via memcpy.
inline void rot90OncePageBytes(const char *src, char *dst,
                               size_t R, size_t C, size_t es)
{
    for (size_t c = 0; c < C; ++c)
        for (size_t rr = 0; rr < R; ++rr)
            std::memcpy(dst + (rr * C + (C - 1 - c)) * es,
                        src + (c * R + rr) * es, es);
}

inline void rot180PageBytes(const char *src, char *dst,
                            size_t R, size_t C, size_t es)
{
    for (size_t c = 0; c < C; ++c)
        for (size_t rr = 0; rr < R; ++rr)
            std::memcpy(dst + ((C - 1 - c) * R + (R - 1 - rr)) * es,
                        src + (c * R + rr) * es, es);
}

inline void rot270PageBytes(const char *src, char *dst,
                            size_t R, size_t C, size_t es)
{
    for (size_t c = 0; c < C; ++c)
        for (size_t rr = 0; rr < R; ++rr)
            std::memcpy(dst + ((R - 1 - rr) * C + c) * es,
                        src + (c * R + rr) * es, es);
}

} // namespace

MValue rot90(Allocator &alloc, const MValue &x, int k)
{
    int kMod = k % 4;
    if (kMod < 0) kMod += 4;

    const auto &dd = x.dims();
    const int nd = dd.ndim();
    const size_t R = dd.rows(), C = dd.cols();

    // ND fallback (rank ≥ 4): rotate every (R×C) slice indexed by axes
    // 2..N-1. Output rank matches input; axes 0 and 1 swap for kMod 1/3.
    // Type-agnostic via byte-copy.
    if (nd >= 4) {
        const MType t = x.type();
        if (t == MType::CELL || t == MType::STRUCT || t == MType::STRING
            || t == MType::FUNC_HANDLE)
            throw MError(std::string("rot90: ND fallback does not support type '")
                         + mtypeName(t) + "'",
                         0, 0, "rot90", "", "m:rot90:typeND");
        constexpr int kMaxNd = Dims::kMaxRank;
        if (nd > kMaxNd)
            throw MError("rot90: rank exceeds 32",
                         0, 0, "rot90", "", "m:rot90:tooManyDims");
        size_t outDims[kMaxNd];
        outDims[0] = (kMod == 1 || kMod == 3) ? C : R;
        outDims[1] = (kMod == 1 || kMod == 3) ? R : C;
        for (int i = 2; i < nd; ++i) outDims[i] = dd.dim(i);
        auto r = MValue::matrixND(outDims, nd, t, &alloc);
        if (x.numel() == 0) return r;
        const size_t es = elementSize(t);
        const char *src = static_cast<const char *>(x.rawData());
        char *dst = static_cast<char *>(r.rawDataMut());
        const size_t pageElems = R * C;
        if (kMod == 0) {
            std::memcpy(dst, src, x.numel() * es);
        } else {
            auto pageFn = (kMod == 1) ? rot90OncePageBytes
                        : (kMod == 2) ? rot180PageBytes
                                      : rot270PageBytes;
            forEachOuterPage(dd, [&](size_t pp, const size_t *) {
                pageFn(src + pp * pageElems * es,
                       dst + pp * pageElems * es, R, C, es);
            });
        }
        return r;
    }

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

MValue circshiftND(Allocator &alloc, const MValue &x,
                   const int64_t *shifts, int nshifts)
{
    const MType t = x.type();
    if (t == MType::CELL || t == MType::STRUCT || t == MType::STRING
        || t == MType::FUNC_HANDLE)
        throw MError(std::string("circshift: ND fallback does not support type '")
                     + mtypeName(t) + "'",
                     0, 0, "circshift", "", "m:circshift:typeND");
    const auto &d = x.dims();
    const int nd = d.ndim();
    constexpr int kMaxNd = Dims::kMaxRank;
    if (nd > kMaxNd)
        throw MError("circshift: rank exceeds 32",
                     0, 0, "circshift", "", "m:circshift:tooManyDims");

    size_t outDims[kMaxNd];
    for (int i = 0; i < nd; ++i) outDims[i] = d.dim(i);
    auto r = MValue::matrixND(outDims, nd, t, &alloc);
    if (x.numel() == 0) return r;

    size_t shiftMod[kMaxNd] = {0};
    for (int i = 0; i < nd; ++i) {
        const int64_t s = (i < nshifts) ? shifts[i] : 0;
        shiftMod[i] = wrap(s, d.dim(i));
    }

    const size_t es = elementSize(t);
    const char *src = static_cast<const char *>(x.rawData());
    char *dst = static_cast<char *>(r.rawDataMut());
    const size_t R = d.dim(0);
    const size_t shift0 = shiftMod[0];

    if (nd == 1) {
        for (size_t i = 0; i < R; ++i)
            std::memcpy(dst + i * es,
                        src + ((i + R - shift0) % R) * es, es);
        return r;
    }

    size_t srcStrides[kMaxNd];
    computeStridesColMajor(d, srcStrides);

    size_t outerDimsArr[kMaxNd];
    for (int i = 1; i < nd; ++i) outerDimsArr[i - 1] = d.dim(i);
    Dims outerIter(outerDimsArr, nd - 1);

    size_t outerCoords[kMaxNd] = {0};
    do {
        size_t srcOuterOff = 0, dstOuterOff = 0;
        for (int i = 1; i < nd; ++i) {
            const size_t dimI = d.dim(i);
            const size_t srcCoord = (outerCoords[i - 1] + dimI - shiftMod[i]) % dimI;
            srcOuterOff += srcCoord * srcStrides[i];
            dstOuterOff += outerCoords[i - 1] * srcStrides[i];
        }
        if (shift0 == 0) {
            std::memcpy(dst + dstOuterOff * es,
                        src + srcOuterOff * es,
                        R * es);
        } else {
            for (size_t i = 0; i < R; ++i)
                std::memcpy(dst + (dstOuterOff + i) * es,
                            src + (srcOuterOff + (i + R - shift0) % R) * es,
                            es);
        }
    } while (incrementCoords(outerCoords, outerIter));

    return r;
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
    if (dd.ndim() >= 4) {
        const int64_t shifts[2] = {kRow, kCol};
        return circshiftND(alloc, x, shifts, 2);
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

// Type-agnostic per-page tril/triu via byte-copy + memset(0). All numeric
// types (DOUBLE, SINGLE, integer, LOGICAL, COMPLEX) zero correctly via
// memset since their zero element is all-zero bits.
inline void trilPageBytes(const char *src, char *dst, size_t R, size_t C,
                          int k, size_t es)
{
    for (size_t c = 0; c < C; ++c)
        for (size_t rr = 0; rr < R; ++rr) {
            const int diff = static_cast<int>(c) - static_cast<int>(rr);
            const size_t off = (c * R + rr) * es;
            if (diff <= k) std::memcpy(dst + off, src + off, es);
            else           std::memset(dst + off, 0, es);
        }
}

inline void triuPageBytes(const char *src, char *dst, size_t R, size_t C,
                          int k, size_t es)
{
    for (size_t c = 0; c < C; ++c)
        for (size_t rr = 0; rr < R; ++rr) {
            const int diff = static_cast<int>(c) - static_cast<int>(rr);
            const size_t off = (c * R + rr) * es;
            if (diff >= k) std::memcpy(dst + off, src + off, es);
            else           std::memset(dst + off, 0, es);
        }
}

} // namespace

// ND tril/triu: apply the 2D byte-level mask to every outer-axis slice.
// The first two axes form the matrix; axes 2..N-1 are "outer pages".
// All numeric types supported via byte-copy + memset(0) (zero bit pattern
// is the canonical zero for DOUBLE/SINGLE/integer/LOGICAL/COMPLEX).
namespace {

template <typename PageBytesFn>
MValue trilTriuND(Allocator &alloc, const MValue &x, int k,
                  PageBytesFn pageFn, const char *fn)
{
    const auto &dd = x.dims();
    constexpr int kMaxNd = Dims::kMaxRank;
    const int nd = dd.ndim();
    if (nd > kMaxNd)
        throw MError(std::string(fn) + ": rank exceeds 32",
                     0, 0, fn, "", std::string("m:") + fn + ":tooManyDims");
    const MType t = x.type();
    if (t == MType::CELL || t == MType::STRUCT || t == MType::STRING
        || t == MType::FUNC_HANDLE)
        throw MError(std::string(fn) + ": ND fallback does not support type '"
                     + mtypeName(t) + "'",
                     0, 0, fn, "", std::string("m:") + fn + ":typeND");

    const size_t R = dd.rows(), C = dd.cols();
    size_t outDimArr[kMaxNd];
    for (int i = 0; i < nd; ++i) outDimArr[i] = dd.dim(i);
    auto r = MValue::matrixND(outDimArr, nd, t, &alloc);
    if (x.numel() == 0) return r;

    const size_t es = elementSize(t);
    const char *src = static_cast<const char *>(x.rawData());
    char *dst = static_cast<char *>(r.rawDataMut());
    const size_t pageBytes = R * C * es;
    forEachOuterPage(dd, [&](size_t pp, const size_t *) {
        pageFn(src + pp * pageBytes, dst + pp * pageBytes, R, C, k, es);
    });
    return r;
}

} // namespace

MValue tril(Allocator &alloc, const MValue &x, int k)
{
    const auto &dd = x.dims();
    if (dd.ndim() >= 4)
        return trilTriuND(alloc, x, k, trilPageBytes, "tril");

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
    if (dd.ndim() >= 4)
        return trilTriuND(alloc, x, k, triuPageBytes, "triu");

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
    //   repmat(A, [m n p ...])  → vector (any length ≥ 1)
    //   repmat(A, m, n)         → two scalars
    //   repmat(A, m, n, p, ...) → ≥ 2 scalars
    std::vector<size_t> tiles;
    if (args.size() == 2) {
        const MValue &v = args[1];
        const size_t k = v.numel();
        if (k == 0) {
            throw MError("repmat: tile vector must not be empty",
                         0, 0, "repmat", "", "m:repmat:badTileVec");
        }
        if (k == 1) {
            const size_t s = static_cast<size_t>(v.toScalar());
            tiles = {s, s};
        } else {
            tiles.reserve(k);
            for (size_t i = 0; i < k; ++i)
                tiles.push_back(static_cast<size_t>(v.doubleData()[i]));
        }
    } else {
        tiles.reserve(args.size() - 1);
        for (size_t i = 1; i < args.size(); ++i)
            tiles.push_back(static_cast<size_t>(args[i].toScalar()));
    }

    // Fast path: rank ≤ 3 + tile vector ≤ 3 + DOUBLE → existing 2D/3D
    // kernel. Anything else (higher rank, longer tile vector, or non-
    // DOUBLE type) goes through repmatND.
    const auto &inDims = args[0].dims();
    const int outNdim = std::max(inDims.ndim(), static_cast<int>(tiles.size()));
    if (outNdim <= 3 && tiles.size() <= 3 && args[0].type() == MType::DOUBLE) {
        const size_t m = tiles[0];
        const size_t n = tiles.size() >= 2 ? tiles[1] : 1;
        const size_t p = tiles.size() >= 3 ? tiles[2] : 1;
        outs[0] = repmat(alloc, args[0], m, n, p);
    } else {
        outs[0] = repmatND(alloc, args[0], tiles.data(), static_cast<int>(tiles.size()));
    }
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
        throw MError("circshift: requires (X, k) or (X, shiftVec)",
                     0, 0, "circshift", "", "m:circshift:nargin");
    const MValue &k = args[1];
    auto &alloc = ctx.engine->allocator();
    const size_t nk = k.numel();
    if (nk == 0)
        throw MError("circshift: shift vector must not be empty",
                     0, 0, "circshift", "", "m:circshift:badShift");

    if (nk == 1) {
        outs[0] = circshift(alloc, args[0], static_cast<int64_t>(k.toScalar()));
        return;
    }
    if (nk == 2 && args[0].dims().ndim() <= 3) {
        outs[0] = circshift(alloc, args[0],
                            static_cast<int64_t>(k.doubleData()[0]),
                            static_cast<int64_t>(k.doubleData()[1]));
        return;
    }
    // ND path: shift vector ≥ 3 entries OR input rank ≥ 4.
    std::vector<int64_t> shifts(nk);
    for (size_t i = 0; i < nk; ++i)
        shifts[i] = static_cast<int64_t>(k.doubleData()[i]);
    outs[0] = circshiftND(alloc, args[0], shifts.data(), static_cast<int>(nk));
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
