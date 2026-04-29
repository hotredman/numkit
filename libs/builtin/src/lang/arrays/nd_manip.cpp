// libs/builtin/src/lang/arrays/nd_manip.cpp
//
// Phase 6: N-D array manipulation. numkit-m's Value is capped at
// 3D so the perm vector must have length 2 or 3. The general
// approach: compute output dims from input dims via the permutation,
// then iterate over output indices and compute the corresponding
// input index. Pure scalar gather; no SIMD opportunity in the
// general case (input strides differ per axis after permutation).

#include <numkit/builtin/lang/arrays/nd_manip.hpp>

#include <numkit/builtin/lang/arrays/matrix.hpp>  // reshape, horzcat, vertcat
#include <numkit/core/engine.hpp>
#include <numkit/core/scratch_arena.hpp>
#include <numkit/core/shape_ops.hpp>      // computeStridesColMajor, incrementCoords
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <cstring>
#include <numeric>
#include <vector>

namespace numkit::builtin {

namespace {

// Check the perm list is a valid 1-based permutation of [1..N].
// Throws on bad input. Stack-mounted scratch (perm length is bounded
// by Dims::kMaxRank); no heap traffic.
void validatePerm(const int *perm, std::size_t N, const char *fn)
{
    if (N == 0)
        throw Error(std::string(fn) + ": perm vector must not be empty",
                     0, 0, fn, "", std::string("m:") + fn + ":emptyPerm");
    if (N > Dims::kMaxRank)
        throw Error(std::string(fn) + ": perm length exceeds 32",
                     0, 0, fn, "", std::string("m:") + fn + ":tooManyDims");
    int sorted[Dims::kMaxRank];
    for (std::size_t i = 0; i < N; ++i) sorted[i] = perm[i];
    std::sort(sorted, sorted + N);
    for (std::size_t i = 0; i < N; ++i) {
        if (sorted[i] != static_cast<int>(i + 1))
            throw Error(std::string(fn) + ": perm must be a permutation of 1..N",
                         0, 0, fn, "", std::string("m:") + fn + ":badPerm");
    }
}

} // namespace

namespace {

// Phase P6: cache-blocked transpose for the per-page [2 1] perm. The
// straight strided gather (read down columns of input, write across
// rows of output) thrashes L1 — at R=512 doubles per column, every
// load misses cache for the first column-tile of each output row. A
// 32×32 block fits in L1 (8 KB), so an inner block transpose copies
// each input element exactly once with locality on both sides.
constexpr size_t TRANSPOSE_BLOCK = 32;

void transposePage(const double *src, double *dst, size_t inR, size_t inC)
{
    const size_t outR = inC; // output rows = input cols
    // Block over output coordinates so that the inner write loop is
    // CONTIGUOUS in the destination (stride 1 along dst column). Reads
    // from src are then strided by inR but stay within a small block,
    // so they're cached after the first access in each block. This
    // minimises write traffic — strided writes evict the write-combiner
    // and require read-for-ownership per cache line, which is the
    // dominant cost in the unblocked transpose.
    for (size_t rBlk = 0; rBlk < inR; rBlk += TRANSPOSE_BLOCK) {
        const size_t rEnd = std::min(rBlk + TRANSPOSE_BLOCK, inR);
        for (size_t cBlk = 0; cBlk < inC; cBlk += TRANSPOSE_BLOCK) {
            const size_t cEnd = std::min(cBlk + TRANSPOSE_BLOCK, inC);
            // Inner block: dst column r holds input row r.
            // For fixed r, vary c: dst write is stride 1 in c.
            for (size_t r = rBlk; r < rEnd; ++r) {
                double *dstCol = dst + r * outR;
                for (size_t c = cBlk; c < cEnd; ++c) {
                    dstCol[c] = src[c * inR + r];
                }
            }
        }
    }
}

} // namespace

// ────────────────────────────────────────────────────────────────────
// permute / ipermute
// ────────────────────────────────────────────────────────────────────
//
// Semantics (MATLAB):
//   B(i_1, i_2, ..., i_N) = A(i_{p_1}, i_{p_2}, ..., i_{p_N})
// i.e. output axis k corresponds to input axis perm[k]. So the size
// of output along axis k equals the size of input along perm[k].
Value permute(Allocator &alloc, const Value &x, const int *perm, std::size_t n)
{
    validatePerm(perm, n, "permute");

    const auto &dd = x.dims();
    const int inNd = std::max<int>(dd.ndim(), static_cast<int>(n));
    if (inNd > Dims::kMaxRank)
        throw Error("permute: rank exceeds 32",
                     0, 0, "permute", "", "m:permute:tooManyDims");

    // All shape arrays on the stack — no per-call heap traffic. Avoids
    // 4 std::vector allocs that dominated cost at small sizes (post-ND
    // generalisation regressed BM_Permute3D /16 by 1.77× before this).
    int p[Dims::kMaxRank];
    size_t inDims[Dims::kMaxRank];
    size_t outDimsArr[Dims::kMaxRank];
    for (int i = 0; i < inNd; ++i)
        p[i] = (i < static_cast<int>(n)) ? perm[i] : (i + 1);
    for (int i = 0; i < inNd; ++i) inDims[i] = dd.dim(i);
    for (int k = 0; k < inNd; ++k) outDimsArr[k] = inDims[p[k] - 1];

    // 2D / 3D fast path uses createMatrix / createMatrix3d via matrixND;
    // ≥ 4D goes through Value::matrixND. Trailing 1s are kept.
    auto r = Value::matrixND(outDimsArr, inNd, ValueType::DOUBLE, &alloc);
    if (x.numel() == 0) return r;

    const double *src = x.doubleData();
    double *dst       = r.doubleDataMut();

    // Phase P6 fast path: per-page transpose ([2, 1] or [2, 1, 3] for
    // ≤ 3D inputs). 2.3× win on 512×512 (cache-blocked vs strided gather).
    // For ND inputs, iterate over the trailing pages of the first 2 axes.
    if (inNd >= 2 && p[0] == 2 && p[1] == 1) {
        bool tailIsIdentity = true;
        for (int k = 2; k < inNd; ++k)
            if (p[k] != k + 1) { tailIsIdentity = false; break; }
        if (tailIsIdentity) {
            const size_t inR = inDims[0], inC = inDims[1];
            size_t P = 1;
            for (int k = 2; k < inNd; ++k) P *= inDims[k];
            for (size_t pp = 0; pp < P; ++pp)
                transposePage(src + pp * inR * inC,
                              dst + pp * inC * inR,
                              inR, inC);
            return r;
        }
    }

    // 3D fast path — explicit nested loops with constant strides. Avoids
    // the per-element coord-walk overhead that dominated pre-fix at
    // small sizes (BM_Permute3D /16: 7.5us → expected ≤ 5us).
    if (inNd == 3) {
        const size_t s[3] = {1, inDims[0], inDims[0] * inDims[1]};
        const size_t sa = s[p[0] - 1];
        const size_t sb = s[p[1] - 1];
        const size_t sc = s[p[2] - 1];
        const size_t outR = outDimsArr[0], outC = outDimsArr[1], outP = outDimsArr[2];
        size_t dstIdx = 0;
        for (size_t k = 0; k < outP; ++k) {
            const size_t baseK = k * sc;
            for (size_t j = 0; j < outC; ++j) {
                const size_t baseJK = baseK + j * sb;
                for (size_t i = 0; i < outR; ++i)
                    dst[dstIdx++] = src[baseJK + i * sa];
            }
        }
        return r;
    }

    // 2D fast path — same idea, two nested loops.
    if (inNd == 2) {
        const size_t s[2] = {1, inDims[0]};
        const size_t sa = s[p[0] - 1];
        const size_t sb = s[p[1] - 1];
        const size_t outR = outDimsArr[0], outC = outDimsArr[1];
        size_t dstIdx = 0;
        for (size_t j = 0; j < outC; ++j) {
            const size_t baseJ = j * sb;
            for (size_t i = 0; i < outR; ++i)
                dst[dstIdx++] = src[baseJ + i * sa];
        }
        return r;
    }

    // General ND permute (4D+) — strided gather via incrementCoords.
    size_t inStrides[Dims::kMaxRank];
    computeStridesColMajor(Dims(inDims, inNd), inStrides);

    size_t outCoords[Dims::kMaxRank] = {0};
    Dims outDimsObj(outDimsArr, inNd);
    size_t dstOff = 0;
    do {
        size_t srcOff = 0;
        for (int k = 0; k < inNd; ++k) {
            const int axIn = p[k] - 1;
            srcOff += outCoords[k] * inStrides[axIn];
        }
        dst[dstOff] = src[srcOff];
        ++dstOff;
    } while (incrementCoords(outCoords, outDimsObj));
    return r;
}

Value ipermute(Allocator &alloc, const Value &x, const int *perm, std::size_t n)
{
    validatePerm(perm, n, "ipermute");
    // Compute inverse permutation: invPerm[perm[i] - 1] = i + 1.
    // Stack-mounted: perm length is bounded by Dims::kMaxRank.
    int inv[Dims::kMaxRank];
    for (std::size_t i = 0; i < n; ++i)
        inv[perm[i] - 1] = static_cast<int>(i + 1);
    return permute(alloc, x, inv, n);
}

// ────────────────────────────────────────────────────────────────────
// squeeze
// ────────────────────────────────────────────────────────────────────
//
// MATLAB squeeze removes singleton dimensions but leaves scalars and
// 2D matrices alone (it never reduces below 2D). So:
//   1×3×4  → 3×4
//   3×1×4  → 3×4
//   3×4×1  → 3×4 (already effectively 2D)
//   3×4    → 3×4 (no-op)
//   1×3    → 1×3 (no-op, MATLAB convention)
//
// Since the underlying data is the same (column-major + page stride),
// squeeze is essentially reshape-with-new-dims. For 1×3×4 this means
// the data layout still matches: the singleton dim collapses cleanly
// because a stride of 1 doesn't introduce gaps.
Value squeeze(Allocator &alloc, const Value &x)
{
    const auto &dd = x.dims();
    const int nd = dd.ndim();

    // 2D and below: shape preserved (MATLAB never collapses below 2D).
    if (nd <= 2)
        return reshape(alloc, x, dd.rows(), dd.cols(), 0);

    // ND (≥ 3): drop every singleton dim, preserve the rest in order.
    // Pad to at least 2 dims with trailing 1s so a fully-singleton input
    // (1×1×1, 1×1×1×1, etc.) collapses to scalar shape (1×1) rather than
    // an invalid 0D shape.
    ScratchArena scratch(alloc);
    auto kept = scratch.vec<size_t>();
    kept.reserve(nd);
    for (int i = 0; i < nd; ++i) {
        const size_t d = dd.dim(i);
        if (d != 1) kept.push_back(d);
    }
    while (kept.size() < 2) kept.push_back(1);

    return reshapeND(alloc, x, kept.data(), kept.size());
}

// ────────────────────────────────────────────────────────────────────
// cat(dim, ...)
// ────────────────────────────────────────────────────────────────────
//
// dim=1 → vertcat (delegate); dim=2 → horzcat (delegate); dim=3 →
// stack 2D pages or extend 3D page count. Other dims throw.
namespace {

Value catDim3(Allocator &alloc, const Value *values, size_t count)
{
    if (count == 0) return Value::empty();

    // First non-empty input fixes (R, C); empties are tolerated and
    // skipped (matches MATLAB).
    size_t R = 0, C = 0;
    bool anchored = false;
    size_t totalPages = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto &v = values[i];
        if (v.isEmpty() || v.numel() == 0) continue;
        const auto &dd = v.dims();
        if (!anchored) {
            R = dd.rows();
            C = dd.cols();
            anchored = true;
        } else {
            if (dd.rows() != R || dd.cols() != C)
                throw Error("cat: dim 3 inputs must agree on rows and cols",
                             0, 0, "cat", "", "m:cat:badDims");
        }
        totalPages += dd.is3D() ? dd.pages() : 1;
    }
    if (!anchored) return Value::empty();

    auto r = Value::matrix3d(R, C, totalPages, ValueType::DOUBLE, &alloc);
    double *dst = r.doubleDataMut();
    size_t pageOff = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto &v = values[i];
        if (v.isEmpty() || v.numel() == 0) continue;
        const auto &dd = v.dims();
        const size_t P = dd.is3D() ? dd.pages() : 1;
        std::memcpy(dst + pageOff * R * C, v.doubleData(),
                    R * C * P * sizeof(double));
        pageOff += P;
    }
    return r;
}

// ND cat for dim >= 4. All non-cat axes must agree across inputs (treating
// ranks past an input's actual ndim as trailing 1s). Result rank =
// max(dim, max input ndim). All numeric types supported via byte-copy
// (elementSize-based). All inputs must share a type (no implicit
// promotion). CELL / STRUCT / STRING / FUNC_HANDLE rejected.
Value catND(Allocator &alloc, int dim, const Value *values, size_t count)
{
    if (count == 0) return Value::empty();
    const int k = dim - 1;

    // Determine output rank: at least `dim`, plus any input may force higher.
    int outNdim = dim;
    for (size_t i = 0; i < count; ++i) {
        const auto &v = values[i];
        if (v.isEmpty() || v.numel() == 0) continue;
        outNdim = std::max(outNdim, v.dims().ndim());
    }
    constexpr int kMaxNd = Dims::kMaxRank;
    if (outNdim > kMaxNd)
        throw Error("cat: rank exceeds 32",
                     0, 0, "cat", "", "m:cat:tooManyDims");

    size_t outDim[kMaxNd];
    for (int j = 0; j < outNdim; ++j) outDim[j] = 0;
    bool anchored = false;
    ValueType outType = ValueType::DOUBLE;

    for (size_t i = 0; i < count; ++i) {
        const auto &v = values[i];
        if (v.isEmpty() || v.numel() == 0) continue;
        const ValueType t = v.type();
        if (t == ValueType::CELL || t == ValueType::STRUCT || t == ValueType::STRING
            || t == ValueType::FUNC_HANDLE)
            throw Error(std::string("cat: ND cat does not support type '")
                         + mtypeName(t) + "'",
                         0, 0, "cat", "", "m:cat:typeND");
        const auto &d = v.dims();
        if (!anchored) {
            for (int j = 0; j < outNdim; ++j)
                outDim[j] = (j < d.ndim()) ? d.dim(j) : 1;
            outType = t;
            anchored = true;
        } else {
            if (t != outType)
                throw Error("cat: ND cat requires all inputs to share a type",
                             0, 0, "cat", "", "m:cat:typeMismatchND");
            for (int j = 0; j < outNdim; ++j) {
                const size_t vd = (j < d.ndim()) ? d.dim(j) : 1;
                if (j == k) {
                    outDim[j] += vd;
                } else if (vd != outDim[j]) {
                    throw Error("cat: dim " + std::to_string(dim)
                                 + " inputs must agree on all axes except dim "
                                 + std::to_string(dim),
                                 0, 0, "cat", "", "m:cat:badDims");
                }
            }
        }
    }
    if (!anchored) return Value::empty();

    // Inner block size B = prod(outDim[0..k-1]); outer count O = prod(outDim[k+1..]).
    size_t B = 1;
    for (int j = 0; j < k; ++j) B *= outDim[j];
    size_t O = 1;
    for (int j = k + 1; j < outNdim; ++j) O *= outDim[j];

    auto result = Value::matrixND(outDim, outNdim, outType, &alloc);
    if (B == 0 || O == 0 || outDim[k] == 0) return result;

    const size_t es = elementSize(outType);
    char *dst = static_cast<char *>(result.rawDataMut());
    const size_t resultOuterStride = outDim[k] * B;
    size_t accumK = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto &v = values[i];
        if (v.isEmpty() || v.numel() == 0) continue;
        const auto &d = v.dims();
        const size_t inputDimK = (k < d.ndim()) ? d.dim(k) : 1;
        if (inputDimK == 0) continue;
        const char *src = static_cast<const char *>(v.rawData());
        const size_t inputOuterStride = inputDimK * B;
        const size_t blockBytes = inputDimK * B * es;
        for (size_t o = 0; o < O; ++o) {
            std::memcpy(dst + (o * resultOuterStride + accumK * B) * es,
                        src + o * inputOuterStride * es,
                        blockBytes);
        }
        accumK += inputDimK;
    }
    return result;
}

} // namespace

Value cat(Allocator &alloc, int dim, const Value *values, size_t count)
{
    if (dim < 1)
        throw Error("cat: dim must be a positive integer",
                     0, 0, "cat", "", "m:cat:badDim");
    switch (dim) {
        case 1: return vertcat(alloc, values, count);
        case 2: return horzcat(alloc, values, count);
        case 3: return catDim3(alloc, values, count);
        default: return catND(alloc, dim, values, count);
    }
}

// ────────────────────────────────────────────────────────────────────
// blkdiag
// ────────────────────────────────────────────────────────────────────
//
// Block-diagonal matrix: diagonal blocks are the inputs (in order),
// off-diagonal regions are zero. 2D inputs only.
Value blkdiag(Allocator &alloc, const Value *values, size_t count)
{
    if (count == 0) return Value::empty();

    size_t totalRows = 0, totalCols = 0;
    for (size_t i = 0; i < count; ++i) {
        if (values[i].dims().is3D())
            throw Error("blkdiag: 3D inputs are not supported",
                         0, 0, "blkdiag", "", "m:blkdiag:3D");
        totalRows += values[i].dims().rows();
        totalCols += values[i].dims().cols();
    }
    auto r = Value::matrix(totalRows, totalCols, ValueType::DOUBLE, &alloc);
    double *dst = r.doubleDataMut();
    // Zero-init: matrix() returns an uninitialised buffer in some
    // builds; explicit clear is safe and cheap (we'll write the block
    // regions over the top).
    std::fill(dst, dst + totalRows * totalCols, 0.0);

    size_t rowOff = 0, colOff = 0;
    for (size_t k = 0; k < count; ++k) {
        const auto &v = values[k];
        const size_t R = v.dims().rows(), C = v.dims().cols();
        const double *src = v.doubleData();
        for (size_t c = 0; c < C; ++c) {
            const size_t dstColStart = (colOff + c) * totalRows + rowOff;
            std::memcpy(dst + dstColStart, src + c * R, R * sizeof(double));
        }
        rowOff += R;
        colOff += C;
    }
    return r;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════
namespace detail {

namespace {

ScratchVec<int> permFromValue(std::pmr::memory_resource *mr, const Value &v)
{
    ScratchVec<int> p(mr);
    p.reserve(v.numel());
    for (size_t i = 0; i < v.numel(); ++i)
        p.push_back(static_cast<int>(v.doubleData()[i]));
    return p;
}

} // namespace

void permute_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("permute: requires (A, perm)",
                     0, 0, "permute", "", "m:permute:nargin");
    auto &alloc = ctx.engine->allocator();
    ScratchArena scratch(alloc);
    auto perm = permFromValue(scratch.resource(), args[1]);
    outs[0] = permute(alloc, args[0], perm.data(), perm.size());
}

void ipermute_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ipermute: requires (A, perm)",
                     0, 0, "ipermute", "", "m:ipermute:nargin");
    auto &alloc = ctx.engine->allocator();
    ScratchArena scratch(alloc);
    auto perm = permFromValue(scratch.resource(), args[1]);
    outs[0] = ipermute(alloc, args[0], perm.data(), perm.size());
}

void squeeze_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.empty())
        throw Error("squeeze: requires 1 argument",
                     0, 0, "squeeze", "", "m:squeeze:nargin");
    outs[0] = squeeze(ctx.engine->allocator(), args[0]);
}

void cat_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cat: requires (dim, A, ...)",
                     0, 0, "cat", "", "m:cat:nargin");
    const int dim = static_cast<int>(args[0].toScalar());
    // Pass &args[1] as the start of the values array.
    outs[0] = cat(ctx.engine->allocator(), dim,
                  &args[1], args.size() - 1);
}

void blkdiag_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    outs[0] = blkdiag(ctx.engine->allocator(), args.data(), args.size());
}

} // namespace detail

} // namespace numkit::builtin
