// libs/signal/src/filter_design/filter_design.cpp
//
// Butterworth IIR design (butter) + windowed-sinc FIR design (fir1).
// freqz / phasez / grpdelay (frequency-domain analysis of an existing
// filter) live in filter_analysis/frequency_response.cpp.

#include <numkit/signal/filter_design/filter_design.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch_arena.hpp>
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

void bilinearTransform(std::pmr::memory_resource *mr,
                       const Complex *sPoles, std::size_t sN,
                       double Wn,
                       ScratchVec<double> &bOut,
                       ScratchVec<double> &aOut)
{
    const int N = static_cast<int>(sN);

    ScratchVec<Complex> zPoles(static_cast<std::size_t>(N), mr);
    for (int i = 0; i < N; ++i) {
        const Complex sp = sPoles[i] * Wn;
        zPoles[i] = (1.0 + sp / 2.0) / (1.0 - sp / 2.0);
    }

    ScratchVec<Complex> zZeros(static_cast<std::size_t>(N), Complex(-1.0, 0.0), mr);

    aOut = polyExpandFromRoots(mr, zPoles.data(), zPoles.size());
    bOut = polyExpandFromRoots(mr, zZeros.data(), zZeros.size());

    Complex numDC(0, 0), denDC(0, 0);
    for (double v : bOut)
        numDC += v;
    for (double v : aOut)
        denDC += v;
    const double dcGain = std::abs(numDC / denDC);
    if (dcGain > 0.0)
        for (double &v : bOut)
            v /= dcGain;
}

void lpToHp(ScratchVec<double> &b, ScratchVec<double> &a)
{
    for (size_t i = 0; i < b.size(); ++i)
        if (i % 2 == 1)
            b[i] = -b[i];
    for (size_t i = 0; i < a.size(); ++i)
        if (i % 2 == 1)
            a[i] = -a[i];

    Complex numNyq(0, 0), denNyq(0, 0);
    for (size_t i = 0; i < b.size(); ++i)
        numNyq += b[i] * ((i % 2 == 0) ? 1.0 : -1.0);
    for (size_t i = 0; i < a.size(); ++i)
        denNyq += a[i] * ((i % 2 == 0) ? 1.0 : -1.0);
    const double nyqGain = std::abs(numNyq / denNyq);
    if (nyqGain > 0.0)
        for (double &v : b)
            v /= nyqGain;
}

} // anonymous namespace

std::tuple<Value, Value>
butter(Allocator &alloc, int N, double Wn, const std::string &type)
{
    if (Wn <= 0.0 || Wn >= 1.0)
        throw Error("butter: Wn must be between 0 and 1",
                     0, 0, "butter", "", "m:butter:badWn");
    if (type != "low" && type != "high")
        throw Error("butter: type must be 'low' or 'high'",
                     0, 0, "butter", "", "m:butter:badType");

    const double Wa = 2.0 * std::tan(M_PI * Wn / 2.0);

    ScratchArena scratch(alloc);
    auto sPoles = butterworthPoles(scratch.resource(), N);

    ScratchVec<double> b(scratch.resource()), a(scratch.resource());
    bilinearTransform(scratch.resource(), sPoles.data(), sPoles.size(), Wa, b, a);

    if (type == "high")
        lpToHp(b, a);

    auto bv = Value::matrix(1, b.size(), ValueType::DOUBLE, &alloc);
    auto av = Value::matrix(1, a.size(), ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < b.size(); ++i)
        bv.doubleDataMut()[i] = b[i];
    for (size_t i = 0; i < a.size(); ++i)
        av.doubleDataMut()[i] = a[i];

    return std::make_tuple(std::move(bv), std::move(av));
}

Value fir1(Allocator &alloc, int N, double Wn, const std::string &type)
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

    ScratchArena scratch(alloc);
    auto h = scratch.vec<double>(filtLen);
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

    auto bv = Value::matrix(1, filtLen, ValueType::DOUBLE, &alloc);
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

    auto [bv, av] = butter(ctx.engine->allocator(), N, Wn, type);
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

    outs[0] = fir1(ctx.engine->allocator(), N, Wn, type);
}

} // namespace detail

} // namespace numkit::signal
