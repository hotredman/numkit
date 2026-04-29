// libs/signal/src/waveform_generation/waveform_generation.cpp
//
// rectpuls, tripuls, gauspuls, pulstran (+ pulstranHandle), chirp.
// Split from library.cpp (transform-domain helpers nextpow2 /
// fftshift / ifftshift moved to transforms/transform_helpers.cpp).

#include <numkit/signal/waveform_generation/waveform_generation.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"  // createLike

#include <cctype>
#include <cmath>
#include <cstring>
#include <string>

namespace numkit::signal {

namespace {

double gauspulsAlpha(double fc, double bw)
{
    // MATLAB: gauspuls envelope is exp(-α·t²) where α is chosen so that
    // the spectrum hits -6 dB (bwr) at the bandwidth edge. With bwr = -6 dB
    // hard-coded, α = -(π·bw·fc)² / (4·log(0.5)).
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

Value rectpuls(std::pmr::memory_resource *mr, const Value &t, double w)
{
    if (w <= 0)
        throw Error("rectpuls: width w must be positive",
                     0, 0, "rectpuls", "", "m:rectpuls:badWidth");
    auto out = createLike(t, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t n = t.numel();
    for (size_t i = 0; i < n; ++i)
        dst[i] = rectpulsScalar(t.elemAsDouble(i), w);
    return out;
}

Value tripuls(std::pmr::memory_resource *mr, const Value &t, double w)
{
    if (w <= 0)
        throw Error("tripuls: width w must be positive",
                     0, 0, "tripuls", "", "m:tripuls:badWidth");
    auto out = createLike(t, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t n = t.numel();
    for (size_t i = 0; i < n; ++i)
        dst[i] = tripulsScalar(t.elemAsDouble(i), w);
    return out;
}

Value gauspuls(std::pmr::memory_resource *mr, const Value &t, double fc, double bw)
{
    if (fc <= 0 || bw <= 0)
        throw Error("gauspuls: fc and bw must be positive",
                     0, 0, "gauspuls", "", "m:gauspuls:badArg");
    const double alpha = gauspulsAlpha(fc, bw);
    auto out = createLike(t, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t n = t.numel();
    for (size_t i = 0; i < n; ++i)
        dst[i] = gauspulsScalar(t.elemAsDouble(i), fc, alpha);
    return out;
}

Value pulstranHandle(std::pmr::memory_resource *mr, const Value &t, const Value &d,
                      const Value &fnHandle, Engine *engine)
{
    if (engine == nullptr)
        throw Error("pulstran: custom function handles need an Engine "
                     "(callback API not available in this context)",
                     0, 0, "pulstran", "", "m:pulstran:fnUnsupported");
    if (!fnHandle.isFuncHandle())
        throw Error("pulstran: 3rd argument must be a function handle or pulse name",
                     0, 0, "pulstran", "", "m:pulstran:fnType");

    auto out = createLike(t, ValueType::DOUBLE, mr);
    const size_t n = t.numel();
    std::memset(out.doubleDataMut(), 0, n * sizeof(double));
    double *dst = out.doubleDataMut();
    const size_t nd = d.numel();

    auto shifted = Value::matrix(t.dims().rows(), t.dims().cols(),
                                  ValueType::DOUBLE, mr);
    double *sh = shifted.doubleDataMut();
    for (size_t k = 0; k < nd; ++k) {
        const double dk = d.elemAsDouble(k);
        for (size_t i = 0; i < n; ++i)
            sh[i] = t.elemAsDouble(i) - dk;
        Span<const Value> args(&shifted, 1);
        Value r = engine->callFunctionHandle(fnHandle, args);
        if (r.numel() != n)
            throw Error("pulstran: handle must return a vector of the same "
                         "length as t",
                         0, 0, "pulstran", "", "m:pulstran:badHandleOutput");
        for (size_t i = 0; i < n; ++i)
            dst[i] += r.elemAsDouble(i);
    }
    return out;
}

Value pulstran(std::pmr::memory_resource *mr, const Value &t, const Value &d,
                const std::string &fnName, double fcOrW, double bw)
{
    auto out = createLike(t, ValueType::DOUBLE, mr);
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
        throw Error("pulstran: unsupported pulse function '" + fnName
                     + "' (built-ins: 'rectpuls'/'tripuls'/'gauspuls'). "
                     + "Custom handles need the engine callback API (planned).",
                     0, 0, "pulstran", "", "m:pulstran:fnUnsupported");
    }
    return out;
}

Value chirp(std::pmr::memory_resource *mr, const Value &t,
             double f0, double t1, double f1,
             const std::string &method)
{
    if (t1 <= 0)
        throw Error("chirp: t1 must be positive",
                     0, 0, "chirp", "", "m:chirp:badT1");

    std::string m = method;
    for (auto &c : m) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    enum class Mode { Linear, Quadratic, Logarithmic };
    Mode mode;
    if      (m == "linear" || m.empty())   mode = Mode::Linear;
    else if (m == "quadratic")             mode = Mode::Quadratic;
    else if (m == "logarithmic")           mode = Mode::Logarithmic;
    else
        throw Error("chirp: method must be 'linear', 'quadratic', or 'logarithmic'",
                     0, 0, "chirp", "", "m:chirp:badMethod");

    if (mode == Mode::Logarithmic) {
        if (f0 <= 0 || f1 <= 0)
            throw Error("chirp: 'logarithmic' requires f0 > 0 and f1 > 0",
                         0, 0, "chirp", "", "m:chirp:badFreq");
        if (f0 == f1)
            throw Error("chirp: 'logarithmic' requires f0 != f1",
                         0, 0, "chirp", "", "m:chirp:badFreq");
    }

    auto out = createLike(t, ValueType::DOUBLE, mr);
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

namespace detail {

void rectpuls_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rectpuls: requires at least 1 argument",
                     0, 0, "rectpuls", "", "m:rectpuls:nargin");
    const double w = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    outs[0] = rectpuls(ctx.engine->resource(), args[0], w);
}

void tripuls_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("tripuls: requires at least 1 argument",
                     0, 0, "tripuls", "", "m:tripuls:nargin");
    const double w = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    outs[0] = tripuls(ctx.engine->resource(), args[0], w);
}

void gauspuls_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("gauspuls: requires at least 2 arguments (t, fc)",
                     0, 0, "gauspuls", "", "m:gauspuls:nargin");
    const double fc = args[1].toScalar();
    const double bw = (args.size() >= 3) ? args[2].toScalar() : 0.5;
    outs[0] = gauspuls(ctx.engine->resource(), args[0], fc, bw);
}

void pulstran_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("pulstran: requires at least 3 arguments (t, d, fn)",
                     0, 0, "pulstran", "", "m:pulstran:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args[2].isFuncHandle()) {
        outs[0] = pulstranHandle(mr, args[0], args[1], args[2], ctx.engine);
        return;
    }
    if (!args[2].isChar() && !args[2].isString())
        throw Error("pulstran: 3rd argument must be a string name or a function handle",
                     0, 0, "pulstran", "", "m:pulstran:fnType");
    const std::string fnName = args[2].toString();
    const double fcOrW = (args.size() >= 4) ? args[3].toScalar() : 1.0;
    const double bw    = (args.size() >= 5) ? args[4].toScalar() : 0.5;
    outs[0] = pulstran(mr, args[0], args[1], fnName, fcOrW, bw);
}

void chirp_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("chirp: requires at least 4 arguments (t, f0, t1, f1)",
                     0, 0, "chirp", "", "m:chirp:nargin");
    const double f0 = args[1].toScalar();
    const double t1 = args[2].toScalar();
    const double f1 = args[3].toScalar();
    std::string method = "linear";
    if (args.size() >= 5) {
        if (!args[4].isChar() && !args[4].isString())
            throw Error("chirp: method must be a string",
                         0, 0, "chirp", "", "m:chirp:badMethodType");
        method = args[4].toString();
    }
    outs[0] = chirp(ctx.engine->resource(), args[0], f0, t1, f1, method);
}

} // namespace detail

} // namespace numkit::signal
