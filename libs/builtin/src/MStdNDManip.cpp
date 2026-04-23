// libs/builtin/src/MStdNDManip.cpp
//
// Phase 6: N-D array manipulation. numkit-m's MValue is capped at
// 3D so the perm vector must have length 2 or 3. The general
// approach: compute output dims from input dims via the permutation,
// then iterate over output indices and compute the corresponding
// input index. Pure scalar gather; no SIMD opportunity in the
// general case (input strides differ per axis after permutation).

#include <numkit/m/builtin/MStdNDManip.hpp>

#include <numkit/m/builtin/MStdMatrix.hpp>  // reshape, horzcat, vertcat
#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <algorithm>
#include <cstring>
#include <numeric>
#include <vector>

namespace numkit::m::builtin {

namespace {

// Check the perm vector is a valid 1-based permutation of [1..N].
// Throws on bad input. Returns N for caller convenience.
size_t validatePerm(const std::vector<int> &perm, const char *fn)
{
    const size_t N = perm.size();
    if (N == 0)
        throw MError(std::string(fn) + ": perm vector must not be empty",
                     0, 0, fn, "", std::string("m:") + fn + ":emptyPerm");
    std::vector<int> sorted = perm;
    std::sort(sorted.begin(), sorted.end());
    for (size_t i = 0; i < N; ++i) {
        if (sorted[i] != static_cast<int>(i + 1))
            throw MError(std::string(fn) + ": perm must be a permutation of 1..N",
                         0, 0, fn, "", std::string("m:") + fn + ":badPerm");
    }
    return N;
}

// Pad perm to 3 dims with identity for any missing axis. So perm=[2 1]
// applied to a 2D matrix becomes [2 1 3]. This lets the 3D code path
// handle both cases uniformly.
std::vector<int> padPerm(const std::vector<int> &perm)
{
    std::vector<int> p = perm;
    while (p.size() < 3) p.push_back(static_cast<int>(p.size() + 1));
    return p;
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

// True if the perm is a transpose-on-each-page: [2, 1] or [2, 1, 3].
inline bool isTransposePerm(const std::vector<int> &p3)
{
    return p3.size() >= 3 && p3[0] == 2 && p3[1] == 1 && p3[2] == 3;
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
MValue permute(Allocator &alloc, const MValue &x, const std::vector<int> &perm)
{
    if (perm.size() > 3)
        throw MError("permute: numkit-m supports up to 3 dimensions",
                     0, 0, "permute", "", "m:permute:tooManyDims");
    validatePerm(perm, "permute");

    const auto &dd = x.dims();
    const size_t inDims[3] = { dd.rows(), dd.cols(),
                               dd.is3D() ? dd.pages() : 1 };
    const auto p = padPerm(perm);
    const size_t outR = inDims[p[0] - 1];
    const size_t outC = inDims[p[1] - 1];
    const size_t outP = inDims[p[2] - 1];
    const bool out3D = (outP > 1) || dd.is3D();

    auto r = out3D ? MValue::matrix3d(outR, outC, outP, MType::DOUBLE, &alloc)
                   : MValue::matrix(outR, outC, MType::DOUBLE, &alloc);
    if (x.numel() == 0) return r;

    const size_t inR = inDims[0], inC = inDims[1];
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();

    // Phase P6 fast path: per-page transpose ([2, 1] or [2, 1, 3]). 2.3×
    // win on 512×512 (cache-blocked vs strided gather).
    if (isTransposePerm(p)) {
        const size_t P = inDims[2];
        for (size_t pp = 0; pp < P; ++pp)
            transposePage(src + pp * inR * inC,
                          dst + pp * outR * outC,
                          inR, inC);
        return r;
    }

    // General N-D permute — strided gather. Inner loop cannot be
    // SIMD-vectorised (input strides differ per axis after the perm).
    const size_t inStride[3] = { 1, inR, inR * inC };
    for (size_t i2 = 0; i2 < outP; ++i2) {
        for (size_t i1 = 0; i1 < outC; ++i1) {
            for (size_t i0 = 0; i0 < outR; ++i0) {
                const size_t outIdx[3] = { i0, i1, i2 };
                size_t inIdx[3] = { 0, 0, 0 };
                inIdx[p[0] - 1] = outIdx[0];
                inIdx[p[1] - 1] = outIdx[1];
                inIdx[p[2] - 1] = outIdx[2];
                const size_t srcOff = inIdx[0] * inStride[0]
                                    + inIdx[1] * inStride[1]
                                    + inIdx[2] * inStride[2];
                const size_t dstOff = i2 * outR * outC + i1 * outR + i0;
                dst[dstOff] = src[srcOff];
            }
        }
    }
    return r;
}

MValue ipermute(Allocator &alloc, const MValue &x, const std::vector<int> &perm)
{
    if (perm.size() > 3)
        throw MError("ipermute: numkit-m supports up to 3 dimensions",
                     0, 0, "ipermute", "", "m:ipermute:tooManyDims");
    validatePerm(perm, "ipermute");
    // Compute inverse permutation: invPerm[perm[i] - 1] = i + 1.
    std::vector<int> inv(perm.size());
    for (size_t i = 0; i < perm.size(); ++i)
        inv[perm[i] - 1] = static_cast<int>(i + 1);
    return permute(alloc, x, inv);
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
MValue squeeze(Allocator &alloc, const MValue &x)
{
    const auto &dd = x.dims();
    if (!dd.is3D()) {
        // 2D / vector / scalar: shape preserved; copy buffer.
        return reshape(alloc, x, dd.rows(), dd.cols(), 0);
    }
    const size_t R = dd.rows(), C = dd.cols(), P = dd.pages();

    // Build the kept-dims list (skip singletons but keep at least 2).
    std::vector<size_t> kept;
    if (R != 1) kept.push_back(R);
    if (C != 1) kept.push_back(C);
    if (P != 1) kept.push_back(P);
    while (kept.size() < 2) kept.push_back(1);  // preserve 2D minimum

    if (kept.size() == 2) {
        return reshape(alloc, x, kept[0], kept[1], 0);
    }
    // All three dims > 1 → no-op, return shape-preserving copy.
    return reshape(alloc, x, R, C, P);
}

// ────────────────────────────────────────────────────────────────────
// cat(dim, ...)
// ────────────────────────────────────────────────────────────────────
//
// dim=1 → vertcat (delegate); dim=2 → horzcat (delegate); dim=3 →
// stack 2D pages or extend 3D page count. Other dims throw.
namespace {

MValue catDim3(Allocator &alloc, const MValue *values, size_t count)
{
    if (count == 0) return MValue::empty();

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
                throw MError("cat: dim 3 inputs must agree on rows and cols",
                             0, 0, "cat", "", "m:cat:badDims");
        }
        totalPages += dd.is3D() ? dd.pages() : 1;
    }
    if (!anchored) return MValue::empty();

    auto r = MValue::matrix3d(R, C, totalPages, MType::DOUBLE, &alloc);
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

} // namespace

MValue cat(Allocator &alloc, int dim, const MValue *values, size_t count)
{
    switch (dim) {
        case 1: return vertcat(alloc, values, count);
        case 2: return horzcat(alloc, values, count);
        case 3: return catDim3(alloc, values, count);
        default:
            throw MError("cat: dim must be 1, 2, or 3",
                         0, 0, "cat", "", "m:cat:badDim");
    }
}

// ────────────────────────────────────────────────────────────────────
// blkdiag
// ────────────────────────────────────────────────────────────────────
//
// Block-diagonal matrix: diagonal blocks are the inputs (in order),
// off-diagonal regions are zero. 2D inputs only.
MValue blkdiag(Allocator &alloc, const MValue *values, size_t count)
{
    if (count == 0) return MValue::empty();

    size_t totalRows = 0, totalCols = 0;
    for (size_t i = 0; i < count; ++i) {
        if (values[i].dims().is3D())
            throw MError("blkdiag: 3D inputs are not supported",
                         0, 0, "blkdiag", "", "m:blkdiag:3D");
        totalRows += values[i].dims().rows();
        totalCols += values[i].dims().cols();
    }
    auto r = MValue::matrix(totalRows, totalCols, MType::DOUBLE, &alloc);
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

std::vector<int> permFromMValue(const MValue &v)
{
    std::vector<int> p;
    p.reserve(v.numel());
    for (size_t i = 0; i < v.numel(); ++i)
        p.push_back(static_cast<int>(v.doubleData()[i]));
    return p;
}

} // namespace

void permute_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("permute: requires (A, perm)",
                     0, 0, "permute", "", "m:permute:nargin");
    outs[0] = permute(ctx.engine->allocator(), args[0],
                      permFromMValue(args[1]));
}

void ipermute_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("ipermute: requires (A, perm)",
                     0, 0, "ipermute", "", "m:ipermute:nargin");
    outs[0] = ipermute(ctx.engine->allocator(), args[0],
                       permFromMValue(args[1]));
}

void squeeze_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                 CallContext &ctx)
{
    if (args.empty())
        throw MError("squeeze: requires 1 argument",
                     0, 0, "squeeze", "", "m:squeeze:nargin");
    outs[0] = squeeze(ctx.engine->allocator(), args[0]);
}

void cat_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
             CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("cat: requires (dim, A, ...)",
                     0, 0, "cat", "", "m:cat:nargin");
    const int dim = static_cast<int>(args[0].toScalar());
    // Pass &args[1] as the start of the values array.
    outs[0] = cat(ctx.engine->allocator(), dim,
                  &args[1], args.size() - 1);
}

void blkdiag_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                 CallContext &ctx)
{
    outs[0] = blkdiag(ctx.engine->allocator(), args.data(), args.size());
}

} // namespace detail

} // namespace numkit::m::builtin
