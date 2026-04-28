// libs/dsp/src/MDspSignalCore.cpp

#include <numkit/m/dsp/MDspSignalCore.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MDspHelpers.hpp"   // Complex typedef
#include "MStdHelpers.hpp"   // createLike

#include <cctype>
#include <cmath>
#include <cstring>
#include <string>

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

// ── Pulse generators ──────────────────────────────────────────────────
namespace {

double gauspulsAlpha(double fc, double bw)
{
    // MATLAB: gauspuls envelope is exp(-α·t²) where α is chosen so that
    // the spectrum is bw·fc wide at the -6 dB (bwr) level. Derivation
    // gives α = (π·bw·fc)² / (2·log(10^(-bwr/-20))) = (π·bw·fc)² · 0.5
    // / log(10^0.3). Using bwr = -6 dB hard-coded:
    //   0.5 / log(10^0.3) = 0.5 / (0.3·ln(10)) ≈ 0.7237 ?
    // Reference numerical: alpha = -(π·bw·fc)² / (4·log(0.5)).
    // (because |H(f)|² at f = fc·(1+bw/2) hits -6 dB → factor 10^(-6/10)
    // = 0.25 = (1/2)² → log envelope: 0.5).
    constexpr double kPi = 3.14159265358979323846;
    const double pifb = kPi * fc * bw;
    return -(pifb * pifb) / (4.0 * std::log(0.5));
}

double rectpulsScalar(double tv, double w)
{
    const double half = 0.5 * w;
    const double a = std::abs(tv);
    if (a < half) return 1.0;
    return 0.0;  // boundary and outside
}

double tripulsScalar(double tv, double w)
{
    const double half = 0.5 * w;
    const double a = std::abs(tv);
    if (a >= half) return 0.0;
    return 1.0 - a / half;
}

double gauspulsScalar(double tv, double fc, double alpha)
{
    constexpr double kTwoPi = 6.28318530717958647692;
    return std::exp(-alpha * tv * tv) * std::cos(kTwoPi * fc * tv);
}

} // namespace

MValue rectpuls(Allocator &alloc, const MValue &t, double w)
{
    if (w <= 0)
        throw MError("rectpuls: width w must be positive",
                     0, 0, "rectpuls", "", "m:rectpuls:badWidth");
    auto out = createLike(t, MType::DOUBLE, &alloc);
    double *dst = out.doubleDataMut();
    const size_t n = t.numel();
    for (size_t i = 0; i < n; ++i)
        dst[i] = rectpulsScalar(t.elemAsDouble(i), w);
    return out;
}

MValue tripuls(Allocator &alloc, const MValue &t, double w)
{
    if (w <= 0)
        throw MError("tripuls: width w must be positive",
                     0, 0, "tripuls", "", "m:tripuls:badWidth");
    auto out = createLike(t, MType::DOUBLE, &alloc);
    double *dst = out.doubleDataMut();
    const size_t n = t.numel();
    for (size_t i = 0; i < n; ++i)
        dst[i] = tripulsScalar(t.elemAsDouble(i), w);
    return out;
}

MValue gauspuls(Allocator &alloc, const MValue &t, double fc, double bw)
{
    if (fc <= 0 || bw <= 0)
        throw MError("gauspuls: fc and bw must be positive",
                     0, 0, "gauspuls", "", "m:gauspuls:badArg");
    const double alpha = gauspulsAlpha(fc, bw);
    auto out = createLike(t, MType::DOUBLE, &alloc);
    double *dst = out.doubleDataMut();
    const size_t n = t.numel();
    for (size_t i = 0; i < n; ++i)
        dst[i] = gauspulsScalar(t.elemAsDouble(i), fc, alpha);
    return out;
}

MValue pulstranHandle(Allocator &alloc, const MValue &t, const MValue &d,
                      const MValue &fnHandle, Engine *engine)
{
    if (engine == nullptr)
        throw MError("pulstran: custom function handles need an Engine "
                     "(callback API not available in this context)",
                     0, 0, "pulstran", "", "m:pulstran:fnUnsupported");
    if (!fnHandle.isFuncHandle())
        throw MError("pulstran: 3rd argument must be a function handle or pulse name",
                     0, 0, "pulstran", "", "m:pulstran:fnType");

    auto out = createLike(t, MType::DOUBLE, &alloc);
    const size_t n = t.numel();
    std::memset(out.doubleDataMut(), 0, n * sizeof(double));
    double *dst = out.doubleDataMut();
    const size_t nd = d.numel();

    // Build a shifted-t vector once per delay and call fn(t - d_k).
    auto shifted = MValue::matrix(t.dims().rows(), t.dims().cols(),
                                  MType::DOUBLE, &alloc);
    double *sh = shifted.doubleDataMut();
    for (size_t k = 0; k < nd; ++k) {
        const double dk = d.elemAsDouble(k);
        for (size_t i = 0; i < n; ++i)
            sh[i] = t.elemAsDouble(i) - dk;
        Span<const MValue> args(&shifted, 1);
        MValue r = engine->callFunctionHandle(fnHandle, args);
        if (r.numel() != n)
            throw MError("pulstran: handle must return a vector of the same "
                         "length as t",
                         0, 0, "pulstran", "", "m:pulstran:badHandleOutput");
        for (size_t i = 0; i < n; ++i)
            dst[i] += r.elemAsDouble(i);
    }
    return out;
}

MValue pulstran(Allocator &alloc, const MValue &t, const MValue &d,
                const std::string &fnName, double fcOrW, double bw)
{
    auto out = createLike(t, MType::DOUBLE, &alloc);
    const size_t n = t.numel();
    std::memset(out.doubleDataMut(), 0, n * sizeof(double));
    double *dst = out.doubleDataMut();

    std::string lower;
    lower.reserve(fnName.size());
    for (char c : fnName)
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    auto applyKernel = [&](double (*kernel)(double, double, double),
                           double a, double b) {
        const size_t nd = d.numel();
        for (size_t k = 0; k < nd; ++k) {
            const double dk = d.elemAsDouble(k);
            for (size_t i = 0; i < n; ++i)
                dst[i] += kernel(t.elemAsDouble(i) - dk, a, b);
        }
    };

    if (lower == "rectpuls") {
        applyKernel([](double tv, double w, double) { return rectpulsScalar(tv, w); },
                    fcOrW, 0.0);
    } else if (lower == "tripuls") {
        applyKernel([](double tv, double w, double) { return tripulsScalar(tv, w); },
                    fcOrW, 0.0);
    } else if (lower == "gauspuls") {
        const double alpha = gauspulsAlpha(fcOrW, bw);
        applyKernel([](double tv, double fc, double a) { return gauspulsScalar(tv, fc, a); },
                    fcOrW, alpha);
    } else {
        throw MError("pulstran: unsupported pulse function '" + fnName
                     + "' (built-ins: 'rectpuls'/'tripuls'/'gauspuls'). "
                     + "Custom handles need the engine callback API (planned).",
                     0, 0, "pulstran", "", "m:pulstran:fnUnsupported");
    }
    return out;
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

void rectpuls_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("rectpuls: requires at least 1 argument",
                     0, 0, "rectpuls", "", "m:rectpuls:nargin");
    const double w = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    outs[0] = rectpuls(ctx.engine->allocator(), args[0], w);
}

void tripuls_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("tripuls: requires at least 1 argument",
                     0, 0, "tripuls", "", "m:tripuls:nargin");
    const double w = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    outs[0] = tripuls(ctx.engine->allocator(), args[0], w);
}

void gauspuls_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("gauspuls: requires at least 2 arguments (t, fc)",
                     0, 0, "gauspuls", "", "m:gauspuls:nargin");
    const double fc = args[1].toScalar();
    const double bw = (args.size() >= 3) ? args[2].toScalar() : 0.5;
    outs[0] = gauspuls(ctx.engine->allocator(), args[0], fc, bw);
}

void pulstran_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw MError("pulstran: requires at least 3 arguments (t, d, fn)",
                     0, 0, "pulstran", "", "m:pulstran:nargin");
    Allocator &alloc = ctx.engine->allocator();
    if (args[2].isFuncHandle()) {
        outs[0] = pulstranHandle(alloc, args[0], args[1], args[2], ctx.engine);
        return;
    }
    if (!args[2].isChar() && !args[2].isString())
        throw MError("pulstran: 3rd argument must be a string name or a function handle",
                     0, 0, "pulstran", "", "m:pulstran:fnType");
    const std::string fnName = args[2].toString();
    const double fcOrW = (args.size() >= 4) ? args[3].toScalar() : 1.0;
    const double bw    = (args.size() >= 5) ? args[4].toScalar() : 0.5;
    outs[0] = pulstran(alloc, args[0], args[1], fnName, fcOrW, bw);
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
