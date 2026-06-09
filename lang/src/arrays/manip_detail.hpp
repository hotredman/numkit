// toolboxes/.../manip_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by manip.cpp + manip_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include "reduction_helpers.hpp"  // engine-free numkit::builtin::detail dim-infra (ops re-export)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::builtin {

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

} // namespace numkit::builtin
