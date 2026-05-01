// libs/signal/src/filter_analysis/responses.cpp
//
// impz / impzlength / stepz / phasedelay / zerophase.

#include <numkit/signal/filter_analysis/responses.hpp>

#include <numkit/builtin/math/poly/polynomials.hpp>     // roots()
#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/signal/digital_filtering/filter.hpp>   // filter()
#include <numkit/signal/filter_analysis/frequency_response.hpp>  // freqz, phasez

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

// Build a length-n column vector of doubles from a generator.
template <typename Gen>
Value makeColVector(std::pmr::memory_resource *mr, size_t n, Gen &&g)
{
    auto v = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    double *dst = v.doubleDataMut();
    for (size_t i = 0; i < n; ++i) dst[i] = g(i);
    return v;
}

// 0-based time-index column vector [0, 1, ..., n-1] as doubles.
Value sampleIndex(std::pmr::memory_resource *mr, size_t n)
{
    return makeColVector(mr, n, [](size_t i) { return static_cast<double>(i); });
}

// True if `a` is "the trivial denominator" — scalar 1 or a vector
// whose only non-zero element is a(0) == 1 (within tolerance).
bool isTrivialA(const Value &a)
{
    if (a.isEmpty()) return true;
    const size_t n = a.numel();
    if (n == 0) return true;
    if (std::abs(a.elemAsDouble(0) - 1.0) > 1e-12) return false;
    for (size_t i = 1; i < n; ++i)
        if (std::abs(a.elemAsDouble(i)) > 1e-12) return false;
    return true;
}

// Largest-magnitude root of a polynomial. Empty / scalar → 0.
double maxRootRadius(std::pmr::memory_resource *mr, const Value &p)
{
    if (p.numel() < 2) return 0.0;
    auto r = builtin::roots(mr, p);
    double mx = 0.0;
    if (r.isComplex()) {
        const Complex *src = r.complexData();
        for (size_t i = 0; i < r.numel(); ++i)
            mx = std::max(mx, std::abs(src[i]));
    } else {
        const double *src = r.doubleData();
        for (size_t i = 0; i < r.numel(); ++i)
            mx = std::max(mx, std::abs(src[i]));
    }
    return mx;
}

} // namespace

// ── impzlength ────────────────────────────────────────────────────────
size_t impzlength(std::pmr::memory_resource *mr, const Value &b, const Value &a)
{
    if (isTrivialA(a)) {
        // FIR: response length is degree + 1 = numel(b).
        return std::max<size_t>(b.numel(), 1);
    }
    const double rho = maxRootRadius(mr, a);
    if (!(rho > 0.0) || rho >= 1.0) {
        // Unstable / undefined → cap at 8192 so callers don't
        // accidentally allocate a huge buffer.
        return 8192;
    }
    // Decay to 10^-5 → n ≈ -5 * log(10) / log(rho).
    const double n = -5.0 * std::log(10.0) / std::log(rho);
    long N = static_cast<long>(std::ceil(n));
    if (N < 50) N = 50;
    if (N > 8192) N = 8192;
    return static_cast<size_t>(N);
}

// ── impz ──────────────────────────────────────────────────────────────
std::tuple<Value, Value>
impz(std::pmr::memory_resource *mr, const Value &b, const Value &a, size_t n)
{
    if (n == 0) n = impzlength(mr, b, a);
    auto x = makeColVector(mr, n, [](size_t i) { return i == 0 ? 1.0 : 0.0; });
    auto h = filter(mr, b, a, x);
    return std::make_tuple(std::move(h), sampleIndex(mr, n));
}

// ── stepz ─────────────────────────────────────────────────────────────
std::tuple<Value, Value>
stepz(std::pmr::memory_resource *mr, const Value &b, const Value &a, size_t n)
{
    if (n == 0) n = impzlength(mr, b, a);
    auto x = makeColVector(mr, n, [](size_t) { return 1.0; });
    auto s = filter(mr, b, a, x);
    return std::make_tuple(std::move(s), sampleIndex(mr, n));
}

// ── phasedelay ────────────────────────────────────────────────────────
std::tuple<Value, Value>
phasedelay(std::pmr::memory_resource *mr, const Value &b, const Value &a, size_t n)
{
    auto [phi, w] = phasez(mr, b, a, n);
    // phi is column vector of length n; w too. Compute pd = -phi/w.
    auto pd = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    double *dst = pd.doubleDataMut();
    const double *phiP = phi.doubleData();
    const double *wP   = w.doubleData();
    for (size_t i = 0; i < n; ++i) {
        if (i == 0) {
            // 0/0 at DC — extrapolate from sample 1 (matches MATLAB shape).
            dst[i] = (n >= 2) ? -phiP[1] / wP[1] : 0.0;
        } else {
            dst[i] = -phiP[i] / wP[i];
        }
    }
    return std::make_tuple(std::move(pd), std::move(w));
}

// ── zerophase ─────────────────────────────────────────────────────────
// Compute the zero-phase response Hr(w): for symmetric / antisymmetric
// FIR filters this is real-valued (possibly negative). General formula:
//   Hr(w) = H(e^{jw}) * exp(j * w * (N-1)/2)         (FIR symmetric)
// We just multiply freqz by exp(j*w*tau) where tau is half the order.
std::tuple<Value, Value>
zerophase(std::pmr::memory_resource *mr, const Value &b, const Value &a, size_t n)
{
    auto [H, w] = freqz(mr, b, a, n);
    const double tau = (b.numel() > 0)
                       ? (static_cast<double>(b.numel()) - 1.0) / 2.0
                       : 0.0;
    auto Hr = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    double *dst = Hr.doubleDataMut();
    const Complex *Hc = H.isComplex() ? H.complexData() : nullptr;
    const double *Hd  = H.isComplex() ? nullptr : H.doubleData();
    const double *wP = w.doubleData();
    for (size_t i = 0; i < n; ++i) {
        const Complex hi = Hc ? Hc[i] : Complex(Hd[i], 0.0);
        const Complex twist = std::polar(1.0, wP[i] * tau);
        const Complex r = hi * twist;
        dst[i] = r.real();   // imag part is ~0 for true linear-phase filters
    }
    return std::make_tuple(std::move(Hr), std::move(w));
}

namespace detail {

void impz_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("impz: requires at least 1 argument (b)",
                     0, 0, "impz", "", "m:impz:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;
    auto [h, t] = impz(ctx.engine->resource(), b, a, n);
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(t);
}

void impzlength_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("impzlength: requires at least 1 argument (b)",
                     0, 0, "impzlength", "", "m:impzlength:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    const size_t n = impzlength(ctx.engine->resource(), b, a);
    outs[0] = Value::scalar(static_cast<double>(n), ctx.engine->resource());
}

void stepz_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("stepz: requires at least 1 argument (b)",
                     0, 0, "stepz", "", "m:stepz:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;
    auto [s, t] = stepz(ctx.engine->resource(), b, a, n);
    outs[0] = std::move(s);
    if (nargout > 1) outs[1] = std::move(t);
}

void phasedelay_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("phasedelay: requires at least 1 argument (b)",
                     0, 0, "phasedelay", "", "m:phasedelay:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 512;
    auto [pd, w] = phasedelay(ctx.engine->resource(), b, a, n);
    outs[0] = std::move(pd);
    if (nargout > 1) outs[1] = std::move(w);
}

void zerophase_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("zerophase: requires at least 1 argument (b)",
                     0, 0, "zerophase", "", "m:zerophase:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 512;
    auto [Hr, w] = zerophase(ctx.engine->resource(), b, a, n);
    outs[0] = std::move(Hr);
    if (nargout > 1) outs[1] = std::move(w);
}

} // namespace detail

} // namespace numkit::signal
