// core/include/numkit/m/core/MShapeOps.hpp
//
// Pure-function helpers for ND tensor shape ops: broadcasting, strides,
// coord-walking. Used by builtin and DSP ops that need to generalise
// past 2D/3D. Header-only inline so the optimiser can fold the ndim
// branches in 2D/3D fast paths.
//
// All routines use column-major strides (rows = innermost), matching
// MATLAB and the existing MValue layout. Trailing-singleton normalisation
// is implicit: a missing trailing dim is treated as 1.

#pragma once

#include <numkit/m/core/MDims.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace numkit::m {

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

} // namespace numkit::m
