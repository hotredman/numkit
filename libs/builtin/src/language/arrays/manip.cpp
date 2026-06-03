// libs/builtin/src/lang/arrays/manip.cpp
//
// Phase-5 array manipulation kernels. All input/output is column-major
// double; 3D layout stored as page-stride R*C. Most ops are pure data
// movement (memcpy where possible) — no SIMD opportunity beyond what
// the compiler auto-vectorises in the inner copy loops.

#include <numkit/builtin/language/arrays/manip.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/shape_ops.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace numkit::builtin {

// ────────────────────────────────────────────────────────────────────
// repmat
// ────────────────────────────────────────────────────────────────────
//
// Tile the input matrix m × n times (and optionally p times along
// pages for the 3D form). Output dims: (R*m) × (C*n) × (P*p).
Value repmat(const Value &x, size_t m, size_t n, size_t p, std::pmr::memory_resource *mr)
{
    // STRING / CELL / non-DOUBLE types delegate to the ND path which
    // handles them. The fast 2-D DOUBLE memcpy loop below assumes
    // contiguous double storage. See BUGS.md #6.
    const ValueType t = x.type();
    if (t != ValueType::DOUBLE) {
        const size_t tiles[3] = { m, n, p };
        const size_t nd = (p > 1 || x.dims().is3D()) ? 3 : 2;
        return repmatND(x, Span<const size_t>(tiles, nd), mr);
    }
    const auto &dd = x.dims();
    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    const size_t outR = R * m, outC = C * n, outP = P * p;
    const bool out3D = (outP > 1) || dd.is3D();

    auto r = out3D ? Value::matrix3d(outR, outC, outP, ValueType::DOUBLE, mr)
                   : Value::matrix(outR, outC, ValueType::DOUBLE, mr);
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
Value repmatND(const Value &x, Span<const size_t> tiles, std::pmr::memory_resource *mr)
{
    const ValueType t = x.type();
    if (t == ValueType::STRUCT || t == ValueType::FUNC_HANDLE)
        throw Error(std::string("repmat: ND repmat does not support type '")
                     + mtypeName(t) + "'",
                     0, 0, "repmat", "", "numkit:repmat:typeND");

    const auto &inDims = x.dims();
    constexpr int kMaxNd = Dims::kMaxRank;
    const int ntiles = static_cast<int>(tiles.size());
    int outNdim = std::max(inDims.ndim(), ntiles);
    if (outNdim > kMaxNd)
        throw Error("repmat: rank exceeds 32",
                     0, 0, "repmat", "", "numkit:repmat:tooManyDims");
    if (outNdim < 1) outNdim = 1;

    // OBJECT arrays store per-element state — build the tile→source index
    // map (per-axis modulo, same as the CELL path) and gather.
    if (x.isObject()) {
        size_t inDP[kMaxNd], tilesP[kMaxNd], outD[kMaxNd];
        for (int i = 0; i < outNdim; ++i) {
            inDP[i] = (i < inDims.ndim()) ? inDims.dim(i) : 1;
            tilesP[i] = (i < ntiles) ? tiles[i] : 1;
            outD[i] = inDP[i] * tilesP[i];
        }
        Dims outDims(outD, outNdim);
        const size_t total = outDims.numel();
        std::vector<size_t> srcMap(total, 0);
        if (x.numel() > 0 && total > 0) {
            size_t outStrides[kMaxNd], srcStrides[kMaxNd];
            computeStridesColMajor(outDims, outStrides);
            computeStridesColMajor(inDims, srcStrides);
            for (int i = inDims.ndim(); i < outNdim; ++i) srcStrides[i] = 0;
            for (size_t outIdx = 0; outIdx < total; ++outIdx) {
                size_t rem = outIdx, srcIdx = 0;
                for (int d = outNdim - 1; d >= 0; --d) {
                    const size_t coord = rem / outStrides[d];
                    rem -= coord * outStrides[d];
                    if (d < inDims.ndim())
                        srcIdx += (coord % inDP[d]) * srcStrides[d];
                }
                srcMap[outIdx] = srcIdx;
            }
        }
        return x.objectGather(srcMap.data(), outDims, mr);
    }

    // STRING / CELL store contents as vector<Value> in cellData, not a
    // contiguous byte buffer — the memcpy paths below would dereference
    // a null rawData(). Walk by element and copy Values directly.
    // See BUGS.md #6 and #7-related cell-array handling.
    if (t == ValueType::STRING || t == ValueType::CELL) {
        size_t inDP[kMaxNd];
        size_t tilesP[kMaxNd];
        size_t outD[kMaxNd];
        for (int i = 0; i < outNdim; ++i) {
            inDP[i]   = (i < inDims.ndim()) ? inDims.dim(i) : 1;
            tilesP[i] = (i < ntiles)        ? tiles[i]      : 1;
            outD[i]   = inDP[i] * tilesP[i];
        }
        // Build output of the same value-type (STRING or CELL) with
        // the new shape; cellData is allocated empty and filled.
        Value r;
        if (t == ValueType::STRING) {
            if (outNdim <= 2)
                r = Value::stringArray(outD[0], outNdim >= 2 ? outD[1] : 1, mr);
            else if (outNdim == 3)
                r = Value::stringArray3D(outD[0], outD[1], outD[2], mr);
            else
                throw Error("repmat: ND > 3 not supported for string",
                             0, 0, "repmat", "", "numkit:repmat:tooManyDimsStr");
        } else { // CELL
            if (outNdim <= 2)
                r = Value::cell(outD[0], outNdim >= 2 ? outD[1] : 1, mr);
            else
                throw Error("repmat: ND > 2 not supported for cell",
                             0, 0, "repmat", "", "numkit:repmat:tooManyDimsCell");
        }
        if (x.numel() == 0) return r;

        size_t outStrides[kMaxNd];
        computeStridesColMajor(r.dims(), outStrides);
        size_t srcStrides[kMaxNd];
        computeStridesColMajor(inDims, srcStrides);
        for (int i = inDims.ndim(); i < outNdim; ++i) srcStrides[i] = 0;

        const size_t total = r.numel();
        for (size_t outIdx = 0; outIdx < total; ++outIdx) {
            // Recover output coord, map back to source coord by per-axis modulo.
            size_t rem = outIdx;
            size_t srcIdx = 0;
            for (int d = outNdim - 1; d >= 0; --d) {
                const size_t coord = rem / outStrides[d];
                rem -= coord * outStrides[d];
                if (d < inDims.ndim())
                    srcIdx += (coord % inDP[d]) * srcStrides[d];
            }
            if (t == ValueType::STRING)
                r.stringElemSet(outIdx, x.stringElem(srcIdx));
            else
                r.cellAt(outIdx) = x.cellAt(srcIdx);
        }
        return r;
    }

    size_t inDimPadded[kMaxNd];
    size_t tilesPadded[kMaxNd];
    size_t outDim[kMaxNd];
    for (int i = 0; i < outNdim; ++i) {
        inDimPadded[i] = (i < inDims.ndim()) ? inDims.dim(i) : 1;
        tilesPadded[i] = (i < ntiles) ? tiles[i] : 1;
        outDim[i] = inDimPadded[i] * tilesPadded[i];
    }

    auto r = Value::matrixND(outDim, outNdim, t, mr);
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

// CELL / STRING store their contents as a vector<Value> (cellData), not a
// contiguous byte buffer, so the byte-copy paths below would dereference a
// null rawData(). These helpers walk elements and copy Values directly,
// mirroring the repmat cell/string handling above. MATLAB's flip family
// (flip/fliplr/flipud/rot90) is type-agnostic — it just permutes elements
// — so cell and string arrays must reorder exactly like numeric ones.

inline bool isCellOrString(ValueType t)
{
    return t == ValueType::CELL || t == ValueType::STRING;
}

// Allocate a CELL or STRING result of shape rows×cols.
inline Value makeCellOrString(ValueType t, size_t rows, size_t cols,
                              std::pmr::memory_resource *mr)
{
    return (t == ValueType::STRING) ? Value::stringArray(rows, cols, mr)
                                    : Value::cell(rows, cols, mr);
}

// Copy element at column-major index srcIdx of src into dstIdx of dst.
inline void copyCellElem(Value &dst, size_t dstIdx, const Value &src, size_t srcIdx)
{
    if (src.type() == ValueType::STRING)
        dst.stringElemSet(dstIdx, src.stringElem(srcIdx));
    else
        dst.cellAt(dstIdx) = src.cellAt(srcIdx);
}

// Reverse a CELL/STRING array along `axis` (0=rows, 1=cols). Supports up to
// 2-D (the only ranks Value::cell can represent); higher-rank cell/string
// flips are rare and deferred with a clear error.
Value flipCellStr(const Value &x, int axis, const char *fn, std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    if (d.ndim() > 2)
        throw Error(std::string(fn) + ": cell/string arrays support up to 2-D",
                     0, 0, fn, "", std::string("numkit:") + fn + ":cellRank");
    const size_t R = d.rows(), C = d.cols();
    Value r = makeCellOrString(x.type(), R, C, mr);
    if (x.numel() == 0) return r;
    const size_t flipDim = (axis == 0) ? R : (axis == 1) ? C : 1;
    if (axis > 1 || flipDim <= 1) {                  // identity copy
        for (size_t i = 0; i < x.numel(); ++i) copyCellElem(r, i, x, i);
        return r;
    }
    for (size_t c = 0; c < C; ++c)
        for (size_t rr = 0; rr < R; ++rr) {
            const size_t outIdx = c * R + rr;
            const size_t srcIdx = (axis == 0) ? (c * R + (R - 1 - rr))
                                              : ((C - 1 - c) * R + rr);
            copyCellElem(r, outIdx, x, srcIdx);
        }
    return r;
}

// ND flip helper: reverses the order of slabs along `axis` (0-based).
// The slab stride is B = prod(dims[0..axis-1]) elements; outer count
// O = prod(dims[axis+1..N-1]). Used by the ND fallback for fliplr
// (axis=1) and flipud (axis=0). Type-preserving via byte-copy.
Value flipNDAlongAxis(const Value &x, int axis, const char *fn, std::pmr::memory_resource *mr)
{
    const ValueType t = x.type();
    if (isCellOrString(t)) return flipCellStr(x, axis, fn, mr);
    if (x.isObject()) {
        // Reverse the `axis` dimension via an index map → objectGather.
        const auto &d = x.dims();
        const int nd = d.ndim();
        const size_t flipDim = (axis < nd) ? d.dim(axis) : 1;
        const size_t total = d.numel();
        std::vector<size_t> map(total);
        if (flipDim <= 1) {
            for (size_t i = 0; i < total; ++i) map[i] = i;
        } else {
            size_t B = 1;
            for (int i = 0; i < axis; ++i) B *= d.dim(i);
            size_t O = 1;
            for (int i = axis + 1; i < nd; ++i) O *= d.dim(i);
            for (size_t o = 0; o < O; ++o) {
                const size_t outerBase = o * flipDim * B;
                for (size_t i = 0; i < flipDim; ++i)
                    for (size_t b = 0; b < B; ++b)
                        map[outerBase + i * B + b] = outerBase + (flipDim - 1 - i) * B + b;
            }
        }
        return x.objectGather(map.data(), d, mr);
    }
    if (t == ValueType::STRUCT || t == ValueType::FUNC_HANDLE)
        throw Error(std::string(fn) + ": ND fallback does not support type '"
                     + mtypeName(t) + "'",
                     0, 0, fn, "", std::string("numkit:") + fn + ":typeND");
    const auto &d = x.dims();
    const int nd = d.ndim();
    constexpr int kMaxNd = Dims::kMaxRank;
    if (nd > kMaxNd)
        throw Error(std::string(fn) + ": rank exceeds 32",
                     0, 0, fn, "", std::string("numkit:") + fn + ":tooManyDims");

    size_t outDimArr[kMaxNd];
    for (int i = 0; i < nd; ++i) outDimArr[i] = d.dim(i);
    auto r = Value::matrixND(outDimArr, nd, t, mr);
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

Value fliplr(const Value &x, std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    if (isCellOrString(x.type())) return flipCellStr(x, 1, "fliplr", mr);
    if (x.isObject()) return flipNDAlongAxis(x, 1, "fliplr", mr); // flip dim 2
    if (dd.ndim() >= 4) return flipNDAlongAxis(x, 1, "fliplr", mr);

    // POD types (DOUBLE/CHAR/LOGICAL/COMPLEX/single/int) copy raw bytes — flip
    // is a pure rearrangement, so it is type-preserving (cell/string handled
    // above via flipCellStr).
    const ValueType t = x.type();
    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? Value::matrix3d(R, C, P, t, mr)
                       : Value::matrix(R, C, t, mr);
    if (x.numel() == 0) return r;

    const size_t es = elementSize(t);
    const char *src = static_cast<const char *>(x.rawData());
    char *dst = static_cast<char *>(r.rawDataMut());
    for (size_t pp = 0; pp < P; ++pp)
        for (size_t c = 0; c < C; ++c) {
            const char *colSrc = src + (pp * R * C + (C - 1 - c) * R) * es;
            char *colDst = dst + (pp * R * C + c * R) * es;
            std::memcpy(colDst, colSrc, R * es);
        }
    return r;
}

Value flipud(const Value &x, std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    if (isCellOrString(x.type())) return flipCellStr(x, 0, "flipud", mr);
    if (x.isObject()) return flipNDAlongAxis(x, 0, "flipud", mr); // flip dim 1
    if (dd.ndim() >= 4) return flipNDAlongAxis(x, 0, "flipud", mr);

    // POD types copy raw bytes — type-preserving (cell/string handled above).
    const ValueType t = x.type();
    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? Value::matrix3d(R, C, P, t, mr)
                       : Value::matrix(R, C, t, mr);
    if (x.numel() == 0) return r;

    const size_t es = elementSize(t);
    const char *src = static_cast<const char *>(x.rawData());
    char *dst = static_cast<char *>(r.rawDataMut());
    for (size_t pp = 0; pp < P; ++pp)
        for (size_t c = 0; c < C; ++c) {
            const char *colSrc = src + (pp * R * C + c * R) * es;
            char *colDst = dst + (pp * R * C + c * R) * es;
            for (size_t rr = 0; rr < R; ++rr)
                std::memcpy(colDst + rr * es, colSrc + (R - 1 - rr) * es, es);
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

// rot90 for CELL / STRING arrays (element-wise Value copy). kMod selects
// the rotation (0/1/2/3 → 0/90/180/270° CCW). Supports up to 2-D.
Value rot90CellStr(const Value &x, int kMod, std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    if (d.ndim() > 2)
        throw Error("rot90: cell/string arrays support up to 2-D",
                     0, 0, "rot90", "", "numkit:rot90:cellRank");
    const size_t R = d.rows(), C = d.cols();
    const ValueType t = x.type();
    if (kMod == 0 || kMod == 2) {                    // shape stays R×C
        Value r = makeCellOrString(t, R, C, mr);
        if (x.numel() == 0) return r;
        for (size_t c = 0; c < C; ++c)
            for (size_t rr = 0; rr < R; ++rr) {
                const size_t outIdx = c * R + rr;
                const size_t srcIdx = (kMod == 0)
                    ? (c * R + rr)
                    : ((C - 1 - c) * R + (R - 1 - rr));
                copyCellElem(r, outIdx, x, srcIdx);
            }
        return r;
    }
    // kMod 1 (90°) / 3 (270°): output shape is C×R (rows/cols swap).
    Value r = makeCellOrString(t, C, R, mr);
    if (x.numel() == 0) return r;
    for (size_t c = 0; c < C; ++c)
        for (size_t rr = 0; rr < R; ++rr) {
            const size_t srcIdx = c * R + rr;            // src(rr, c)
            const size_t outIdx = (kMod == 1)
                ? ((C - 1 - c) + rr * C)                 // out(C-1-c, rr)
                : (c + (R - 1 - rr) * C);                // out(c, R-1-rr)
            copyCellElem(r, outIdx, x, srcIdx);
        }
    return r;
}

} // namespace

Value rot90(const Value &x, int k, std::pmr::memory_resource *mr)
{
    int kMod = k % 4;
    if (kMod < 0) kMod += 4;

    if (isCellOrString(x.type())) return rot90CellStr(x, kMod, mr);

    const auto &dd = x.dims();
    const int nd = dd.ndim();
    const size_t R = dd.rows(), C = dd.cols();

    // OBJECT (2-D): rotate via the same index map as rot90CellStr (each
    // assignment is result[outIdx] = src[srcIdx]) → objectGather.
    if (x.isObject() && nd < 4) {
        std::vector<size_t> map(R * C);
        Dims outD = (kMod == 1 || kMod == 3) ? Dims(C, R) : Dims(R, C);
        for (size_t c = 0; c < C; ++c)
            for (size_t rr = 0; rr < R; ++rr) {
                if (kMod == 0)
                    map[c * R + rr] = c * R + rr;
                else if (kMod == 2)
                    map[c * R + rr] = (C - 1 - c) * R + (R - 1 - rr);
                else if (kMod == 1)
                    map[(C - 1 - c) + rr * C] = c * R + rr;
                else
                    map[c + (R - 1 - rr) * C] = c * R + rr;
            }
        return x.objectGather(map.data(), outD, mr);
    }

    // ND fallback (rank ≥ 4): rotate every (R×C) slice indexed by axes
    // 2..N-1. Output rank matches input; axes 0 and 1 swap for kMod 1/3.
    // Type-agnostic via byte-copy.
    if (nd >= 4) {
        const ValueType t = x.type();
        if (t == ValueType::CELL || t == ValueType::STRUCT || t == ValueType::STRING
            || t == ValueType::FUNC_HANDLE)
            throw Error(std::string("rot90: ND fallback does not support type '")
                         + mtypeName(t) + "'",
                         0, 0, "rot90", "", "numkit:rot90:typeND");
        constexpr int kMaxNd = Dims::kMaxRank;
        if (nd > kMaxNd)
            throw Error("rot90: rank exceeds 32",
                         0, 0, "rot90", "", "numkit:rot90:tooManyDims");
        size_t outDims[kMaxNd];
        outDims[0] = (kMod == 1 || kMod == 3) ? C : R;
        outDims[1] = (kMod == 1 || kMod == 3) ? R : C;
        for (int i = 2; i < nd; ++i) outDims[i] = dd.dim(i);
        auto r = Value::matrixND(outDims, nd, t, mr);
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

    // Non-DOUBLE 2-D/3-D POD types (char/logical/complex/single/int): rot90 is
    // a pure rearrangement, so reuse the byte-copy kernels (cell/string were
    // handled above by rot90CellStr). Output preserves x's type.
    if (x.type() != ValueType::DOUBLE) {
        const ValueType t = x.type();
        const size_t es = elementSize(t);
        const char *src = static_cast<const char *>(x.rawData());
        if (kMod == 0) {
            auto r = is3D ? Value::matrix3d(R, C, P, t, mr) : Value::matrix(R, C, t, mr);
            if (x.numel() > 0) std::memcpy(r.rawDataMut(), src, x.numel() * es);
            return r;
        }
        if (kMod == 2) {
            auto r = is3D ? Value::matrix3d(R, C, P, t, mr) : Value::matrix(R, C, t, mr);
            if (x.numel() == 0) return r;
            char *dst = static_cast<char *>(r.rawDataMut());
            for (size_t pp = 0; pp < P; ++pp)
                rot180PageBytes(src + pp * R * C * es, dst + pp * R * C * es, R, C, es);
            return r;
        }
        // kMod 1 (90° CCW) / 3 (270°): output shape (C, R, P).
        auto r = is3D ? Value::matrix3d(C, R, P, t, mr) : Value::matrix(C, R, t, mr);
        if (x.numel() == 0) return r;
        char *dst = static_cast<char *>(r.rawDataMut());
        const auto kern = (kMod == 1) ? rot90OncePageBytes : rot270PageBytes;
        for (size_t pp = 0; pp < P; ++pp)
            kern(src + pp * R * C * es, dst + pp * C * R * es, R, C, es);
        return r;
    }

    // k mod 4 == 0 → identity (just copy). Same shape as input.
    if (kMod == 0) {
        auto r = is3D ? Value::matrix3d(R, C, P, ValueType::DOUBLE, mr)
                      : Value::matrix(R, C, ValueType::DOUBLE, mr);
        if (x.numel() > 0)
            std::memcpy(r.doubleDataMut(), x.doubleData(),
                        x.numel() * sizeof(double));
        return r;
    }

    // k mod 4 == 2 → same shape (R, C) but elements reflected.
    if (kMod == 2) {
        auto r = is3D ? Value::matrix3d(R, C, P, ValueType::DOUBLE, mr)
                      : Value::matrix(R, C, ValueType::DOUBLE, mr);
        if (x.numel() == 0) return r;
        const double *src = x.doubleData();
        double *dst = r.doubleDataMut();
        for (size_t pp = 0; pp < P; ++pp)
            rot180Page(src + pp * R * C, dst + pp * R * C, R, C);
        return r;
    }

    // k mod 4 == 1 (90° CCW) or == 3 (90° CW = 270°): output shape is
    // (C, R, P) — the per-page rows and cols swap.
    auto r = is3D ? Value::matrix3d(C, R, P, ValueType::DOUBLE, mr)
                  : Value::matrix(C, R, ValueType::DOUBLE, mr);
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

Value circshift(const Value &x, int64_t k, std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    // Non-DOUBLE (char / logical / complex / cell / string): circshift is a
    // pure rearrangement — route through the type-agnostic ND permute. Scalar
    // k shifts along the first non-singleton dimension.
    if (x.type() != ValueType::DOUBLE) {
        if (x.isScalar() || x.numel() == 0) return x;
        int fnsd = 0;
        for (int i = 0; i < dd.ndim(); ++i)
            if (dd.dim(i) > 1) { fnsd = i; break; }
        int64_t sh[Dims::kMaxRank] = {0};
        sh[fnsd] = k;
        return circshiftND(x, Span<const int64_t>(sh, fnsd + 1), mr);
    }
    if (x.isScalar()) return Value::scalar(x.toScalar(), mr);
    if (dd.isVector()) {
        const size_t n = x.numel();
        auto r = Value::matrix(dd.rows(), dd.cols(), ValueType::DOUBLE, mr);
        const size_t sh = wrap(k, n);
        for (size_t i = 0; i < n; ++i)
            r.doubleDataMut()[i] = x.doubleData()[(i + n - sh) % n];
        return r;
    }
    // 2D / 3D matrix: scalar k shifts along first non-singleton dim.
    // For 2D matrices that's dim=1 (rows). For 3D, dim 1 too (since
    // rows is typically > 1).
    return circshift(x, k, 0, mr);
}

Value circshiftND(const Value &x, Span<const int64_t> shifts, std::pmr::memory_resource *mr)
{
    const ValueType t = x.type();
    // STRING arrays + struct arrays + function-handle arrays are deferred.
    if (t == ValueType::STRUCT || t == ValueType::FUNC_HANDLE
        || t == ValueType::STRING)
        throw Error(std::string("circshift: does not support type '")
                     + mtypeName(t) + "'",
                     0, 0, "circshift", "", "numkit:circshift:typeND");
    const auto &d = x.dims();
    const int nd = d.ndim();
    constexpr int kMaxNd = Dims::kMaxRank;
    if (nd > kMaxNd)
        throw Error("circshift: rank exceeds 32",
                     0, 0, "circshift", "", "numkit:circshift:tooManyDims");

    const bool isCell = (t == ValueType::CELL);
    if (isCell && nd > 2)
        throw Error("circshift: cell array rank > 2 not supported",
                     0, 0, "circshift", "", "numkit:circshift:cellND");

    size_t outDims[kMaxNd];
    for (int i = 0; i < nd; ++i) outDims[i] = d.dim(i);
    // CELL needs proper cell storage (Value::matrixND does not allocate it).
    Value r = isCell ? Value::cell(outDims[0], nd >= 2 ? outDims[1] : 1, mr)
                     : Value::matrixND(outDims, nd, t, mr);
    if (x.numel() == 0) return r;

    const int nshifts = static_cast<int>(shifts.size());
    size_t shiftMod[kMaxNd] = {0};
    for (int i = 0; i < nd; ++i) {
        const int64_t s = (i < nshifts) ? shifts[i] : 0;
        shiftMod[i] = wrap(s, d.dim(i));
    }

    // OBJECT: build the per-element shifted source map → objectGather.
    if (x.isObject()) {
        size_t strides[kMaxNd];
        computeStridesColMajor(d, strides);
        const size_t total = d.numel();
        std::vector<size_t> map(total);
        for (size_t dstLin = 0; dstLin < total; ++dstLin) {
            size_t rem = dstLin, srcLin = 0;
            for (int i = nd - 1; i >= 0; --i) {
                const size_t coord = rem / strides[i];
                rem -= coord * strides[i];
                const size_t srcCoord = (coord + d.dim(i) - shiftMod[i]) % d.dim(i);
                srcLin += srcCoord * strides[i];
            }
            map[dstLin] = srcLin;
        }
        return x.objectGather(map.data(), d, mr);
    }

    // CELL permutes element-by-element (Value copy); POD types copy raw
    // bytes. circshift is a pure rearrangement, so it is type-agnostic.
    const size_t es = isCell ? 0 : elementSize(t);
    const char *src = isCell ? nullptr : static_cast<const char *>(x.rawData());
    char *dst = isCell ? nullptr : static_cast<char *>(r.rawDataMut());
    auto copyElem = [&](size_t dIdx, size_t sIdx) {
        if (isCell) r.cellAt(dIdx) = x.cellAt(sIdx);
        else        std::memcpy(dst + dIdx * es, src + sIdx * es, es);
    };
    const size_t R = d.dim(0);
    const size_t shift0 = shiftMod[0];

    if (nd == 1) {
        for (size_t i = 0; i < R; ++i)
            copyElem(i, (i + R - shift0) % R);
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
            if (isCell)
                for (size_t i = 0; i < R; ++i) copyElem(dstOuterOff + i, srcOuterOff + i);
            else
                std::memcpy(dst + dstOuterOff * es, src + srcOuterOff * es, R * es);
        } else {
            for (size_t i = 0; i < R; ++i)
                copyElem(dstOuterOff + i, srcOuterOff + (i + R - shift0) % R);
        }
    } while (incrementCoords(outerCoords, outerIter));

    return r;
}

Value circshift(const Value &x, int64_t kRow, int64_t kCol, std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    // Non-DOUBLE: route through the type-agnostic ND permute (shift dim1 by
    // kRow, dim2 by kCol; higher dims unshifted).
    if (x.type() != ValueType::DOUBLE) {
        if (x.isScalar() || x.numel() == 0) return x;
        const int64_t shifts[2] = {kRow, kCol};
        return circshiftND(x, Span<const int64_t>(shifts, 2), mr);
    }
    if (x.isScalar()) return Value::scalar(x.toScalar(), mr);
    if (dd.is3D()) {
        const size_t R = dd.rows(), C = dd.cols(), P = dd.pages();
        auto r = Value::matrix3d(R, C, P, ValueType::DOUBLE, mr);
        for (size_t pp = 0; pp < P; ++pp)
            shift2D(x.doubleData() + pp * R * C,
                    r.doubleDataMut() + pp * R * C,
                    R, C, kRow, kCol);
        return r;
    }
    if (dd.ndim() >= 4) {
        const int64_t shifts[2] = {kRow, kCol};
        return circshiftND(x, Span<const int64_t>(shifts, 2), mr);
    }
    const size_t R = dd.rows(), C = dd.cols();
    auto r = Value::matrix(R, C, ValueType::DOUBLE, mr);
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
Value trilTriuND(const Value &x, int k, PageBytesFn pageFn, const char *fn, std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    constexpr int kMaxNd = Dims::kMaxRank;
    const int nd = dd.ndim();
    if (nd > kMaxNd)
        throw Error(std::string(fn) + ": rank exceeds 32",
                     0, 0, fn, "", std::string("numkit:") + fn + ":tooManyDims");
    const ValueType t = x.type();
    if (t == ValueType::CELL || t == ValueType::STRUCT || t == ValueType::STRING
        || t == ValueType::FUNC_HANDLE)
        throw Error(std::string(fn) + ": ND fallback does not support type '"
                     + mtypeName(t) + "'",
                     0, 0, fn, "", std::string("numkit:") + fn + ":typeND");

    const size_t R = dd.rows(), C = dd.cols();
    size_t outDimArr[kMaxNd];
    for (int i = 0; i < nd; ++i) outDimArr[i] = dd.dim(i);
    auto r = Value::matrixND(outDimArr, nd, t, mr);
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

Value tril(const Value &x, int k, std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    if (dd.ndim() >= 4)
        return trilTriuND(x, k, trilPageBytes, "tril", mr);

    // Type-preserving: keep the lower triangle, zero-fill the rest. The
    // zeroed bit pattern is the canonical zero for every supported type
    // (DOUBLE / SINGLE / int / LOGICAL / CHAR / COMPLEX). CELL / STRING /
    // STRUCT rejected, matching MATLAB ("must be numeric, char, or logical").
    const ValueType t = x.type();
    if (t == ValueType::CELL || t == ValueType::STRUCT || t == ValueType::STRING
        || t == ValueType::FUNC_HANDLE)
        throw Error("tril: inputs must be numeric, char, or logical",
                     0, 0, "tril", "", "numkit:tril:badType");

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? Value::matrix3d(R, C, P, t, mr)
                       : Value::matrix(R, C, t, mr);
    if (x.numel() == 0) return r;
    const size_t es = elementSize(t);
    const char *src = static_cast<const char *>(x.rawData());
    char *dst = static_cast<char *>(r.rawDataMut());
    for (size_t pp = 0; pp < P; ++pp)
        trilPageBytes(src + pp * R * C * es, dst + pp * R * C * es, R, C, k, es);
    return r;
}

Value triu(const Value &x, int k, std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    if (dd.ndim() >= 4)
        return trilTriuND(x, k, triuPageBytes, "triu", mr);

    // Type-preserving (see tril).
    const ValueType t = x.type();
    if (t == ValueType::CELL || t == ValueType::STRUCT || t == ValueType::STRING
        || t == ValueType::FUNC_HANDLE)
        throw Error("triu: inputs must be numeric, char, or logical",
                     0, 0, "triu", "", "numkit:triu:badType");

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? Value::matrix3d(R, C, P, t, mr)
                       : Value::matrix(R, C, t, mr);
    if (x.numel() == 0) return r;
    const size_t es = elementSize(t);
    const char *src = static_cast<const char *>(x.rawData());
    char *dst = static_cast<char *>(r.rawDataMut());
    for (size_t pp = 0; pp < P; ++pp)
        triuPageBytes(src + pp * R * C * es, dst + pp * R * C * es, R, C, k, es);
    return r;
}

// ────────────────────────────────────────────────────────────────────
// flip — generalized N-D flip
// ────────────────────────────────────────────────────────────────────

Value flip(const Value &x, int dim1Based, std::pmr::memory_resource *mr)
{
    int axis;
    if (dim1Based <= 0) {
        // First non-singleton dim, 0-based.
        axis = 0;
        const auto &d = x.dims();
        for (int i = 0; i < d.ndim(); ++i) {
            if (d.dim(i) > 1) { axis = i; break; }
        }
    } else {
        axis = dim1Based - 1;
    }
    return flipNDAlongAxis(x, axis, "flip", mr);
}

// ────────────────────────────────────────────────────────────────────
// repelem — element-wise replication
// ────────────────────────────────────────────────────────────────────
//
// 1-D form: repelem(v, n) returns a vector where v(i) is repeated n
// times for each i. Output length = numel(v) * n. Result shape mirrors
// the input vector's row/column orientation (row → row, column →
// column, scalar → 1×n row).
//
// 2-D form: repelem(A, m, n) replaces each element a(i,j) with an
// m × n constant block of a(i,j) values. Output is (R*m) × (C*n).
//
// DOUBLE inputs only for now (covers ~all script use). Other types
// would need the typed-byte-copy treatment used by repmat / flip.

Value repelem(const Value &x, size_t n, std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    if (d.ndim() > 2 || (d.rows() != 1 && d.cols() != 1 && !x.isScalar()))
        throw Error("repelem: 1-arg form requires a vector input",
                     0, 0, "repelem", "", "numkit:repelem:notVector");
    if (x.type() != ValueType::DOUBLE)
        throw Error("repelem: only DOUBLE inputs are supported",
                     0, 0, "repelem", "", "numkit:repelem:type");

    const size_t inN = x.numel();
    const size_t outN = inN * n;
    // Preserve column-vector orientation; scalar → row vector of n.
    const bool isCol = (d.rows() > 1 && d.cols() == 1);
    auto r = isCol ? Value::matrix(outN, 1, ValueType::DOUBLE, mr)
                   : Value::matrix(1, outN, ValueType::DOUBLE, mr);
    if (outN == 0) return r;

    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    for (size_t i = 0; i < inN; ++i) {
        const double v = src[i];
        for (size_t k = 0; k < n; ++k)
            dst[i * n + k] = v;
    }
    return r;
}

Value repelem(const Value &x, size_t m, size_t n, std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    if (d.ndim() > 2)
        throw Error("repelem: 3-arg form is 2-D only",
                     0, 0, "repelem", "", "numkit:repelem:rank");
    if (x.type() != ValueType::DOUBLE)
        throw Error("repelem: only DOUBLE inputs are supported",
                     0, 0, "repelem", "", "numkit:repelem:type");

    const size_t R = d.rows(), C = d.cols();
    const size_t outR = R * m, outC = C * n;
    auto r = Value::matrix(outR, outC, ValueType::DOUBLE, mr);
    if (outR == 0 || outC == 0) return r;

    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    // Column-major: walk output by (outRow, outCol) and pull from
    // src(outRow / m, outCol / n).
    for (size_t oc = 0; oc < outC; ++oc) {
        const size_t srcCol = oc / n;
        for (size_t orow = 0; orow < outR; ++orow) {
            const size_t srcRow = orow / m;
            dst[oc * outR + orow] = src[srcCol * R + srcRow];
        }
    }
    return r;
}

namespace {

// Build the output→source index map for one dimension given a replication
// count spec. `counts` is a scalar (broadcast to all `dimLen` entries) or a
// DOUBLE vector of length `dimLen`. map[k] = source index for output slot k.
ScratchVec<size_t> repelemMap(const Value &counts, size_t dimLen, ScratchArena &arena)
{
    ScratchVec<size_t> map(&arena);
    auto checkCount = [](double c) -> size_t {
        if (!(c >= 0.0) || std::floor(c) != c)
            throw Error("repelem: replication counts must be nonnegative integers",
                         0, 0, "repelem", "", "numkit:repelem:badCount");
        return static_cast<size_t>(c);
    };
    if (counts.isScalar()) {
        const size_t rep = checkCount(counts.toScalar());
        map.reserve(dimLen * rep);
        for (size_t i = 0; i < dimLen; ++i)
            for (size_t k = 0; k < rep; ++k) map.push_back(i);
        return map;
    }
    if (counts.type() != ValueType::DOUBLE)
        throw Error("repelem: count vector must be DOUBLE",
                     0, 0, "repelem", "", "numkit:repelem:type");
    if (counts.numel() != dimLen)
        throw Error("repelem: count vector length must equal the dimension size",
                     0, 0, "repelem", "", "numkit:repelem:countLen");
    const double *cd = counts.doubleData();
    for (size_t i = 0; i < dimLen; ++i) {
        const size_t rep = checkCount(cd[i]);
        for (size_t k = 0; k < rep; ++k) map.push_back(i);
    }
    return map;
}

} // namespace

// repelem(v, counts): per-element counts (scalar or vector the length of v).
Value repelem(const Value &x, const Value &counts, std::pmr::memory_resource *mr)
{
    if (counts.isScalar())   // fast path / preserves scalar-form semantics
        return repelem(x, static_cast<size_t>(counts.toScalar()), mr);

    const auto &d = x.dims();
    if (d.ndim() > 2 || (d.rows() != 1 && d.cols() != 1 && !x.isScalar()))
        throw Error("repelem: vector-count form requires a vector input",
                     0, 0, "repelem", "", "numkit:repelem:notVector");
    if (x.type() != ValueType::DOUBLE)
        throw Error("repelem: only DOUBLE inputs are supported",
                     0, 0, "repelem", "", "numkit:repelem:type");

    ScratchArena arena(mr);
    auto map = repelemMap(counts, x.numel(), arena);
    const size_t outN = map.size();
    const bool isCol = (d.rows() > 1 && d.cols() == 1);
    auto r = isCol ? Value::matrix(outN, 1, ValueType::DOUBLE, mr)
                   : Value::matrix(1, outN, ValueType::DOUBLE, mr);
    if (outN == 0) return r;

    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    for (size_t k = 0; k < outN; ++k) dst[k] = src[map[k]];
    return r;
}

// repelem(A, r, c): per-row / per-column counts (each scalar or vector).
Value repelem(const Value &x, const Value &rCounts, const Value &cCounts,
              std::pmr::memory_resource *mr)
{
    if (rCounts.isScalar() && cCounts.isScalar())   // fast path
        return repelem(x, static_cast<size_t>(rCounts.toScalar()),
                          static_cast<size_t>(cCounts.toScalar()), mr);

    const auto &d = x.dims();
    if (d.ndim() > 2)
        throw Error("repelem: 3-arg form is 2-D only",
                     0, 0, "repelem", "", "numkit:repelem:rank");
    if (x.type() != ValueType::DOUBLE)
        throw Error("repelem: only DOUBLE inputs are supported",
                     0, 0, "repelem", "", "numkit:repelem:type");

    const size_t R = d.rows(), C = d.cols();
    ScratchArena arena(mr);
    auto rowMap = repelemMap(rCounts, R, arena);
    auto colMap = repelemMap(cCounts, C, arena);
    const size_t outR = rowMap.size(), outC = colMap.size();
    auto r = Value::matrix(outR, outC, ValueType::DOUBLE, mr);
    if (outR == 0 || outC == 0) return r;

    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    for (size_t oc = 0; oc < outC; ++oc) {
        const size_t srcCol = colMap[oc];
        for (size_t orow = 0; orow < outR; ++orow)
            dst[oc * outR + orow] = src[srcCol * R + rowMap[orow]];
    }
    return r;
}

// ────────────────────────────────────────────────────────────────────
// Pack 32: paddata / trimdata / resize
// ────────────────────────────────────────────────────────────────────
//
// Vector-only for now; row/column orientation is preserved. Numeric
// inputs only — the pad cell is `0` (matches MATLAB R2024 default).

namespace {
inline bool isVectorLike(const Value &v)
{
    const auto &d = v.dims();
    // Accept scalars, 1×N / N×1 vectors, and the genuinely-empty 0×0
    // (so paddata([], n) doesn't throw — it just expands the empty
    // store with zeros).
    return d.ndim() <= 2 && (d.rows() <= 1 || d.cols() <= 1 || v.isScalar());
}
} // anon

Value paddata(const Value &v, size_t n, std::pmr::memory_resource *mr)
{
    if (!isVectorLike(v))
        throw Error("paddata: vector input required",
                     0, 0, "paddata", "", "numkit:paddata:notVector");
    if (!v.isEmpty() && v.type() != ValueType::DOUBLE)
        throw Error("paddata: only DOUBLE inputs supported",
                     0, 0, "paddata", "", "numkit:paddata:type");
    const size_t cur = v.numel();
    if (cur >= n) return v;
    const auto &d = v.dims();
    const bool col = (d.cols() == 1 && d.rows() > 1);
    auto out = col ? Value::matrix(n, 1, ValueType::DOUBLE, mr)
                   : Value::matrix(1, n, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    if (cur > 0) std::memcpy(dst, v.doubleData(), cur * sizeof(double));
    for (size_t i = cur; i < n; ++i) dst[i] = 0.0;
    return out;
}

Value trimdata(const Value &v, size_t n, std::pmr::memory_resource *mr)
{
    if (!isVectorLike(v))
        throw Error("trimdata: vector input required",
                     0, 0, "trimdata", "", "numkit:trimdata:notVector");
    if (!v.isEmpty() && v.type() != ValueType::DOUBLE)
        throw Error("trimdata: only DOUBLE inputs supported",
                     0, 0, "trimdata", "", "numkit:trimdata:type");
    const size_t cur = v.numel();
    if (cur <= n) return v;
    const auto &d = v.dims();
    const bool col = (d.cols() == 1 && d.rows() > 1);
    auto out = col ? Value::matrix(n, 1, ValueType::DOUBLE, mr)
                   : Value::matrix(1, n, ValueType::DOUBLE, mr);
    if (n > 0) std::memcpy(out.doubleDataMut(), v.doubleData(), n * sizeof(double));
    return out;
}

Value resize(const Value &v, size_t n, std::pmr::memory_resource *mr)
{
    return (v.numel() < n) ? paddata(v, n, mr) : trimdata(v, n, mr);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════
namespace detail {

void repmat_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("repmat: requires at least 2 arguments",
                     0, 0, "repmat", "", "numkit:repmat:nargin");
    auto *mr = ctx.engine->resource();

    // Forms:
    //   repmat(A, n)            → m=n=arg
    //   repmat(A, [m n])        → vector
    //   repmat(A, [m n p ...])  → vector (any length ≥ 1)
    //   repmat(A, m, n)         → two scalars
    //   repmat(A, m, n, p, ...) → ≥ 2 scalars
    ScratchArena scratch(mr);
    auto tiles = ScratchVec<size_t>(&scratch);
    if (args.size() == 2) {
        const Value &v = args[1];
        const size_t k = v.numel();
        if (k == 0) {
            throw Error("repmat: tile vector must not be empty",
                         0, 0, "repmat", "", "numkit:repmat:badTileVec");
        }
        if (k == 1) {
            const size_t s = static_cast<size_t>(v.toScalar());
            tiles.assign({s, s});
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
    if (outNdim <= 3 && tiles.size() <= 3 && args[0].type() == ValueType::DOUBLE) {
        const size_t m = tiles[0];
        const size_t n = tiles.size() >= 2 ? tiles[1] : 1;
        const size_t p = tiles.size() >= 3 ? tiles[2] : 1;
        outs[0] = repmat(args[0], m, n, p, mr);
    } else {
        outs[0] = repmatND(args[0], Span<const size_t>(tiles.data(), tiles.size()), mr);
    }
}

#define NK_FLIP_REG(name)                                                      \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,               \
                    Span<Value> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.empty())                                                      \
            throw Error(#name ": requires 1 argument",                        \
                         0, 0, #name, "", "numkit:" #name ":nargin");               \
        outs[0] = name(args[0], ctx.engine->resource());                      \
    }

NK_FLIP_REG(fliplr)
NK_FLIP_REG(flipud)

#undef NK_FLIP_REG

void rot90_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw Error("rot90: requires at least 1 argument",
                     0, 0, "rot90", "", "numkit:rot90:nargin");
    int k = (args.size() >= 2 && !args[1].isEmpty())
                ? static_cast<int>(args[1].toScalar())
                : 1;
    outs[0] = rot90(args[0], k, ctx.engine->resource());
}

void circshift_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("circshift: requires (X, k) or (X, shiftVec)",
                     0, 0, "circshift", "", "numkit:circshift:nargin");
    const Value &k = args[1];
    auto *mr = ctx.engine->resource();
    const size_t nk = k.numel();
    if (nk == 0)
        throw Error("circshift: shift vector must not be empty",
                     0, 0, "circshift", "", "numkit:circshift:badShift");

    if (nk == 1) {
        const int64_t kk = static_cast<int64_t>(k.toScalar());
        // circshift(X, K, dim): shift by K ONLY along dimension `dim`. The
        // previous code ignored args[2] and always shifted dim 1.
        if (args.size() >= 3 && !args[2].isEmpty()) {
            const int dim = static_cast<int>(args[2].toScalar());
            if (dim < 1)
                throw Error("circshift: dim must be a positive integer",
                             0, 0, "circshift", "", "numkit:circshift:badDim");
            ScratchArena scratch(mr);
            auto shifts = ScratchVec<int64_t>(static_cast<size_t>(dim), &scratch);
            for (int i = 0; i < dim; ++i) shifts[i] = 0;
            shifts[dim - 1] = kk;
            outs[0] = circshiftND(args[0],
                                  Span<const int64_t>(shifts.data(), dim), mr);
            return;
        }
        outs[0] = circshift(args[0], kk, mr);
        return;
    }
    if (nk == 2 && args[0].dims().ndim() <= 3) {
        outs[0] = circshift(args[0], static_cast<int64_t>(k.doubleData()[0]), static_cast<int64_t>(k.doubleData()[1]), mr);
        return;
    }
    // ND path: shift vector ≥ 3 entries OR input rank ≥ 4.
    ScratchArena scratch(mr);
    auto shifts = ScratchVec<int64_t>(nk, &scratch);
    for (size_t i = 0; i < nk; ++i)
        shifts[i] = static_cast<int64_t>(k.doubleData()[i]);
    outs[0] = circshiftND(args[0], Span<const int64_t>(shifts.data(), nk), mr);
}

#define NK_TRI_REG(name)                                                       \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,               \
                    Span<Value> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.empty())                                                      \
            throw Error(#name ": requires at least 1 argument",               \
                         0, 0, #name, "", "numkit:" #name ":nargin");               \
        int k = (args.size() >= 2 && !args[1].isEmpty())                       \
                    ? static_cast<int>(args[1].toScalar())                     \
                    : 0;                                                        \
        outs[0] = name(args[0], k, ctx.engine->resource());                   \
    }

NK_TRI_REG(tril)
NK_TRI_REG(triu)

#undef NK_TRI_REG

void flip_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("flip: requires at least 1 argument",
                     0, 0, "flip", "", "numkit:flip:nargin");
    int dim = (args.size() >= 2 && !args[1].isEmpty())
                  ? static_cast<int>(args[1].toScalar())
                  : 0;
    outs[0] = flip(args[0], dim, ctx.engine->resource());
}

void repelem_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("repelem: requires at least 2 arguments",
                     0, 0, "repelem", "", "numkit:repelem:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        // counts may be a scalar or a per-element vector — the Value
        // overload dispatches internally.
        outs[0] = repelem(args[0], args[1], mr);
        return;
    }
    // r / c may each be a scalar or a per-row / per-column vector.
    outs[0] = repelem(args[0], args[1], args[2], mr);
}

// sub2ind(siz, i1, i2, ...) → linear index. Column-major, 1-based.
void sub2ind_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("sub2ind: requires siz and at least 1 subscript",
                     0, 0, "sub2ind", "", "numkit:sub2ind:nargin");
    auto *mr = ctx.engine->resource();
    const Value &siz = args[0];
    const size_t nDims = siz.numel();
    if (nDims == 0)
        throw Error("sub2ind: siz must not be empty",
                     0, 0, "sub2ind", "", "numkit:sub2ind:badSiz");

    ScratchArena scratch(mr);
    auto dims = ScratchVec<size_t>(nDims, &scratch);
    for (size_t i = 0; i < nDims; ++i)
        dims[i] = static_cast<size_t>(siz.doubleData()[i]);

    // Subscript args. Pad missing higher-dim subs with 1.
    const size_t nSubs = args.size() - 1;
    if (nSubs > nDims)
        throw Error("sub2ind: too many subscript arrays for given siz",
                     0, 0, "sub2ind", "", "numkit:sub2ind:tooManySubs");

    // All sub arrays must agree on shape; result inherits that shape.
    const Value &shapeRef = args[1];
    const size_t outN = shapeRef.numel();
    for (size_t a = 1; a < args.size(); ++a) {
        if (args[a].numel() != outN)
            throw Error("sub2ind: subscript arrays must be the same size",
                         0, 0, "sub2ind", "", "numkit:sub2ind:shape");
    }

    auto r = (shapeRef.isScalar())
                ? Value::scalar(0.0, mr)
                : Value::matrix(shapeRef.dims().rows(), shapeRef.dims().cols(),
                                ValueType::DOUBLE, mr);
    double *dst = r.doubleDataMut();
    // Strides for column-major: stride[0]=1, stride[1]=dim0, stride[2]=dim0*dim1.
    auto strideOfDim = [&](size_t d) {
        size_t s = 1;
        for (size_t i = 0; i < d && i < nDims; ++i) s *= dims[i];
        return s;
    };
    for (size_t k = 0; k < outN; ++k) {
        size_t lin = 0;
        for (size_t d = 0; d < nSubs; ++d) {
            const double sd = args[d + 1].isScalar()
                                  ? args[d + 1].toScalar()
                                  : args[d + 1].doubleData()[k];
            const size_t idx = static_cast<size_t>(sd) - 1;  // 1-based → 0-based
            lin += idx * strideOfDim(d);
        }
        dst[k] = static_cast<double>(lin + 1);  // back to 1-based
    }
    outs[0] = std::move(r);
}

// paddata / trimdata / resize adapters.
#define NK_RESIZE_REG(FN)                                                       \
    void FN##_reg(Span<const Value> args, size_t /*nargout*/,                  \
                  Span<Value> outs, CallContext &ctx)                           \
    {                                                                            \
        if (args.size() < 2)                                                     \
            throw Error(#FN " requires (v, n)",                                  \
                         0, 0, #FN, "", "numkit:" #FN ":nargin");                     \
        const size_t n = static_cast<size_t>(args[1].toScalar());                \
        outs[0] = FN(args[0], n, ctx.engine->resource());                       \
    }

NK_RESIZE_REG(paddata)
NK_RESIZE_REG(trimdata)
NK_RESIZE_REG(resize)

#undef NK_RESIZE_REG

// ind2sub(siz, ind) → multiple outputs (one per dim of siz). When the
// caller requests fewer outputs than siz has dims, the last output
// absorbs trailing dims (column-major linear index of the remainder).
void ind2sub_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ind2sub: requires siz and ind",
                     0, 0, "ind2sub", "", "numkit:ind2sub:nargin");
    auto *mr = ctx.engine->resource();
    const Value &siz = args[0];
    const Value &ind = args[1];
    const size_t nDims = siz.numel();
    if (nDims == 0)
        throw Error("ind2sub: siz must not be empty",
                     0, 0, "ind2sub", "", "numkit:ind2sub:badSiz");

    ScratchArena scratch(mr);
    auto dims = ScratchVec<size_t>(nDims, &scratch);
    for (size_t i = 0; i < nDims; ++i)
        dims[i] = static_cast<size_t>(siz.doubleData()[i]);

    const size_t outDims = std::max<size_t>(nargout, 1);
    const size_t outN = ind.numel();

    // Build effective dim list: outDims entries. The first outDims-1
    // match siz; the last absorbs the product of remaining dims.
    auto effDim = [&](size_t d) -> size_t {
        if (d + 1 < outDims) return d < nDims ? dims[d] : 1;
        // Last output: absorb everything from d..nDims-1.
        size_t r = 1;
        for (size_t i = d; i < nDims; ++i) r *= dims[i];
        return r ? r : 1;
    };
    // Strides from outDims dim list.
    ScratchVec<size_t> stride(outDims, &scratch);
    {
        size_t s = 1;
        for (size_t d = 0; d < outDims; ++d) {
            stride[d] = s;
            s *= effDim(d);
        }
    }

    auto makeLike = [&]() {
        return ind.isScalar()
                   ? Value::scalar(0.0, mr)
                   : Value::matrix(ind.dims().rows(), ind.dims().cols(),
                                   ValueType::DOUBLE, mr);
    };
    ScratchVec<Value> rs(&scratch);
    rs.reserve(outDims);
    for (size_t i = 0; i < outDims; ++i) rs.emplace_back(makeLike());

    for (size_t k = 0; k < outN; ++k) {
        const double iv = ind.isScalar() ? ind.toScalar() : ind.doubleData()[k];
        size_t lin = static_cast<size_t>(iv) - 1;
        for (size_t d = 0; d < outDims; ++d) {
            const size_t v = (d + 1 < outDims)
                                 ? (lin / stride[d]) % effDim(d)
                                 : (lin / stride[d]);
            rs[d].doubleDataMut()[k] = static_cast<double>(v + 1);
        }
    }

    for (size_t i = 0; i < outDims && i < outs.size(); ++i)
        outs[i] = std::move(rs[i]);
}

} // namespace detail

} // namespace numkit::builtin
