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

// out[i] = min(hi, max(lo, scale*x[i] + offset))   — the min-outer spelling of
// the same clamp. Identical to fusedAffineClamp for finite inputs, but differs
// on NaN (min-outer saturates a NaN to lo, max-outer to hi — matching the
// respective per-op min/max nesting), so it is a separate kernel, not a flag.
void fusedAffineClampMinOuter(const double *x, double scale, double offset,
                              double lo, double hi, double *out, std::size_t n);

// out[i] = scale*x[i] + offset   — plain affine, NO clamp, NaN-preserving.
// Covers two-op scale-and-shift chains: a.*x+b, x.*a-b, b+a.*x (rescale,
// negate, unit conversion). A separate kernel from fusedAffineClamp because a
// ±inf "clamp" can NOT stand in for "no clamp": fmin(+inf, NaN) = +inf would
// erase a NaN, whereas scale*NaN+offset = NaN flows through here untouched.
void fusedAffine(const double *x, double scale, double offset,
                 double *out, std::size_t n);

// out[i] = a*x[i] + b*y[i]   — two-array linear combination (x, y same size).
// Covers blend/lerp (b=1-a), weighted sum, scaled difference (b<0), a.*x+b.*y.
// Mul, Mul, Add (no FMA) so it matches the per-op `a.*x + b.*y` bit-for-bit.
void fusedAxpby(const double *x, double a, const double *y, double b,
                double *out, std::size_t n);

// out[i] = (x[i] - sub) * mul   — center then scale (rescale, normalize,
// z-score by a precomputed reciprocal). Sub then Mul (two roundings) so it is
// bit-identical to the per-op `(x - c).*s`. NaN/Inf propagate naturally.
void fusedShiftScaleMul(const double *x, double sub, double mul,
                        double *out, std::size_t n);

// out[i] = (x[i] - sub) / div   — center then divide ((x-mu)./sigma,
// (x-lo)./range). Sub then Div (two roundings) so it is bit-identical to the
// per-op `(x - c)./d` (which can't be folded into a *mul: 1/d would round).
void fusedShiftScaleDiv(const double *x, double sub, double div,
                        double *out, std::size_t n);

// out[i] = |scale*x[i] + offset|   — magnitude of an affine. Mul, Add, then
// abs (abs is exact — a sign-bit clear), so bit-identical to `abs(a.*x ± b)`.
// Also serves `abs(x - c)` (scale 1) since |1*x + (-c)| == |x - c|.
void fusedAbsAffine(const double *x, double scale, double offset,
                    double *out, std::size_t n);

// out[i] = |x[i] - y[i]|   — absolute difference of two arrays (L1 residual /
// error). Sub then abs (exact) → bit-identical to `abs(x - y)`.
void fusedAbsDiff(const double *x, const double *y, double *out, std::size_t n);

// Unary function applied to an affine: out[i] = f(scale*x[i] + offset).
// Restricted to the functions whose SIMD form is bit-identical to the scalar
// libm one — sqrt is correctly-rounded; floor/ceil are exact — so the result
// matches the per-op `f(a.*x ± b)` regardless of how the array is chunked.
// (exp/log/sin… are NOT here: Highway's polynomial differs from libm by a few
// ULP, so they'd only match if the per-op tail-vs-SIMD split were reproduced.)
// Sqrt's negative-input domain (where MATLAB promotes to complex) is handled by
// the caller, which declines before invoking this kernel.
enum class UnaryAffineFn { Sqrt, Floor, Ceil };
void fusedUnaryAffine(const double *x, double scale, double offset,
                      UnaryAffineFn fn, double *out, std::size_t n);

} // namespace numkit::ops
