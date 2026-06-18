// ops/include/numkit/ops/compiler.hpp
//
// Small compiler-portability shims for numkit_ops. Highway's HWY_* attribute
// macros are only available under NUMKIT_WITH_SIMD (they live in Highway's
// headers, which the portable build doesn't include or link), so code that is
// compiled in BOTH builds — e.g. the always-on scalar kernels in
// fused_scalar.cpp / binary_scalar.cpp — needs its own.

#pragma once

// Force a function NOT to be inlined into its callers. Used on the shared
// scalar kernels so the optimizer can't merge their auto-vectorizable loop
// back into a Highway dispatcher whose escaping parallel_for lambda would
// defeat MSVC's alias analysis (the capture trap documented in
// bugs/ops/cheap-elementwise-simd-small-n). With IPO/LTCG off (our default) a
// separate TU already prevents the cross-TU merge; this is belt-and-suspenders
// that keeps the guarantee if whole-program optimization is ever enabled.
#if defined(_MSC_VER)
#  define NUMKIT_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#  define NUMKIT_NOINLINE __attribute__((noinline))
#else
#  define NUMKIT_NOINLINE
#endif
