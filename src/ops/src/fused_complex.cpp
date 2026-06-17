// ops/src/fused_complex.cpp
//
// Complex-input fused kernels. Unlike the real kernels these are plain scalar
// std::complex<double> loops — Highway has no complex transcendentals, and
// numkit's own per-op complex path is scalar std::complex too, so there is no
// SIMD form to mirror. Each kernel reproduces numkit's per-op composition with
// the SAME std::complex operators / std:: functions on the SAME compiler, so the
// fused result is bit-identical to the unfused one by construction (the parity
// harness checks it via isequaln). The win here is temporary elimination (fewer
// passes over memory), not SIMD. Complex arithmetic / transcendentals are total
// (no real-domain promotion), so no kernel needs a domain guard.
//
// One file (not split per kernel like the Highway TUs): scalar loops carry no
// per-TU inliner-budget cost. Lives in the always-on ops sources so it is in
// both the SIMD and portable builds.

#include <numkit/ops/fused_kernels.hpp>

#include <cmath>
#include <complex>
#include <cstddef>

namespace numkit::ops {

// The inner of a complex f(inner): affine (genuine complex multiply, for the
// product-coefficient kinds) or a pure shift/neg (scale is exactly ±1 → add or
// negate, NOT a complex mul by (±1+0i), which would make 0*Inf=NaN and diverge
// from the per-op bare add/sub on a non-finite z).
static inline Cx cxInner(const Cx &z, const Cx &scale, const Cx &offset,
                         bool affine) {
    if (affine) return scale * z + offset;
    return (scale.real() > 0.0 ? z : -z) + offset;
}

void fusedAffineCx(const Cx *x, Cx scale, Cx offset, Cx *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = scale * x[i] + offset;
}

void fusedAxpbyCx(const Cx *x, Cx a, const Cx *y, Cx b, Cx *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = a * x[i] + b * y[i];
}

void fusedShiftScaleMulCx(const Cx *x, Cx sub, Cx mul, Cx *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = (x[i] - sub) * mul;
}

void fusedShiftScaleDivCx(const Cx *x, Cx sub, Cx div, Cx *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = (x[i] - sub) / div;
}

// z.^2 on a COMPLEX base is std::pow(z, 2) in numkit (the .^2 == z.*z fast-path
// is real-only — for complex, elementPower takes the std::pow branch), so the
// complex square mirrors that, NOT v*v (std::pow(z,2) != z*z for complex).
void fusedSqAffineCx(const Cx *x, Cx scale, Cx offset, bool affine,
                     Cx *out, std::size_t n) {
    const Cx two(2.0, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        out[i] = std::pow(cxInner(x[i], scale, offset, affine), two);
}

void fusedSqDiffCx(const Cx *x, const Cx *y, Cx *out, std::size_t n) {
    const Cx two(2.0, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        out[i] = std::pow(x[i] - y[i], two);
}

void fusedSqrtAffineCx(const Cx *x, Cx scale, Cx offset, bool affine,
                       Cx *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        out[i] = std::sqrt(cxInner(x[i], scale, offset, affine));
}

void fusedSqrtShiftDivCx(const Cx *x, Cx sub, Cx div, Cx *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = std::sqrt((x[i] - sub) / div);
}

// Per-element complex transcendental, mirroring numkit's complex op for each fn
// (the complexOp lambdas in trig/exp_log): mostly std::F(z); log2 = log(z)/log2,
// log1p = log(1+z). Expm1 is real-only in numkit (declined upstream) so it never
// reaches here — mapped to identity defensively.
static inline Cx transCxApply(TransAffineFn fn, Cx v) {
    switch (fn) {
        case TransAffineFn::Exp:   return std::exp(v);
        case TransAffineFn::Log:   return std::log(v);
        case TransAffineFn::Log2:  return std::log(v) / std::log(2.0);
        case TransAffineFn::Log10: return std::log10(v);
        case TransAffineFn::Log1p: return std::log(Cx(1.0, 0.0) + v);
        case TransAffineFn::Sin:   return std::sin(v);
        case TransAffineFn::Cos:   return std::cos(v);
        case TransAffineFn::Tan:   return std::tan(v);
        case TransAffineFn::Tanh:  return std::tanh(v);
        case TransAffineFn::Sinh:  return std::sinh(v);
        case TransAffineFn::Cosh:  return std::cosh(v);
        case TransAffineFn::Atan:  return std::atan(v);
        case TransAffineFn::Asinh: return std::asinh(v);
        case TransAffineFn::Asin:  return std::asin(v);
        case TransAffineFn::Acos:  return std::acos(v);
        case TransAffineFn::Acosh: return std::acosh(v);
        case TransAffineFn::Atanh: return std::atanh(v);
        case TransAffineFn::Expm1: return v;  // unreachable (declined upstream)
    }
    return v;
}

void fusedTransAffineCx(const Cx *x, Cx scale, Cx offset, bool affine,
                        TransAffineFn fn, Cx *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        out[i] = transCxApply(fn, cxInner(x[i], scale, offset, affine));
}

void fusedTransShiftDivCx(const Cx *x, Cx sub, Cx div, TransAffineFn fn,
                          Cx *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        out[i] = transCxApply(fn, (x[i] - sub) / div);
}

// abs of complex → the REAL magnitude std::abs(z) (mirrors numkit's abs(complex),
// which is per-element std::abs). out is double.
void fusedAbsAffineCx(const Cx *x, Cx scale, Cx offset, bool affine,
                      double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        out[i] = std::abs(cxInner(x[i], scale, offset, affine));
}

void fusedAbsShiftDivCx(const Cx *x, Cx sub, Cx div, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = std::abs((x[i] - sub) / div);
}

void fusedAbsDiffCx(const Cx *x, const Cx *y, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = std::abs(x[i] - y[i]);
}

// complex soft-threshold: sign(z) .* max(0, |z| - t). sign(z) = z/|z| (0 at
// z==0, MATLAB R2025b); the .* by the real max promotes it to Complex(mx,0)
// (full complex mul), exactly as numkit's per-op sign(z).*max(...).
void fusedSoftThresholdCx(const Cx *x, double t, Cx *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        const Cx z = x[i];
        const double m = std::abs(z);
        const Cx s = (m == 0.0) ? Cx(0.0, 0.0) : z / m;   // sign(z)
        out[i] = s * Cx(std::fmax(0.0, m - t), 0.0);
    }
}

} // namespace numkit::ops
