// toolboxes/signal/src/filter_analysis/responses.cpp
//
// impz / impzlength / stepz / phasedelay / zerophase.

#include <numkit/signal/filter_analysis/responses.hpp>

#include <numkit/builtin/polyfun.hpp>     // roots()
#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>
#include <tuple>
#include <numkit/signal/digital_filtering/filter.hpp>   // filter()
#include <numkit/signal/filter_analysis/frequency_response.hpp>  // freqz, phasez

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

// Build a length-n column vector of doubles from a generator.
template <typename Gen>
Value makeColVector(size_t n, Gen &&g, std::pmr::memory_resource *mr)
{
    auto v = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    double *dst = v.doubleDataMut();
    for (size_t i = 0; i < n; ++i) dst[i] = g(i);
    return v;
}

// 0-based time-index column vector [0, 1, ..., n-1] as doubles.
Value sampleIndex(size_t n, std::pmr::memory_resource *mr)
{
    return makeColVector(n, [](size_t i) { return static_cast<double>(i); }, mr);
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
double maxRootRadius(const Value &p, std::pmr::memory_resource *mr)
{
    if (p.numel() < 2) return 0.0;
    auto r = numkit::builtin::roots(p, mr);
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
//
// MATLAB convention: returns the number of samples needed for the
// impulse response to decay to 0.00005 (= 5e-5) of its peak. Formula:
//   N = floor(log(5e-5) / log(rho))
// where rho is the largest pole magnitude.
size_t impzlength(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    if (isTrivialA(a)) {
        // FIR: response length is degree + 1 = numel(b).
        return std::max<size_t>(b.numel(), 1);
    }
    const double rho = maxRootRadius(a, mr);
    if (!(rho > 0.0)) {
        // Trivial / no IIR contribution -> fall back to FIR length.
        return std::max<size_t>(b.numel(), 1);
    }
    if (rho >= 1.0) {
        // Unstable / undefined -> cap so callers don't accidentally
        // allocate a huge buffer. MATLAB also caps but at a larger
        // value; 8192 is a safe practical limit.
        return 8192;
    }
    // Decay to 5e-5 of initial amplitude.
    const double decayThresh = 5e-5;
    const double n = std::log(decayThresh) / std::log(rho);
    long N = static_cast<long>(std::floor(n));
    if (N < 1) N = 1;
    if (N > 8192) N = 8192;
    return static_cast<size_t>(N);
}

// ── impz ──────────────────────────────────────────────────────────────
std::tuple<Value, Value>
impz(const Value &b, const Value &a, size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0) n = impzlength(b, a, mr);
    auto x = makeColVector(n, [](size_t i) { return i == 0 ? 1.0 : 0.0; }, mr);
    auto h = filter(b, a, x, mr);
    return std::make_tuple(std::move(h), sampleIndex(n, mr));
}

// ── stepz ─────────────────────────────────────────────────────────────
std::tuple<Value, Value>
stepz(const Value &b, const Value &a, size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0) n = impzlength(b, a, mr);
    auto x = makeColVector(n, [](size_t) { return 1.0; }, mr);
    auto s = filter(b, a, x, mr);
    return std::make_tuple(std::move(s), sampleIndex(n, mr));
}

// ── phasedelay ────────────────────────────────────────────────────────
std::tuple<Value, Value>
phasedelay(const Value &b, const Value &a, size_t n, std::pmr::memory_resource *mr)
{
    auto [phi, w] = phasez(b, a, n, mr);
    // phi is column vector of length n; w too. Compute pd = -phi/w.
    auto pd = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    double *dst = pd.doubleDataMut();
    const double *phiP = phi.doubleData();
    const double *wP   = w.doubleData();
    // MATLAB: pd = -phi / w. At DC (w == 0) the ratio is 0/0 (or NaN/0
    // when the filter has a zero at DC) — both degenerate to NaN. Do
    // not extrapolate; let std::nan propagate so the caller sees the
    // documented MATLAB behaviour.
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    for (size_t i = 0; i < n; ++i) {
        if (wP[i] == 0.0) {
            dst[i] = kNaN;
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
zerophase(const Value &b, const Value &a, size_t n, std::pmr::memory_resource *mr)
{
    auto [H, w] = freqz(b, a, n, mr);
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

} // namespace numkit::signal
