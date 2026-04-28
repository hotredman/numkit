// libs/dsp/src/MDspSignalCore.cpp

#include <numkit/m/dsp/MDspSignalCore.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MDspHelpers.hpp"   // Complex typedef
#include "MStdHelpers.hpp"   // createLike

#include <cmath>

namespace numkit::m::dsp {

namespace {

// Cyclic shift core used by both fftshift and ifftshift — only the shift
// amount differs between them.
MValue cyclicShift(const MValue &x, size_t shift, Allocator &alloc)
{
    const size_t N = x.numel();
    if (x.isComplex()) {
        auto r = createLike(x, MType::COMPLEX, &alloc);
        const Complex *src = x.complexData();
        Complex *dst = r.complexDataMut();
        for (size_t i = 0; i < N; ++i)
            dst[i] = src[(i + shift) % N];
        return r;
    }
    auto r = createLike(x, MType::DOUBLE, &alloc);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    for (size_t i = 0; i < N; ++i)
        dst[i] = src[(i + shift) % N];
    return r;
}

} // anonymous namespace

// ── nextpow2 ──────────────────────────────────────────────────────────
MValue nextpow2(Allocator &alloc, double n)
{
    if (n <= 0)
        return MValue::scalar(0.0, &alloc);
    return MValue::scalar(std::ceil(std::log2(n)), &alloc);
}

// ── fftshift ──────────────────────────────────────────────────────────
MValue fftshift(Allocator &alloc, const MValue &x)
{
    return cyclicShift(x, x.numel() / 2, alloc);
}

// ── ifftshift ─────────────────────────────────────────────────────────
MValue ifftshift(Allocator &alloc, const MValue &x)
{
    return cyclicShift(x, (x.numel() + 1) / 2, alloc);
}

// ── chirp ─────────────────────────────────────────────────────────────
MValue chirp(Allocator &alloc, const MValue &t,
             double f0, double t1, double f1,
             const std::string &method)
{
    if (t1 <= 0)
        throw MError("chirp: t1 must be positive",
                     0, 0, "chirp", "", "m:chirp:badT1");

    // Lower-cased method name.
    std::string m = method;
    for (auto &c : m) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    enum class Mode { Linear, Quadratic, Logarithmic };
    Mode mode;
    if      (m == "linear" || m.empty())   mode = Mode::Linear;
    else if (m == "quadratic")             mode = Mode::Quadratic;
    else if (m == "logarithmic")           mode = Mode::Logarithmic;
    else
        throw MError("chirp: method must be 'linear', 'quadratic', or 'logarithmic'",
                     0, 0, "chirp", "", "m:chirp:badMethod");

    if (mode == Mode::Logarithmic) {
        if (f0 <= 0 || f1 <= 0)
            throw MError("chirp: 'logarithmic' requires f0 > 0 and f1 > 0",
                         0, 0, "chirp", "", "m:chirp:badFreq");
        if (f0 == f1)
            throw MError("chirp: 'logarithmic' requires f0 != f1",
                         0, 0, "chirp", "", "m:chirp:badFreq");
    }

    // Promote source to DOUBLE for the read path.
    auto out = createLike(t, MType::DOUBLE, &alloc);
    double *dst = out.doubleDataMut();
    const size_t N = t.numel();

    constexpr double kTwoPi = 6.28318530717958647692;

    auto readT = [&](size_t i) -> double { return t.elemAsDouble(i); };

    switch (mode) {
    case Mode::Linear: {
        const double k = (f1 - f0) / t1;
        for (size_t i = 0; i < N; ++i) {
            const double tv = readT(i);
            const double phase = kTwoPi * (f0 * tv + 0.5 * k * tv * tv);
            dst[i] = std::cos(phase);
        }
        break;
    }
    case Mode::Quadratic: {
        const double k = (f1 - f0) / (t1 * t1);
        for (size_t i = 0; i < N; ++i) {
            const double tv = readT(i);
            const double phase = kTwoPi * (f0 * tv + (k / 3.0) * tv * tv * tv);
            dst[i] = std::cos(phase);
        }
        break;
    }
    case Mode::Logarithmic: {
        const double beta = std::pow(f1 / f0, 1.0 / t1);
        const double logBeta = std::log(beta);
        for (size_t i = 0; i < N; ++i) {
            const double tv = readT(i);
            const double phase = kTwoPi * f0 * (std::pow(beta, tv) - 1.0) / logBeta;
            dst[i] = std::cos(phase);
        }
        break;
    }
    }
    return out;
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void nextpow2_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("nextpow2: requires 1 argument",
                     0, 0, "nextpow2", "", "m:nextpow2:nargin");
    outs[0] = nextpow2(ctx.engine->allocator(), args[0].toScalar());
}

void fftshift_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("fftshift: requires 1 argument",
                     0, 0, "fftshift", "", "m:fftshift:nargin");
    outs[0] = fftshift(ctx.engine->allocator(), args[0]);
}

void ifftshift_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("ifftshift: requires 1 argument",
                     0, 0, "ifftshift", "", "m:ifftshift:nargin");
    outs[0] = ifftshift(ctx.engine->allocator(), args[0]);
}

void chirp_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw MError("chirp: requires at least 4 arguments (t, f0, t1, f1)",
                     0, 0, "chirp", "", "m:chirp:nargin");
    const double f0 = args[1].toScalar();
    const double t1 = args[2].toScalar();
    const double f1 = args[3].toScalar();
    std::string method = "linear";
    if (args.size() >= 5) {
        if (!args[4].isChar() && !args[4].isString())
            throw MError("chirp: method must be a string",
                         0, 0, "chirp", "", "m:chirp:badMethodType");
        method = args[4].toString();
    }
    outs[0] = chirp(ctx.engine->allocator(), args[0], f0, t1, f1, method);
}

} // namespace detail

} // namespace numkit::m::dsp
