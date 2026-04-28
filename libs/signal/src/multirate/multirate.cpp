// libs/signal/src/MDspResample.cpp

#include <numkit/signal/multirate/multirate.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

// Windowed-sinc lowpass FIR, Hamming window, cutoff wc (radians).
// Normalized so DC gain is 1. Order is filtLen - 1; filtLen must be >= 2.
std::vector<double> designLowpassFir(size_t filtLen, double wc)
{
    const size_t filtOrder = filtLen - 1;
    const double half = filtOrder / 2.0;

    std::vector<double> h(filtLen);
    double hSum = 0.0;
    for (size_t i = 0; i < filtLen; ++i) {
        const double n = i - half;
        const double sinc = (std::abs(n) < 1e-12)
                                ? wc / M_PI
                                : std::sin(wc * n) / (M_PI * n);
        const double win = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / filtOrder);
        h[i] = sinc * win;
        hSum += h[i];
    }
    for (size_t i = 0; i < filtLen; ++i)
        h[i] /= hSum;
    return h;
}

// Direct Form II transposed FIR apply — matches MDspFilter's core for
// the a = [1] denominator case. Used by decimate and resample.
std::vector<double> applyFirDf2t(const std::vector<double> &h, const double *x, size_t nx)
{
    const size_t filtLen = h.size();
    std::vector<double> out(nx);
    std::vector<double> z(filtLen, 0.0);
    for (size_t n = 0; n < nx; ++n) {
        out[n] = h[0] * x[n] + z[0];
        for (size_t i = 1; i < filtLen; ++i)
            z[i - 1] = h[i] * x[n] + (i < filtLen - 1 ? z[i] : 0.0);
    }
    return out;
}

} // anonymous namespace

// ── downsample ────────────────────────────────────────────────────────
Value downsample(Allocator &alloc, const Value &x, size_t n)
{
    const size_t nx = x.numel();
    const size_t outLen = (nx + n - 1) / n;
    const bool isRow = x.dims().rows() == 1;

    auto r = isRow ? Value::matrix(1, outLen, ValueType::DOUBLE, &alloc)
                   : Value::matrix(outLen, 1, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < outLen; ++i)
        r.doubleDataMut()[i] = x.doubleData()[i * n];
    return r;
}

// ── upsample ──────────────────────────────────────────────────────────
Value upsample(Allocator &alloc, const Value &x, size_t n)
{
    const size_t nx = x.numel();
    const size_t outLen = nx * n;
    const bool isRow = x.dims().rows() == 1;

    auto r = isRow ? Value::matrix(1, outLen, ValueType::DOUBLE, &alloc)
                   : Value::matrix(outLen, 1, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < outLen; ++i)
        r.doubleDataMut()[i] = 0.0;
    for (size_t i = 0; i < nx; ++i)
        r.doubleDataMut()[i * n] = x.doubleData()[i];
    return r;
}

// ── decimate ──────────────────────────────────────────────────────────
Value decimate(Allocator &alloc, const Value &x, size_t factor)
{
    const size_t nx = x.numel();
    const double *xd = x.doubleData();

    size_t filtOrder = 8 * factor;
    if (filtOrder >= nx)
        filtOrder = nx - 1;
    const size_t filtLen = filtOrder + 1;
    const double wc = M_PI / factor;

    auto h = designLowpassFir(filtLen, wc);
    auto filtered = applyFirDf2t(h, xd, nx);

    const size_t outLen = (nx + factor - 1) / factor;
    const bool isRow = x.dims().rows() == 1;
    auto r = isRow ? Value::matrix(1, outLen, ValueType::DOUBLE, &alloc)
                   : Value::matrix(outLen, 1, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < outLen; ++i)
        r.doubleDataMut()[i] = filtered[i * factor];
    return r;
}

// ── resample ──────────────────────────────────────────────────────────
Value resample(Allocator &alloc, const Value &x, size_t p, size_t q)
{
    const size_t nx = x.numel();
    const double *xd = x.doubleData();

    // Upsample by p (zero-stuff, multiply by p for gain)
    const size_t upLen = nx * p;
    std::vector<double> up(upLen, 0.0);
    for (size_t i = 0; i < nx; ++i)
        up[i * p] = static_cast<double>(p) * xd[i];

    // Anti-alias FIR lowpass
    size_t filtOrder = 10 * std::max(p, q);
    if (filtOrder >= upLen)
        filtOrder = upLen - 1;
    const size_t filtLen = filtOrder + 1;
    const double wc = M_PI / std::max(p, q);

    auto h = designLowpassFir(filtLen, wc);
    auto filtered = applyFirDf2t(h, up.data(), upLen);

    // Downsample by q
    const size_t outLen = (upLen + q - 1) / q;
    const bool isRow = x.dims().rows() == 1;
    auto r = isRow ? Value::matrix(1, outLen, ValueType::DOUBLE, &alloc)
                   : Value::matrix(outLen, 1, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < outLen; ++i) {
        const size_t idx = i * q;
        r.doubleDataMut()[i] = (idx < upLen) ? filtered[idx] : 0.0;
    }
    return r;
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void downsample_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("downsample: requires 2 arguments",
                     0, 0, "downsample", "", "m:downsample:nargin");
    outs[0] = downsample(ctx.engine->allocator(),
                         args[0],
                         static_cast<size_t>(args[1].toScalar()));
}

void upsample_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("upsample: requires 2 arguments",
                     0, 0, "upsample", "", "m:upsample:nargin");
    outs[0] = upsample(ctx.engine->allocator(),
                       args[0],
                       static_cast<size_t>(args[1].toScalar()));
}

void decimate_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("decimate: requires 2 arguments",
                     0, 0, "decimate", "", "m:decimate:nargin");
    outs[0] = decimate(ctx.engine->allocator(),
                       args[0],
                       static_cast<size_t>(args[1].toScalar()));
}

void resample_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("resample: requires 3 arguments",
                     0, 0, "resample", "", "m:resample:nargin");
    outs[0] = resample(ctx.engine->allocator(),
                       args[0],
                       static_cast<size_t>(args[1].toScalar()),
                       static_cast<size_t>(args[2].toScalar()));
}

} // namespace detail

} // namespace numkit::signal
