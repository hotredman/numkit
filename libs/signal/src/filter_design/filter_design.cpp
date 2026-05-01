// libs/signal/src/filter_design/filter_design.cpp
//
// Butterworth IIR design (butter) + windowed-sinc FIR design (fir1).
// freqz / phasez / grpdelay (frequency-domain analysis of an existing
// filter) live in filter_analysis/frequency_response.cpp.

#include <numkit/signal/filter_design/filter_design.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "../dsp_helpers.hpp"           // Complex typedef
#include "poly_helpers.hpp"             // polyExpandFromRoots

#define _USE_MATH_DEFINES
#include <cmath>
#include <complex>
#include <memory_resource>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

ScratchVec<Complex> butterworthPoles(std::pmr::memory_resource *mr, int N)
{
    ScratchVec<Complex> poles(mr);
    poles.reserve(N);
    for (int k = 0; k < N; ++k) {
        const double theta = M_PI * (2.0 * k + N + 1) / (2.0 * N);
        poles.emplace_back(std::cos(theta), std::sin(theta));
    }
    return poles;
}

using numkit::builtin::detail::polyExpandFromRoots;

// Bilinear-transform helper: take an arbitrary set of analog poles and
// zeros (already pre-warped + frequency-transformed) and produce the
// real-coefficient digital (b, a). Caller is responsible for the analog
// → analog transformations (LP scale, LP→HP); this function just maps
// each s-plane root through z = (2+s)/(2-s).
void bilinearTransformPZ(std::pmr::memory_resource *mr,
                         const Complex *sPoles, std::size_t pN,
                         const Complex *sZeros, std::size_t zN,
                         ScratchVec<double> &bOut,
                         ScratchVec<double> &aOut)
{
    ScratchVec<Complex> zPoles(pN, mr);
    for (std::size_t i = 0; i < pN; ++i) {
        const Complex sp = sPoles[i];
        zPoles[i] = (2.0 + sp) / (2.0 - sp);
    }
    aOut = polyExpandFromRoots(mr, zPoles.data(), zPoles.size());

    // Map each finite s-plane zero. Any "zero at infinity" in the analog
    // domain (count = pN - zN, classic for an N-pole all-pole prototype
    // like Butterworth LP) maps to z = -1 by the bilinear transform.
    const std::size_t totalZ = pN;
    ScratchVec<Complex> zZeros(totalZ, mr);
    for (std::size_t i = 0; i < zN; ++i) {
        const Complex sz = sZeros[i];
        zZeros[i] = (2.0 + sz) / (2.0 - sz);
    }
    for (std::size_t i = zN; i < totalZ; ++i)
        zZeros[i] = Complex(-1.0, 0.0);

    bOut = polyExpandFromRoots(mr, zZeros.data(), zZeros.size());
}

// Normalise b so that |H(z0)| == 1 at the reference frequency z0 (== 1
// for LP, == -1 for HP).
void normaliseAtRef(ScratchVec<double> &b, const ScratchVec<double> &a,
                    Complex z0)
{
    Complex num(0, 0), den(0, 0);
    Complex zk(1, 0);
    for (std::size_t i = 0; i < std::max(b.size(), a.size()); ++i) {
        if (i < b.size()) num += b[i] * zk;
        if (i < a.size()) den += a[i] * zk;
        zk *= z0;
    }
    const double mag = std::abs(num / den);
    if (mag > 0.0)
        for (double &v : b) v /= mag;
}

} // anonymous namespace

std::tuple<Value, Value>
butter(std::pmr::memory_resource *mr, int N, double Wn, const std::string &type)
{
    if (Wn <= 0.0 || Wn >= 1.0)
        throw Error("butter: Wn must be between 0 and 1",
                     0, 0, "butter", "", "m:butter:badWn");
    if (type != "low" && type != "high")
        throw Error("butter: type must be 'low' or 'high'",
                     0, 0, "butter", "", "m:butter:badType");

    // Pre-warp the digital cutoff to the analog domain.
    const double Wa = 2.0 * std::tan(M_PI * Wn / 2.0);

    ScratchArena scratch(mr);
    auto sPoles = butterworthPoles(&scratch, N);   // unit-cutoff prototype

    // Apply the LP scale or LP→HP transform IN THE ANALOG DOMAIN before
    // the bilinear map. For LP: s_k = sp_k * Wa, no finite zeros. For
    // HP: s_k = Wa / sp_k, plus N zeros at s = 0 (which map to z = 1
    // through the bilinear).
    ScratchVec<Complex> sP(static_cast<std::size_t>(N), &scratch);
    ScratchVec<Complex> sZ(&scratch);
    if (type == "low") {
        for (int i = 0; i < N; ++i) sP[i] = sPoles[i] * Wa;
        // sZ stays empty — Butterworth LP has all zeros at infinity.
    } else {
        for (int i = 0; i < N; ++i) sP[i] = Wa / sPoles[i];
        sZ.assign(static_cast<std::size_t>(N), Complex(0.0, 0.0));
    }

    ScratchVec<double> b(&scratch), a(&scratch);
    bilinearTransformPZ(&scratch, sP.data(), sP.size(),
                        sZ.data(), sZ.size(), b, a);

    // Normalise the gain at the reference frequency: DC (z=1) for LP,
    // Nyquist (z=-1) for HP.
    normaliseAtRef(b, a, type == "low" ? Complex(1.0, 0.0)
                                       : Complex(-1.0, 0.0));

    auto bv = Value::matrix(1, b.size(), ValueType::DOUBLE, mr);
    auto av = Value::matrix(1, a.size(), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < b.size(); ++i)
        bv.doubleDataMut()[i] = b[i];
    for (size_t i = 0; i < a.size(); ++i)
        av.doubleDataMut()[i] = a[i];

    return std::make_tuple(std::move(bv), std::move(av));
}

Value fir1(std::pmr::memory_resource *mr, int N, double Wn, const std::string &type)
{
    if (Wn <= 0.0 || Wn >= 1.0)
        throw Error("fir1: Wn must be between 0 and 1",
                     0, 0, "fir1", "", "m:fir1:badWn");
    if (type != "low" && type != "high")
        throw Error("fir1: type must be 'low' or 'high'",
                     0, 0, "fir1", "", "m:fir1:badType");

    const size_t filtLen = N + 1;
    const double wc = M_PI * Wn;
    const double half = N / 2.0;

    ScratchArena scratch(mr);
    auto h = ScratchVec<double>(filtLen, &scratch);
    double hSum = 0.0;

    for (size_t i = 0; i < filtLen; ++i) {
        const double n = i - half;
        const double sinc = (std::abs(n) < 1e-12) ? wc / M_PI
                                                  : std::sin(wc * n) / (M_PI * n);
        const double win = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / N);
        h[i] = sinc * win;
        hSum += h[i];
    }

    if (type == "low") {
        for (size_t i = 0; i < filtLen; ++i)
            h[i] /= hSum;
    } else { // "high"
        for (size_t i = 0; i < filtLen; ++i)
            h[i] /= hSum;
        for (size_t i = 0; i < filtLen; ++i)
            h[i] = -h[i];
        h[static_cast<size_t>(half)] += 1.0;
    }

    auto bv = Value::matrix(1, filtLen, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < filtLen; ++i)
        bv.doubleDataMut()[i] = h[i];
    return bv;
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void butter_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("butter: requires at least 2 arguments",
                     0, 0, "butter", "", "m:butter:nargin");
    const int N = static_cast<int>(args[0].toScalar());
    const double Wn = args[1].toScalar();
    std::string type = "low";
    if (args.size() >= 3 && args[2].isChar())
        type = args[2].toString();

    auto [bv, av] = butter(ctx.engine->resource(), N, Wn, type);
    outs[0] = std::move(bv);
    if (nargout > 1)
        outs[1] = std::move(av);
}

void fir1_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fir1: requires at least 2 arguments",
                     0, 0, "fir1", "", "m:fir1:nargin");
    const int N = static_cast<int>(args[0].toScalar());
    const double Wn = args[1].toScalar();
    std::string type = "low";
    if (args.size() >= 3 && args[2].isChar())
        type = args[2].toString();

    outs[0] = fir1(ctx.engine->resource(), N, Wn, type);
}

} // namespace detail

} // namespace numkit::signal
