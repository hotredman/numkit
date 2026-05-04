// libs/comm/src/channel/fading.cpp
//
// Frequency-flat Rayleigh / Rician fading channels. iid per-sample —
// no Doppler correlation, no taps; the simplest "flat fade" model
// for first-cut digital-comm sims composing with the cycle-17–20
// modulation primitives.

#include <numkit/comm/channel/fading.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cmath>
#include <complex>
#include <mutex>
#include <random>
#include <vector>

namespace numkit::comm {

namespace {

using Cd = std::complex<double>;

// Generate N iid samples of complex Gaussian CN(0, 1), i.e.
//   real, imag ~ N(0, 1/2)  →  E[|h|²] = 1.
std::vector<Cd> sampleCN01(size_t N) {
    std::vector<Cd> h(N);
    if (N == 0) return h;
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::normal_distribution<double> nd(0.0, std::sqrt(0.5));
    std::lock_guard<std::mutex> lk(mtx);
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

Value rayleighchan(std::pmr::memory_resource *mr, const Value &x)
{
    const size_t N = x.numel();
    Value y = Value::matrix(x.dims().rows(), x.dims().cols(),
                            ValueType::COMPLEX, mr);
    if (N == 0) return y;
    auto h = sampleCN01(N);
    Cd *yd = y.complexDataMut();
    for (size_t i = 0; i < N; ++i) yd[i] = h[i] * asComplex(x, i);
    return y;
}

Value ricianchan(std::pmr::memory_resource *mr, const Value &x, double K)
{
    if (K < 0.0)
        throw Error("ricianchan: K-factor must be ≥ 0",
                    0, 0, "ricianchan", "", "m:ricianchan:K");
    const size_t N = x.numel();
    Value y = Value::matrix(x.dims().rows(), x.dims().cols(),
                            ValueType::COMPLEX, mr);
    if (N == 0) return y;
    auto g = sampleCN01(N);
    const double losAmp = std::sqrt(K / (K + 1.0));
    const double sctAmp = std::sqrt(1.0 / (K + 1.0));
    Cd *yd = y.complexDataMut();
    for (size_t i = 0; i < N; ++i) {
        const Cd h = Cd(losAmp, 0.0) + sctAmp * g[i];
        yd[i] = h * asComplex(x, i);
    }
    return y;
}

namespace detail {

void rayleighchan_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rayleighchan: requires (x)",
                    0, 0, "rayleighchan", "", "m:rayleighchan:nargin");
    outs[0] = rayleighchan(ctx.engine->resource(), args[0]);
}

void ricianchan_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ricianchan: requires (x, K)",
                    0, 0, "ricianchan", "", "m:ricianchan:nargin");
    outs[0] = ricianchan(ctx.engine->resource(), args[0],
                         args[1].toScalar());
}

} // namespace detail

} // namespace numkit::comm
