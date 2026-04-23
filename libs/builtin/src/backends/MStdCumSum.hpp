// libs/builtin/src/backends/MStdCumSum.hpp
//
// Internal contract for SIMD prefix-sum (cumsum) on contiguous double
// vectors. SIMD-dispatched (Highway) when NUMKIT_WITH_SIMD=ON, scalar
// otherwise. The data dependency `s += x[i]; r[i] = s;` makes the
// straight scalar loop fundamentally serial — Hillis-Steele parallel
// scan within each SIMD vector breaks the chain into log2(lanes) steps,
// then propagates a carry across vectors.

#pragma once

#include <cstddef>

namespace numkit::m::builtin {

// Inclusive prefix sum over [src, src+n) into [dst, dst+n). Buffers may
// alias (caller may pass src == dst for in-place; we don't, but we
// permit it). Returns nothing — the result is dst[i] = sum(src[0..i]).
void cumsumScan(const double *src, double *dst, std::size_t n);

} // namespace numkit::m::builtin
