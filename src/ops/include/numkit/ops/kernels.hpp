// numkit/ops/kernels.hpp
//
// Public raw-buffer compute kernels — the STABLE facade over the backend-split
// inner loops (which live in numkit::ops::detail and are selected at build time
// by NUMKIT_WITH_SIMD). Two audiences:
//
//   * the codegen subsystem — generated C++ calls these directly as its
//     lowering target for heavy array ops (column-major double buffers, the
//     same RawBuffer ABI the emitter already uses), so a transpiled kernel
//     reuses ops' SIMD implementation instead of emitting a naive loop; and
//   * any in-tree caller that already holds raw buffers and wants the fast path
//     without going through Value.
//
// Callers use ONLY this header — never numkit::ops::detail. This is where the
// complex / elementwise / transcendental / fft kernel entries will also land
// as the codegen lowering surface grows.
#pragma once

#include <cstddef>

// Linkage. Default (static — compiled into numkit_ops, a test, or a
// self-contained artifact): plain. Building the nk_ops_kernels shared lib:
// dllexport. A consumer linking that shared lib (a codegen artifact that uses
// ops kernels): define NK_OPS_USE_DLL -> dllimport. No effect off Windows.
// Mirrors NK_RT_API (nk_codegen_rt.h).
#if defined(_WIN32) && defined(NK_OPS_BUILDING_DLL)
#  define NK_OPS_API __declspec(dllexport)
#elif defined(_WIN32) && defined(NK_OPS_USE_DLL)
#  define NK_OPS_API __declspec(dllimport)
#else
#  define NK_OPS_API
#endif

namespace numkit::ops {

// C(M×N) = A(M×K) · B(K×N), all column-major (a[k*M+i], b[j*K+k], c[j*M+i]).
// C is caller-allocated; the kernel zeroes then accumulates. SIMD MulAdd down
// columns of A under NUMKIT_WITH_SIMD, a portable loop otherwise.
NK_OPS_API void matmulDouble(const double *a, const double *b, double *c,
                             std::size_t M, std::size_t N, std::size_t K);

} // namespace numkit::ops
