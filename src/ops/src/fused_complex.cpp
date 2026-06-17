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
void fusedSqAffineCx(const Cx *x, Cx scale, Cx offset, Cx *out, std::size_t n) {
    const Cx two(2.0, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        out[i] = std::pow(scale * x[i] + offset, two);
}

void fusedSqDiffCx(const Cx *x, const Cx *y, Cx *out, std::size_t n) {
    const Cx two(2.0, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        out[i] = std::pow(x[i] - y[i], two);
}

void fusedSqrtAffineCx(const Cx *x, Cx scale, Cx offset, Cx *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = std::sqrt(scale * x[i] + offset);
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

void fusedTransAffineCx(const Cx *x, Cx scale, Cx offset, TransAffineFn fn,
                        Cx *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        out[i] = transCxApply(fn, scale * x[i] + offset);
}

void fusedTransShiftDivCx(const Cx *x, Cx sub, Cx div, TransAffineFn fn,
                          Cx *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        out[i] = transCxApply(fn, (x[i] - sub) / div);
}

} // namespace numkit::ops
