// toolboxes/comm/src/channel/fading.cpp
//
// Frequency-flat Rayleigh / Rician fading channels. iid per-sample —
// no Doppler correlation, no taps; the simplest "flat fade" model
// for first-cut digital-comm sims composing with the cycle-17–20
// modulation primitives.

#include <nk/comm/channel/fading.hpp>

#include <nk/builtin/datafun.hpp>
#include <nk/ops/rng_context.hpp>

#include <nk/value/value.hpp>
#include <nk/value/error.hpp>

#include <cmath>
#include <complex>
#include <mutex>
#include <random>
#include <vector>

namespace nk::comm {

namespace {

using Cd = std::complex<double>;

// Generate N iid samples of complex Gaussian CN(0, 1), i.e.
//   real, imag ~ N(0, 1/2)  →  E[|h|²] = 1.
std::vector<Cd> sampleCN01(::nk::ops::RngContext &rng, size_t N) {
    std::vector<Cd> h(N);
    if (N == 0) return h;
    auto &gen = rng;
    std::normal_distribution<double> nd(0.0, std::sqrt(0.5));
    for (size_t i = 0; i < N; ++i) {
        const double re = nd(gen);
        const double im = nd(gen);
        h[i] = Cd(re, im);
    }
    return h;
}

// Pull element i of `x` as a complex value (x may be real or complex).
Cd asComplex(const Value &x, size_t i) {
    if (x.type() == ValueType::COMPLEX) return x.complexData()[i];
    return Cd(x.elemAsDouble(i), 0.0);
}

} // anonymous

Value rayleighchan(::nk::ops::RngContext &rng, const Value &x, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    Value y = Value::matrix(x.dims().rows(), x.dims().cols(),
                            ValueType::COMPLEX, mr);
    if (N == 0) return y;
    auto h = sampleCN01(rng, N);
    Cd *yd = y.complexDataMut();
    for (size_t i = 0; i < N; ++i) yd[i] = h[i] * asComplex(x, i);
    return y;
}

Value ricianchan(::nk::ops::RngContext &rng, const Value &x, double K, std::pmr::memory_resource *mr)
{
    if (K < 0.0)
        throw Error("ricianchan: K-factor must be ≥ 0",
                    0, 0, "ricianchan", "", "nk:ricianchan:K");
    const size_t N = x.numel();
    Value y = Value::matrix(x.dims().rows(), x.dims().cols(),
                            ValueType::COMPLEX, mr);
    if (N == 0) return y;
    auto g = sampleCN01(rng, N);
    const double losAmp = std::sqrt(K / (K + 1.0));
    const double sctAmp = std::sqrt(1.0 / (K + 1.0));
    Cd *yd = y.complexDataMut();
    for (size_t i = 0; i < N; ++i) {
        const Cd h = Cd(losAmp, 0.0) + sctAmp * g[i];
        yd[i] = h * asComplex(x, i);
    }
    return y;
}

} // namespace nk::comm
