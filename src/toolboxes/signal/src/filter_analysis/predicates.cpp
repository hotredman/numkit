// toolboxes/signal/src/filter_analysis/predicates.cpp
//
// isfir / isstable / isminphase / ismaxphase / islinphase / isallpass.

#include <numkit/signal/filter_analysis/predicates.hpp>
#include "predicates_detail.hpp"

#include <numkit/math/poly/polynomials.hpp>             // roots()
#include <numkit/signal/filter_analysis/frequency_response.hpp> // freqz()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <complex>

namespace numkit::signal {

namespace {

constexpr double kTol = 1e-12;

bool isTrivialA(const Value &a)
{
    if (a.isEmpty()) return true;
    const size_t n = a.numel();
    if (n == 0) return true;
    if (std::abs(a.elemAsDouble(0) - 1.0) > kTol) return false;
    for (size_t i = 1; i < n; ++i)
        if (std::abs(a.elemAsDouble(i)) > kTol) return false;
    return true;
}

double maxRootRadius(const Value &p, std::pmr::memory_resource *mr)
{
    if (p.numel() < 2) return 0.0;
    auto r = numkit::math::roots(p, mr);
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

// Strip trailing zeros from a polynomial vector — they don't change the
// polynomial value but may confuse symmetry tests on b.
std::vector<double> trimTrailingZeros(const Value &p)
{
    std::vector<double> v;
    v.reserve(p.numel());
    for (size_t i = 0; i < p.numel(); ++i)
        v.push_back(p.elemAsDouble(i));
    while (v.size() > 1 && std::abs(v.back()) <= kTol) v.pop_back();
    return v;
}

bool isSymmetric(const std::vector<double> &v, double scale, double tol)
{
    const size_t n = v.size();
    for (size_t i = 0; i < n / 2; ++i)
        if (std::abs(v[i] - scale * v[n - 1 - i]) > tol * (1.0 + std::abs(v[i])))
            return false;
    return true;
}


// ── isfir ─────────────────────────────────────────────────────────────
bool isfir(const Value &b)
{
    (void)b;
    return true;   // single-arg form: anything with implicit a=1 is FIR
}

bool isfir(const Value & /*b*/, const Value &a)
{
    return isTrivialA(a);
}

// ── isstable ──────────────────────────────────────────────────────────
bool isstable(const Value & /*b*/, const Value &a, std::pmr::memory_resource *mr)
{
    if (isTrivialA(a)) return true;          // FIR is always stable
    return maxRootRadius(a, mr) < 1.0 - 1e-9;
}

// ── isminphase ────────────────────────────────────────────────────────
bool isminphase(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    if (!isstable(b, a, mr)) return false;
    return maxRootRadius(b, mr) < 1.0 - 1e-9;
}

// ── ismaxphase ────────────────────────────────────────────────────────
bool ismaxphase(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    // All zeros must be OUTSIDE the unit circle. Implementation: invert
    // the polynomial (reverse coefficients) and check that *its* roots
    // are inside — equivalent and avoids min(abs(roots(b))) edge cases
    // when some roots are exactly on the unit circle.
    if (b.numel() < 2) return false;          // constant has no zeros
    auto coeffs = trimTrailingZeros(b);
    if (coeffs.size() < 2) return false;
    std::reverse(coeffs.begin(), coeffs.end());
    auto reversed = Value::matrix(1, coeffs.size(), ValueType::DOUBLE, mr);
    double *dst = reversed.doubleDataMut();
    for (size_t i = 0; i < coeffs.size(); ++i) dst[i] = coeffs[i];
    if (maxRootRadius(reversed, mr) >= 1.0 - 1e-9)
        return false;
    // Plus the filter must be FIR (denominator trivial) — IIR maxphase
    // is rare; MATLAB checks denominator separately.
    return isTrivialA(a);
}

// ── islinphase ────────────────────────────────────────────────────────
bool islinphase(const Value &b, const Value &a)
{
    if (!isTrivialA(a)) return false;        // IIR ≠ linear phase
    auto v = trimTrailingZeros(b);
    if (v.size() <= 1) return true;
    return isSymmetric(v, +1.0) || isSymmetric(v, -1.0);
}

// ── isallpass ─────────────────────────────────────────────────────────
// True when |H(e^{jw})| is constant. For real-coefficient transfer
// functions: a == flip(b) (up to a constant scale).
bool isallpass(const Value &b, const Value &a)
{
    auto bv = trimTrailingZeros(b);
    auto av = trimTrailingZeros(a);
    if (bv.size() != av.size() || bv.empty()) return false;
    // Find the scale factor between leading coefficient of a and trailing of b.
    if (std::abs(bv.back()) < kTol) return false;
    const double scale = av.front() / bv.back();
    for (size_t i = 0; i < bv.size(); ++i) {
        const double expected = scale * bv[bv.size() - 1 - i];
        if (std::abs(av[i] - expected) > 1e-9 * (1.0 + std::abs(av[i])))
            return false;
    }
    return true;
}

} // namespace numkit::signal
