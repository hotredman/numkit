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

} // namespace numkit::ops
