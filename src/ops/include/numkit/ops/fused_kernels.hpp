// ops/include/numkit/ops/fused_kernels.hpp
//
// Fused element-wise kernels: each is a hand-written, fixed-structure SIMD
// loop that computes a whole common idiom (an element-wise chain) in ONE pass
// — every array leaf read once, the output written once, intermediates kept in
// registers. This is the form that actually wins: a runtime micro-VM can't keep
// intermediates in registers (variable slot indices spill to the stack), so it
// loses to the per-op SIMD path; a fixed-structure kernel does not.
//
// These are the primitives behind VM element-wise fusion (the compiler/walker
// recognise an idiom and dispatch here, falling back to the per-op path when
// the runtime types don't fit — see the fusion rule registry). Real double
// only; `out` may alias an array input (each element reads its inputs before
// its own store). Large arrays are threaded internally via parallel_for.
//
// One Highway TU per kernel (fused_<name>_highway.cpp) keeps MSVC's per-TU
// inliner budget from leaking between kernels (same reason the FFT kernels are
// split); the scalar fallbacks share fused_kernels_portable.cpp.

#pragma once

#include <cstddef>

namespace numkit::ops {

// out[i] = max(lo, min(hi, scale*x[i] + offset))   — affine then clamp.
// Covers normalize+saturate idioms: max(0,min(1, a.*x+b)), rescale, ReLU(+aff).
void fusedAffineClamp(const double *x, double scale, double offset,
                      double lo, double hi, double *out, std::size_t n);

} // namespace numkit::ops
