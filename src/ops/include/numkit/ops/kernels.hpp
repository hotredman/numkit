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

#include <complex>
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

// Same, for complex matrices (column-major std::complex<double>). Portable
// SAXPY loop (no SIMD-complex split); the single call is amortised over O(M·N·K)
// work, so it carries no per-element overhead vs an inline loop.
NK_OPS_API void matmulComplex(const std::complex<double> *a, const std::complex<double> *b,
                              std::complex<double> *c,
                              std::size_t M, std::size_t N, std::size_t K);

// Element-wise binary ops over two equal-length DOUBLE buffers: out[i] = a[i] OP
// b[i], n elements (flat — rank-agnostic). SIMD under NUMKIT_WITH_SIMD with an
// internal small-N scalar gate (so no dynamic-dispatch crater at tiny n). NOTE:
// a plain __restrict inline loop already auto-vectorises to match these for
// cheap arithmetic (A3) — codegen uses them only as the opt-in ops-kernel tier;
// the inline loop stays the self-contained default.
NK_OPS_API void plusDouble   (const double *a, const double *b, double *out, std::size_t n);
NK_OPS_API void minusDouble  (const double *a, const double *b, double *out, std::size_t n);
NK_OPS_API void timesDouble  (const double *a, const double *b, double *out, std::size_t n);
NK_OPS_API void rdivideDouble(const double *a, const double *b, double *out, std::size_t n);

// Element-wise transcendentals over a DOUBLE buffer: out[i] = fn(x[i]), n
// elements. Forward to the SIMD fusedTransAffine kernel (scale=1, offset=0) —
// the SAME vectorised path the runtime's expression fusion uses, so the result
// matches the interpreter. A plain std::<fn> inline loop does NOT auto-vectorise
// on MSVC, so these are a genuine speedup (unlike the cheap-arithmetic ops). The
// set is the real-total transcendentals (no complex-domain decline): the
// codegen lowers exactly these inline today and routes them here when opt-in.
NK_OPS_API void sinDouble  (const double *x, double *out, std::size_t n);
NK_OPS_API void cosDouble  (const double *x, double *out, std::size_t n);
NK_OPS_API void tanDouble  (const double *x, double *out, std::size_t n);
NK_OPS_API void atanDouble (const double *x, double *out, std::size_t n);
NK_OPS_API void sinhDouble (const double *x, double *out, std::size_t n);
NK_OPS_API void coshDouble (const double *x, double *out, std::size_t n);
NK_OPS_API void tanhDouble (const double *x, double *out, std::size_t n);
NK_OPS_API void expDouble  (const double *x, double *out, std::size_t n);
NK_OPS_API void asinhDouble(const double *x, double *out, std::size_t n);
NK_OPS_API void expm1Double(const double *x, double *out, std::size_t n);

} // namespace numkit::ops
