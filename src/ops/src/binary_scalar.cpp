// ops/src/binary_scalar.cpp
//
// Single source of truth for the SCALAR bodies of the cheap binary element-wise
// ops (plus / minus / times / rdivide). Compiled in BOTH the SIMD and portable
// builds: the portable fallbacks (binary_ops_portable.cpp) forward here, and the
// Highway dispatchers' small-N gate (binary_ops_highway.cpp) calls these
// directly instead of re-inlining the loop.
//
// Same rationale as fused_scalar.cpp: one body shared by the gate and the
// fallback (no drift), living in a lambda-free TU so it auto-vectorizes
// regardless of the dispatcher's parallel_for capture — see
// bugs/ops/cheap-elementwise-simd-small-n. Each op is one IEEE operation, so the
// scalar body is bit-identical to the Highway lane op.

#include <numkit/ops/binary_ops.hpp>
#include <numkit/ops/compiler.hpp>

#include <cstddef>

namespace numkit::ops::detail {

NUMKIT_NOINLINE void plusScalar(const double *a, const double *b, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = a[i] + b[i];
}

NUMKIT_NOINLINE void minusScalar(const double *a, const double *b, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = a[i] - b[i];
}

NUMKIT_NOINLINE void timesScalar(const double *a, const double *b, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = a[i] * b[i];
}

NUMKIT_NOINLINE void rdivideScalar(const double *a, const double *b, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = a[i] / b[i];
}

} // namespace numkit::ops::detail
