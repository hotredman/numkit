// core/include/numkit/core/shape_ops.hpp
//
// Pure-function helpers for ND tensor shape ops: broadcasting, strides,
// coord-walking. Used by builtin and DSP ops that need to generalise
// past 2D/3D. Header-only inline so the optimiser can fold the ndim
// branches in 2D/3D fast paths.
//
// All routines use column-major strides (rows = innermost), matching
// MATLAB and the existing Value layout. Trailing-singleton normalisation
// is implicit: a missing trailing dim is treated as 1.

#pragma once

#include <numkit/core/dims.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace numkit {

// ============================================================
// broadcastDimsND
// ------------------------------------------------------------
// NumPy-style implicit expansion. Aligns dims right (i.e. trailing-
// singleton-extend the shorter shape), then per-axis: dims must match
// or one must be 1. Output ndim = max(a.ndim, b.ndim).
//
// Returns true on success and writes the result Dims to out. Returns
// false if any axis is incompatible (output left untouched).
// ============================================================
inline bool broadcastDimsND(const Dims &a, const Dims &b, Dims &out)
{
    const int na = a.ndim();
    const int nb = b.ndim();
    const int nOut = std::max(na, nb);

    // Build the output dim vector on the stack (or fall back to small
    // heap if you really need >32D, which isn't a thing).
    constexpr int kMaxStack = 32;
    size_t buf[kMaxStack];
    if (nOut > kMaxStack) return false;  // refuse pathological ranks

    for (int i = 0; i < nOut; ++i) {
        const size_t da = (i < na) ? a.dim(i) : 1;
        const size_t db = (i < nb) ? b.dim(i) : 1;
        if (da != db && da != 1 && db != 1) return false;
        buf[i] = std::max(da, db);
    }
    out = Dims(buf, nOut);
    return true;
}

// ============================================================
// broadcastOffsetND
// ------------------------------------------------------------
// Given output coords and an operand's shape, compute the operand's
// linear (column-major) offset under broadcasting. Operand axes that
// are 1 collapse their coord to 0 (broadcasting); operand axes missing
// (because operand has lower ndim than output) collapse to 0 too.
//
// Hot inner-loop function; kept inline. coords[] length must be ≥ outNdim.
// ============================================================
inline size_t broadcastOffsetND(const size_t *coords, int outNdim,
                                const Dims &operand)
{
    const int nOp = operand.ndim();
    size_t off    = 0;
    size_t stride = 1;
    for (int i = 0; i < outNdim; ++i) {
        const size_t opDim = (i < nOp) ? operand.dim(i) : 1;
        const size_t c     = (opDim == 1) ? 0 : coords[i];
        off += c * stride;
        stride *= opDim;
    }
    return off;
}

// ============================================================
// computeStridesColMajor
// ------------------------------------------------------------
// strides[0] = 1; strides[i] = strides[i-1] * d.dim(i-1).
// Caller provides strides[] of length ≥ d.ndim().
// ============================================================
inline void computeStridesColMajor(const Dims &d, size_t *strides)
{
    const int n = d.ndim();
    if (n == 0) return;
    strides[0] = 1;
    for (int i = 1; i < n; ++i)
        strides[i] = strides[i - 1] * d.dim(i - 1);
}

// ============================================================
// linearizeFromCoords
// ------------------------------------------------------------
// Linear (column-major) offset from coords[] given precomputed strides.
// Used after computeStridesColMajor; saves the per-element multiply chain
// in tight loops by amortising it across coordinates.
// ============================================================
inline size_t linearizeFromCoords(const size_t *coords, const size_t *strides,
                                  int nd)
{
    size_t off = 0;
    for (int i = 0; i < nd; ++i)
        off += coords[i] * strides[i];
    return off;
}

// ============================================================
// incrementCoords
// ------------------------------------------------------------
// In-place ++coords with carry from inner (axis 0) to outer (axis nd-1).
// Returns false when the iteration is complete (coords have wrapped past
// the highest axis). Used by generic ND walkers in ops that don't have
// a 2D/3D fast path.
//
// Pattern:
//   size_t coords[N] = {0};
//   do {
//       // ... use coords ...
//   } while (incrementCoords(coords, dims));
// ============================================================
inline bool incrementCoords(size_t *coords, const Dims &d)
{
    const int n = d.ndim();
    for (int i = 0; i < n; ++i) {
        if (++coords[i] < d.dim(i)) return true;
        coords[i] = 0;
    }
    return false;
}

// ============================================================
// forEachNDPair
// ------------------------------------------------------------
// Walk every output coordinate of a binary elementwise op under NumPy
// broadcasting and call fn(outIdx, aOff, bOff) with the column-major
// linear offsets into operand A and B.
//
// Same-shape fast path: aDims == bDims == outDims collapses to a single
// linear loop where all three indices coincide. General path walks
// coords with carry and uses broadcastOffsetND per operand.
//
// Precondition: outDims was produced by broadcastDimsND (so its ndim
// is already ≤ kMaxRank). Fn signature: (size_t outIdx, size_t aOff,
// size_t bOff).
// ============================================================
template <typename Fn>
inline void forEachNDPair(const Dims &aDims, const Dims &bDims,
                          const Dims &outDims, Fn &&fn)
{
    if (aDims == bDims && aDims == outDims) {
        const size_t n = outDims.numel();
        for (size_t i = 0; i < n; ++i) fn(i, i, i);
        return;
    }
    const int nd = outDims.ndim();
    size_t coords[Dims::kMaxRank] = {0};
    size_t outIdx = 0;
    do {
        const size_t aOff = broadcastOffsetND(coords, nd, aDims);
        const size_t bOff = broadcastOffsetND(coords, nd, bDims);
        fn(outIdx++, aOff, bOff);
    } while (incrementCoords(coords, outDims));
}

// ============================================================
// forEachOuterPage
// ------------------------------------------------------------
// Walk every (R×C) "page" of d, indexed by axes 2..ndim-1 in column-
// major order (axis 2 fastest, then axis 3, ...). For each page index
// `plin ∈ [0, outerCount)`, calls fn(plin, outerCoords) where
// outerCoords[i] is the 0-based position along axis i+2.
//
// Used by ND format / disp (header per page) and by trilTriuND /
// rot90 ND (per-page byte kernel). For ndim ≤ 2 there's exactly one
// "page" with empty outerCoords.
// ============================================================
template <typename Fn>
inline void forEachOuterPage(const Dims &d, Fn &&fn)
{
    const int nd = d.ndim();
    size_t outerCount = 1;
    for (int i = 2; i < nd; ++i) outerCount *= d.dim(i);
    size_t outerCoords[Dims::kMaxRank] = {0};
    for (size_t plin = 0; plin < outerCount; ++plin) {
        fn(plin, outerCoords);
        // Increment outer coords (axis 2..nd-1) in column-major order.
        for (int i = 2; i < nd; ++i) {
            if (++outerCoords[i - 2] < d.dim(i)) break;
            outerCoords[i - 2] = 0;
        }
    }
}

} // namespace numkit
