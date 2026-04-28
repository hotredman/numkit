// libs/dsp/src/MDspWindows.cpp

#include <numkit/m/signal/MDspWindows.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::m::signal {

namespace {

// Modified Bessel function of the first kind, order 0, via series expansion.
// Converges quickly for beta values typical of Kaiser window (0–20 range).
double besseli0(double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k <= 25; ++k) {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < 1e-16 * sum)
            break;
    }
    return sum;
}

} // anonymous namespace

// ── hamming ───────────────────────────────────────────────────────────
MValue hamming(Allocator &alloc, size_t N)
{
    auto r = MValue::matrix(N, 1, MType::DOUBLE, &alloc);
    if (N == 1) {
        r.doubleDataMut()[0] = 1.0;
        return r;
    }
    for (size_t i = 0; i < N; ++i)
        r.doubleDataMut()[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (N - 1));
    return r;
}

// ── hann ──────────────────────────────────────────────────────────────
MValue hann(Allocator &alloc, size_t N)
{
    auto r = MValue::matrix(N, 1, MType::DOUBLE, &alloc);
    if (N == 1) {
        r.doubleDataMut()[0] = 1.0;
        return r;
    }
    for (size_t i = 0; i < N; ++i)
        r.doubleDataMut()[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (N - 1)));
    return r;
}

// ── blackman ──────────────────────────────────────────────────────────
MValue blackman(Allocator &alloc, size_t N)
{
    auto r = MValue::matrix(N, 1, MType::DOUBLE, &alloc);
    if (N == 1) {
        r.doubleDataMut()[0] = 1.0;
        return r;
    }
    for (size_t i = 0; i < N; ++i) {
        const double x = 2.0 * M_PI * i / (N - 1);
        r.doubleDataMut()[i] = 0.42 - 0.5 * std::cos(x) + 0.08 * std::cos(2.0 * x);
    }
    return r;
}

// ── kaiser ────────────────────────────────────────────────────────────
MValue kaiser(Allocator &alloc, size_t N, double beta)
{
    auto r = MValue::matrix(N, 1, MType::DOUBLE, &alloc);
    if (N == 1) {
        r.doubleDataMut()[0] = 1.0;
        return r;
    }
    const double denom = besseli0(beta);
    for (size_t i = 0; i < N; ++i) {
        const double alpha = 2.0 * i / (N - 1) - 1.0;
        r.doubleDataMut()[i] = besseli0(beta * std::sqrt(1.0 - alpha * alpha)) / denom;
    }
    return r;
}

// ── rectwin ───────────────────────────────────────────────────────────
MValue rectwin(Allocator &alloc, size_t N)
{
    auto r = MValue::matrix(N, 1, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < N; ++i)
        r.doubleDataMut()[i] = 1.0;
    return r;
}

// ── bartlett ──────────────────────────────────────────────────────────
MValue bartlett(Allocator &alloc, size_t N)
{
    auto r = MValue::matrix(N, 1, MType::DOUBLE, &alloc);
    if (N == 1) {
        r.doubleDataMut()[0] = 1.0;
        return r;
    }
    const double half = (N - 1) / 2.0;
    for (size_t i = 0; i < N; ++i)
        r.doubleDataMut()[i] = 1.0 - std::abs((i - half) / half);
    return r;
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void hamming_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("hamming: requires 1 argument",
                     0, 0, "hamming", "", "m:hamming:nargin");
    outs[0] = hamming(ctx.engine->allocator(), static_cast<size_t>(args[0].toScalar()));
}

void hann_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("hann: requires 1 argument",
                     0, 0, "hann", "", "m:hann:nargin");
    outs[0] = hann(ctx.engine->allocator(), static_cast<size_t>(args[0].toScalar()));
}

void blackman_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("blackman: requires 1 argument",
                     0, 0, "blackman", "", "m:blackman:nargin");
    outs[0] = blackman(ctx.engine->allocator(), static_cast<size_t>(args[0].toScalar()));
}

void kaiser_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("kaiser: requires at least 1 argument",
                     0, 0, "kaiser", "", "m:kaiser:nargin");
    const size_t N = static_cast<size_t>(args[0].toScalar());
    const double beta = (args.size() >= 2) ? args[1].toScalar() : 0.5;
    outs[0] = kaiser(ctx.engine->allocator(), N, beta);
}

void rectwin_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("rectwin: requires 1 argument",
                     0, 0, "rectwin", "", "m:rectwin:nargin");
    outs[0] = rectwin(ctx.engine->allocator(), static_cast<size_t>(args[0].toScalar()));
}

void bartlett_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("bartlett: requires 1 argument",
                     0, 0, "bartlett", "", "m:bartlett:nargin");
    outs[0] = bartlett(ctx.engine->allocator(), static_cast<size_t>(args[0].toScalar()));
}

} // namespace detail

} // namespace numkit::m::signal
