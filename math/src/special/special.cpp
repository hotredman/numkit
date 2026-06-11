// toolboxes/builtin/src/math/elementary/special.cpp
//
// Special functions — gamma / gammaln / erf / erfc / erfinv.

#include <numkit/math/special/special.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "helpers.hpp"

#define _USE_MATH_DEFINES
#include <cmath>
#include <limits>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::math {

// Emscripten / libc++ shim for the C++17 special-math Bessel family.
// libc++ does not implement P0226's `std::cyl_bessel_*`, so on the
// WASM build we route them through:
//   • POSIX `jn(int, double)` / `yn(int, double)` for non-negative
//     INTEGER order J_n / Y_n (these are in <math.h> on emscripten).
//   • A portable power-series + asymptotic implementation for
//     I_n / K_n at non-negative integer order (in `bessel_portable`
//     below — compiled on every platform so they can be gtest-
//     covered on desktop against `std::cyl_*`).
//   • A clear "non-integer order not yet supported on WASM" runtime
//     error for fractional ν (closes most of BUG #36 — integer-order
//     Bessel was the bulk of WASM usage; fractional ν is deferred to
//     a follow-up cycle).
// The desktop build (MSVC / libstdc++) keeps using `std::cyl_*` for
// `cyl_bessel_*` dispatch; the portable I/K helpers below stay
// reachable from gtest for parity verification.

namespace bessel_portable {

// Modified Bessel I_n(x) — power series for any x (converges
// absolutely for all real x). 200 terms cover |x| ≤ ~50 to 1e-15
// relative error; for larger |x| we use the asymptotic expansion.
// Integer-order only.
inline double bessel_I_series(int n, double x) {
    if (x == 0.0) return (n == 0) ? 1.0 : 0.0;
    const double half = 0.5 * x;
    const double half_sq = half * half;
    // term_0 = (x/2)^n / n!.
    double term = 1.0;
    for (int k = 1; k <= n; ++k) term *= half / k;
    double sum = term;
    for (int m = 1; m < 200; ++m) {
        term *= half_sq / (m * (m + n));
        sum += term;
        if (std::abs(term) < 1e-18 * std::abs(sum)) break;
    }
    return sum;
}
inline double bessel_I_asymp(int n, double x) {
    // I_n(x) ~ e^x / sqrt(2πx) · Σ (-1)^k a_k(n) / x^k, where
    // a_k(n) = (4n²-1²)(4n²-3²)...(4n²-(2k-1)²) / (k! 8^k).
    const double mu = 4.0 * static_cast<double>(n) * static_cast<double>(n);
    double sum = 1.0;
    double term = 1.0;
    for (int k = 1; k < 30; ++k) {
        const double odd = 2.0 * k - 1.0;
        term *= -(mu - odd * odd) / (k * 8.0 * x);
        sum += term;
        if (std::abs(term) < 1e-18 * std::abs(sum)) break;
    }
    return std::exp(x) / std::sqrt(2.0 * M_PI * x) * sum;
}
inline double bessel_I_int(int n, double x) {
    if (x < 0.0) {
        // I_n(-x) = (-1)^n · I_n(x).
        const double v = bessel_I_int(n, -x);
        return (n & 1) ? -v : v;
    }
    if (x == 0.0) return (n == 0) ? 1.0 : 0.0;
    // Switch to asymptotic once series convergence slows (|x| > ~15
    // for low n; conservative crossover at 20 keeps the series safe).
    if (x > 20.0) return bessel_I_asymp(n, x);
    return bessel_I_series(n, x);
}

// Modified Bessel K_n(x) for integer n >= 0, x > 0.
// Small x: K_0 / K_1 via series (Abramowitz & Stegun 9.6.13 / 9.6.11).
// Large x: asymptotic expansion (A&S 9.7.2).
// Higher n: forward recurrence K_{n+1} = (2n/x) K_n + K_{n-1}
//   (stable in the forward direction for K).
inline double bessel_K0_series(double x) {
    // Abramowitz & Stegun 9.6.13:
    //   K_0(x) = −(ln(x/2) + γ)·I_0(x) + Σ_{k=1}^∞ H_k/(k!)² · (x/2)^(2k)
    // where H_k = 1 + 1/2 + … + 1/k. We compute I_0 inline and add
    // the harmonic-weighted series term-by-term.
    constexpr double EULER = 0.5772156649015328606065120900824024;
    const double half = 0.5 * x;
    const double half_sq = half * half;
    // I_0(x) part — same series as bessel_I_series(0, x).
    double I0  = 1.0;
    double t_I = 1.0;
    for (int m = 1; m < 200; ++m) {
        t_I *= half_sq / (m * m);
        I0  += t_I;
        if (std::abs(t_I) < 1e-18 * std::abs(I0)) break;
    }
    // Harmonic series: Σ_{k=1} H_k / (k!)² · (x/2)^(2k).
    double harm = 0.0;
    double t_h  = 1.0;
    double H    = 0.0;
    for (int k = 1; k < 200; ++k) {
        t_h *= half_sq / (k * k);
        H   += 1.0 / k;
        const double inc = H * t_h;
        harm += inc;
        if (std::abs(inc) < 1e-18 * std::abs(harm) && k > 5) break;
    }
    return -(std::log(half) + EULER) * I0 + harm;
}
inline double bessel_K_asymp(int n, double x) {
    // K_n(x) ~ √(π/(2x)) · e^{-x} · Σ a_k(n) / x^k, where
    // a_k(n) = (4n²-1²)(4n²-3²)...(4n²-(2k-1)²) / (k! 8^k).
    // All terms positive for K (sign-alternating I version flipped).
    const double mu = 4.0 * static_cast<double>(n) * static_cast<double>(n);
    double sum = 1.0;
    double term = 1.0;
    for (int k = 1; k < 30; ++k) {
        const double odd = 2.0 * k - 1.0;
        term *= (mu - odd * odd) / (k * 8.0 * x);
        sum += term;
        if (std::abs(term) < 1e-18 * std::abs(sum)) break;
    }
    return std::sqrt(M_PI / (2.0 * x)) * std::exp(-x) * sum;
}
inline double bessel_K_int(int n, double x) {
    if (n < 0) n = -n;                   // K_{-n} = K_n
    if (x <= 0.0) return std::numeric_limits<double>::infinity();
    // K_0 series suffers catastrophic cancellation between
    //   −(ln(x/2)+γ)·I_0(x)   and   Σ H_k/(k!)² (x/2)^(2k)
    // for moderate x — at x=10 the two summands are ~6·10³ each and
    // the result K_0(10) ~ 1.8·10⁻⁵, so ~8 digits cancel.
    // Asymptotic expansion in 1/(8x) is accurate to ~1e-9 by x≈9
    // and to ~1e-13 by x≈20. Cross over at x>9 — this is the best
    // single-series compromise without Chebyshev mini-max.
    if (x > 9.0) return bessel_K_asymp(n, x);
    // Small x: series K_0, then K_1 via Wronskian
    //   I_0(x) K_1(x) + I_1(x) K_0(x) = 1/x
    // → K_1 = (1/x − I_1 K_0) / I_0.
    // Higher K_n via forward recurrence (stable for K).
    const double K0 = bessel_K0_series(x);
    if (n == 0) return K0;
    const double I0 = bessel_I_series(0, x);
    const double I1 = bessel_I_series(1, x);
    const double K1 = (1.0 / x - I1 * K0) / I0;
    if (n == 1) return K1;
    double km1 = K0, k = K1;
    for (int j = 1; j < n; ++j) {
        const double next = (2.0 * j / x) * k + km1;
        km1 = k;
        k   = next;
    }
    return k;
}

// Is `nu` a non-negative integer? (Accepts tiny round-off so a
// caller passing 3.0000000001 still matches n=3.)
inline bool isNonNegInt(double nu) {
    return nu >= 0.0 && std::abs(nu - std::round(nu)) < 1e-12;
}
// Is `nu` an integer (positive, negative, or zero)?
inline bool isInteger(double nu) {
    return std::abs(nu - std::round(nu)) < 1e-12;
}

// ── Fractional-order paths (any real ν, x > 0) ───────────────────────
//
// All four families use the standard power series in (x/2)² with the
// leading factor (x/2)^ν / Γ(ν+1). Series converges for any real x;
// in practice we limit to x ≤ ~30 for fractional orders — beyond
// that, alternating-series cancellation (J/Y) or term growth (I/K)
// eats precision. MATLAB scripts typically use fractional ν only at
// modest x (airy ζ < 10, ellipke z ≤ 1), so this covers production
// use. Asymptotic for fractional ν is deferred.
//
// Signs for the I-series are all-positive (term *= +half_sq / ...);
// for the J-series they alternate (term *= -half_sq / ...).

inline double bessel_J_series_real(double nu, double x) {
    // J_ν(x) = Σ_{m=0}^∞ (−1)^m / (m! Γ(ν+m+1)) · (x/2)^(ν+2m)
    if (x == 0.0) {
        if (nu == 0.0) return 1.0;
        return (nu > 0.0) ? 0.0
               : std::numeric_limits<double>::infinity();
    }
    const double half    = 0.5 * std::abs(x);
    const double half_sq = half * half;
    double term = std::pow(half, nu) / std::tgamma(nu + 1.0);
    double sum  = term;
    for (int m = 1; m < 300; ++m) {
        term *= -half_sq / (m * (nu + m));
        sum  += term;
        if (std::abs(term) < 1e-18 * std::abs(sum)) break;
    }
    // J_ν(-x) = (-1)^ν J_ν(x); for non-integer ν, MATLAB returns
    // complex. Here we mirror the desktop std::cyl_bessel_j domain
    // (defined for real x ≥ 0); negative-x branch is left to the
    // caller via the `besselj_reg` shim that promotes to complex.
    return sum;
}

inline double bessel_I_series_real(double nu, double x) {
    // I_ν(x) = Σ_{m=0}^∞ 1 / (m! Γ(ν+m+1)) · (x/2)^(ν+2m)
    if (x == 0.0) {
        if (nu == 0.0) return 1.0;
        return (nu > 0.0) ? 0.0
               : std::numeric_limits<double>::infinity();
    }
    const double half    = 0.5 * std::abs(x);
    const double half_sq = half * half;
    double term = std::pow(half, nu) / std::tgamma(nu + 1.0);
    double sum  = term;
    for (int m = 1; m < 300; ++m) {
        term *= half_sq / (m * (nu + m));
        sum  += term;
        if (std::abs(term) < 1e-18 * std::abs(sum)) break;
    }
    return sum;
}

// Y_ν via reflection (works for non-integer ν only; integer ν must
// route through bessel_portable::cyl_neumann's POSIX yn path).
//   Y_ν(x) = [J_ν(x)·cos(νπ) − J_{−ν}(x)] / sin(νπ)
inline double bessel_Y_frac(double nu, double x) {
    const double s = std::sin(nu * M_PI);
    if (std::abs(s) < 1e-300)
        return std::numeric_limits<double>::quiet_NaN();
    const double Jp = bessel_J_series_real( nu, x);
    const double Jm = bessel_J_series_real(-nu, x);
    return (Jp * std::cos(nu * M_PI) - Jm) / s;
}

// K_ν via reflection (works for non-integer ν only).
//   K_ν(x) = π / (2 sin(νπ)) · [I_{−ν}(x) − I_ν(x)]
inline double bessel_K_frac(double nu, double x) {
    const double s = std::sin(nu * M_PI);
    if (std::abs(s) < 1e-300)
        return std::numeric_limits<double>::quiet_NaN();
    const double Im = bessel_I_series_real(-nu, x);
    const double Ip = bessel_I_series_real( nu, x);
    return (M_PI / (2.0 * s)) * (Im - Ip);
}

} // namespace bessel_portable

namespace special_compat {
#ifdef __EMSCRIPTEN__

extern "C" double jn(int, double);
extern "C" double yn(int, double);

// Dispatch ν → integer fast-path or fractional series. For negative
// integer ν we route to the |ν| integer path with the appropriate
// parity flip: J_{−n}(x) = (−1)^n J_n(x), Y_{−n}(x) = (−1)^n Y_n(x),
// I_{−n} = I_n, K_{−n} = K_n.

inline double cyl_bessel_j(double nu, double x) {
    if (bessel_portable::isInteger(nu)) {
        const int n = static_cast<int>(std::round(std::abs(nu)));
        const double v = ::jn(n, x);
        return (nu < 0.0 && (n & 1)) ? -v : v;
    }
    return bessel_portable::bessel_J_series_real(nu, x);
}
inline double cyl_neumann(double nu, double x) {
    if (bessel_portable::isInteger(nu)) {
        const int n = static_cast<int>(std::round(std::abs(nu)));
        const double v = ::yn(n, x);
        return (nu < 0.0 && (n & 1)) ? -v : v;
    }
    return bessel_portable::bessel_Y_frac(nu, x);
}
inline double cyl_bessel_i(double nu, double x) {
    if (bessel_portable::isInteger(nu)) {
        const int n = static_cast<int>(std::round(std::abs(nu)));
        return bessel_portable::bessel_I_int(n, x);
    }
    return bessel_portable::bessel_I_series_real(nu, x);
}
inline double cyl_bessel_k(double nu, double x) {
    if (bessel_portable::isInteger(nu)) {
        const int n = static_cast<int>(std::round(std::abs(nu)));
        return bessel_portable::bessel_K_int(n, x);
    }
    return bessel_portable::bessel_K_frac(nu, x);
}
#else
using std::cyl_bessel_j;
using std::cyl_bessel_i;
using std::cyl_bessel_k;
using std::cyl_neumann;
#endif
} // namespace special_compat

namespace {

// Inverse error function via Winitzki's approximation + 3 Newton steps.
// Winitzki (2008) gives an initial estimate accurate to ~10⁻³ uniformly
// on (-1, 1); three Newton iterations on f(z) = erf(z) - y, f'(z) =
// 2/√π · exp(-z²) bring us to full double precision (the tails need
// the third step).
double erfinvScalar(double y)
{
    if (std::isnan(y))            return y;
    if (y >  1.0 || y < -1.0)     return std::nan("");
    if (y ==  1.0)                return std::numeric_limits<double>::infinity();
    if (y == -1.0)                return -std::numeric_limits<double>::infinity();
    if (y ==  0.0)                return 0.0;

    constexpr double kA  = 0.147;            // Winitzki constant
    constexpr double k2P = 2.0 / 3.14159265358979323846;
    const double s   = (y < 0) ? -1.0 : 1.0;
    const double ay  = std::abs(y);
    const double L   = std::log(1.0 - ay * ay);
    const double t   = k2P / kA + 0.5 * L;
    double z = s * std::sqrt(std::sqrt(t * t - L / kA) - t);

    constexpr double kInvSqrtPi = 0.56418958354775628694;
    for (int i = 0; i < 3; ++i) {
        const double err = std::erf(z) - y;
        const double dz  = err / (2.0 * kInvSqrtPi * std::exp(-z * z));
        z -= dz;
    }
    return z;
}

} // namespace

Value gammaFn(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) {
        // Gamma has poles at every non-positive integer; MATLAB returns +Inf
        // there (and also gamma(-Inf)=Inf). std::tgamma yields NaN at NEGATIVE
        // integers (it returns +Inf only at 0), so map v<=0 with v==floor(v) to
        // +Inf — this also covers -Inf (floor(-Inf)==-Inf). +Inf (v>0) and NaN
        // fall through to tgamma unchanged. bugs/builtin/gamma-negative-integer-poles.md.
        if (v <= 0.0 && v == std::floor(v))
            return std::numeric_limits<double>::infinity();
        return std::tgamma(v);
    }, mr);
}

Value gammaln(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return std::lgamma(v); }, mr);
}

// erf is backend-split (SLEEF-ported Highway dd kernel) and lives in
// special_highway.cpp + special_portable.cpp, like exp / log.

Value erfc(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return std::erfc(v); }, mr);
}

Value erfinv(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return erfinvScalar(v); }, mr);
}

Value erfcinv(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) {
        // Domain check: (0, 2). y = 0 → +Inf, y = 2 → -Inf.
        if (v <= 0.0) return std::numeric_limits<double>::infinity();
        if (v >= 2.0) return -std::numeric_limits<double>::infinity();
        return erfinvScalar(1.0 - v);
    }, mr);
}

Value erfcx(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) {
        // erfcx(x) = exp(x²) · erfc(x). Direct formula loses precision
        // for moderately large x; use the Cody / Faddeeva approach via
        // exp(x²) · erfc(x) and rely on libm's erfc accuracy.
        if (std::isnan(v)) return v;
        if (v < 0.0) {
            // erfcx(-x) = 2·exp(x²) - erfcx(x). Direct compute is fine
            // for negative x of small magnitude.
            return std::exp(v * v) * std::erfc(v);
        }
        // Large x: erfc(x) ~ exp(-x²)/(x √π) · (1 − 1/(2x²) + ...)
        // so erfcx(x) ~ 1/(x √π) · series. Use direct exp·erfc when
        // x² < ~700 (no overflow in exp(x²)·tiny).
        if (v < 26.0) return std::exp(v * v) * std::erfc(v);
        // Asymptotic series for x >= 26 (erfc(x) ~ exp(-x²)/(x √π) · ...)
        const double inv_sqrt_pi = 0.5641895835477563;
        const double y = 1.0 / (v * v);
        return inv_sqrt_pi / v
             * (1.0 - 0.5 * y + 0.75 * y * y - 1.875 * y * y * y);
    }, mr);
}

// ── Pack 19: beta / betaln / expint / psi ────────────────────────────

Value beta(const Value &z, const Value &w, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(z, w, [](double zz, double ww) {
        // Use the lgamma-then-exp form to avoid overflow at moderate
        // arguments. Sign is positive whenever z, w > 0; for negative
        // arguments we fall back to direct tgamma if its absolute value
        // is finite.
        if (zz > 0.0 && ww > 0.0)
            return std::exp(std::lgamma(zz) + std::lgamma(ww) - std::lgamma(zz + ww));
        return std::tgamma(zz) * std::tgamma(ww) / std::tgamma(zz + ww);
    }, mr);
}

Value betaln(const Value &z, const Value &w, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(z, w, [](double zz, double ww) {
        return std::lgamma(zz) + std::lgamma(ww) - std::lgamma(zz + ww);
    }, mr);
}

namespace {
// E1(x) for x > 0 via the standard regimes:
//   x ≤ 1: power-series  E1(x) = -γ - ln(x) - Σ ((-1)^n x^n) / (n·n!)
//   x > 1: continued-fraction (Lentz) E1(x) = e^{-x} · CF(x)
// Accurate to ~1e-12 for typical real inputs.
double expintScalar(double x)
{
    if (std::isnan(x)) return x;
    if (x == 0.0) return std::numeric_limits<double>::infinity();
    if (x < 0.0) return std::numeric_limits<double>::quiet_NaN();
    constexpr double EUL = 0.57721566490153286060;
    if (x <= 1.0) {
        double term = 1.0;
        double sum = 0.0;
        for (int n = 1; n <= 100; ++n) {
            term *= -x / static_cast<double>(n);
            const double add = -term / static_cast<double>(n);
            sum += add;
            if (std::abs(add) < std::abs(sum) * 1e-16) break;
        }
        return -EUL - std::log(x) + sum;
    }
    // Continued fraction (Numerical Recipes form).
    constexpr double TINY = 1e-300;
    double b = x + 1.0;
    double c = 1.0 / TINY;
    double d = 1.0 / b;
    double h = d;
    for (int i = 1; i <= 200; ++i) {
        const double a = -static_cast<double>(i * i);
        b += 2.0;
        d = 1.0 / (a * d + b);
        c = b + a / c;
        const double del = c * d;
        h *= del;
        if (std::abs(del - 1.0) < 1e-16) break;
    }
    return h * std::exp(-x);
}

double psiScalar(double x)
{
    if (std::isnan(x)) return x;
    // Pole at 0: MATLAB ψ(0) = -Inf. bugs/builtin/psi-zero-pole.md.
    if (x == 0.0)
        return -std::numeric_limits<double>::infinity();
    // Negative integers are also poles; numkit returns NaN here. (MATLAB
    // rejects ALL negative input — "X must be nonnegative" — so its exact value
    // at these points is moot; we keep the lenient NaN rather than erroring.)
    if (x == std::floor(x) && x < 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    // Reflection for x < 0.5: ψ(1-x) - ψ(x) = π·cot(π·x)
    double r = 0.0;
    double y = x;
    if (y < 0.5) {
        r -= 3.14159265358979323846 / std::tan(3.14159265358979323846 * y);
        y = 1.0 - y;
    }
    // Recurrence to push y ≥ 6 for asymptotic accuracy.
    while (y < 6.0) {
        r -= 1.0 / y;
        y += 1.0;
    }
    // Asymptotic series: ψ(y) = ln y - 1/(2y) - Σ B_{2k}/(2k·y^{2k})
    const double yi = 1.0 / y;
    const double yi2 = yi * yi;
    double s = std::log(y) - 0.5 * yi;
    // B2/2 = 1/12, B4/4 = -1/120, B6/6 = 1/252, B8/8 = -1/240, B10/10 = 5/660
    s -= yi2 * (1.0 / 12.0
              - yi2 * (1.0 / 120.0
                     - yi2 * (1.0 / 252.0
                            - yi2 * (1.0 / 240.0
                                   - yi2 * (5.0 / 660.0)))));
    return r + s;
}
} // anon

Value expint(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return expintScalar(v); }, mr);
}

Value psi(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return psiScalar(v); }, mr);
}

// ── Pack 26: incomplete gamma / beta / Legendre ──────────────────────

namespace {
// Regularized lower incomplete gamma P(a, x) = γ(a,x)/Γ(a).
// Series form (good for x < a+1) and continued-fraction form (good for
// x ≥ a+1). Numerical Recipes layout.
double gserScalar(double a, double x)
{
    if (x <= 0.0) return 0.0;
    constexpr int kMaxIter = 200;
    constexpr double kEps = 1e-15;
    double ap = a;
    double summ = 1.0 / a;
    double del = summ;
    for (int n = 0; n < kMaxIter; ++n) {
        ap += 1.0;
        del *= x / ap;
        summ += del;
        if (std::abs(del) < std::abs(summ) * kEps) break;
    }
    return summ * std::exp(-x + a * std::log(x) - std::lgamma(a));
}

double gcfScalar(double a, double x)
{
    constexpr int kMaxIter = 200;
    constexpr double kEps = 1e-15;
    constexpr double kFpMin = 1e-300;
    double b = x + 1.0 - a;
    double c = 1.0 / kFpMin;
    double d = 1.0 / b;
    double h = d;
    for (int i = 1; i <= kMaxIter; ++i) {
        const double an = -i * (i - a);
        b += 2.0;
        d = an * d + b;
        if (std::abs(d) < kFpMin) d = kFpMin;
        c = b + an / c;
        if (std::abs(c) < kFpMin) c = kFpMin;
        d = 1.0 / d;
        const double del = d * c;
        h *= del;
        if (std::abs(del - 1.0) < kEps) break;
    }
    const double Q = std::exp(-x + a * std::log(x) - std::lgamma(a)) * h;
    return 1.0 - Q;
}

double gammaincScalar(double x, double a)
{
    if (std::isnan(x) || std::isnan(a)) return std::numeric_limits<double>::quiet_NaN();
    if (x < 0.0 || a <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    if (x == 0.0) return 0.0;
    if (x < a + 1.0) return gserScalar(a, x);
    return gcfScalar(a, x);
}

// Regularized incomplete beta I_x(a, b) = B(x; a, b) / B(a, b).
// Continued-fraction (Lentz) per Numerical Recipes.
double betacfScalar(double a, double b, double x)
{
    constexpr int kMaxIter = 200;
    constexpr double kEps = 1e-15;
    constexpr double kFpMin = 1e-300;
    const double qab = a + b, qap = a + 1.0, qam = a - 1.0;
    double c = 1.0;
    double d = 1.0 - qab * x / qap;
    if (std::abs(d) < kFpMin) d = kFpMin;
    d = 1.0 / d;
    double h = d;
    for (int m = 1; m <= kMaxIter; ++m) {
        const int m2 = 2 * m;
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1.0 + aa * d;
        if (std::abs(d) < kFpMin) d = kFpMin;
        c = 1.0 + aa / c;
        if (std::abs(c) < kFpMin) c = kFpMin;
        d = 1.0 / d;
        h *= d * c;
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1.0 + aa * d;
        if (std::abs(d) < kFpMin) d = kFpMin;
        c = 1.0 + aa / c;
        if (std::abs(c) < kFpMin) c = kFpMin;
        d = 1.0 / d;
        const double del = d * c;
        h *= del;
        if (std::abs(del - 1.0) < kEps) break;
    }
    return h;
}

double betaincScalar(double x, double a, double b)
{
    if (std::isnan(x) || std::isnan(a) || std::isnan(b))
        return std::numeric_limits<double>::quiet_NaN();
    if (x < 0.0 || x > 1.0 || a <= 0.0 || b <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    if (x == 0.0) return 0.0;
    if (x == 1.0) return 1.0;
    const double bt = std::exp(std::lgamma(a + b) - std::lgamma(a) - std::lgamma(b)
                                + a * std::log(x) + b * std::log(1.0 - x));
    if (x < (a + 1.0) / (a + b + 2.0))
        return bt * betacfScalar(a, b, x) / a;
    return 1.0 - bt * betacfScalar(b, a, 1.0 - x) / b;
}
} // anon

Value gammainc(const Value &x, const Value &a, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(x, a,
        [](double xx, double aa) { return gammaincScalar(xx, aa); }, mr);
}

Value betainc(const Value &x, const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    // Compose two element-wise binaries: first build a tmp matrix of
    // (a, b) → fold with x as outer. Easier: walk arrays in parallel
    // here for the broadcast-scalar / same-shape cases.
    if (x.isScalar() && a.isScalar() && b.isScalar()) {
        return Value::scalar(
            betaincScalar(x.toScalar(), a.toScalar(), b.toScalar()), mr);
    }
    // For non-scalar inputs, require matching shape (no broadcast for
    // 3-arg). MATLAB allows broadcasting too; covered by repeated unary.
    const size_t nx = x.numel(), na = a.numel(), nb = b.numel();
    const size_t n = std::max({nx, na, nb});
    auto pickShape = [&]() -> const Value & {
        if (nx == n) return x;
        if (na == n) return a;
        return b;
    };
    auto r = createLike(pickShape(), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < n; ++i) {
        const double xi = (nx == 1) ? x.toScalar() : x.doubleData()[i];
        const double ai = (na == 1) ? a.toScalar() : a.doubleData()[i];
        const double bi = (nb == 1) ? b.toScalar() : b.doubleData()[i];
        r.doubleDataMut()[i] = betaincScalar(xi, ai, bi);
    }
    return r;
}

Value legendre(int n, const Value &x, std::pmr::memory_resource *mr)
{
    if (n < 0)
        throw Error("legendre: n must be >= 0", 0, 0, "legendre", "",
                     "numkit:legendre:badN");
    const size_t L = x.numel();
    auto r = Value::matrix(static_cast<size_t>(n + 1), L, ValueType::DOUBLE, mr);
    double *dst = r.doubleDataMut();
    // Build P_n^m(x) for m = 0..n at each x_k.
    for (size_t k = 0; k < L; ++k) {
        const double xk = x.elemAsDouble(k);
        // P_0^0 = 1.
        std::vector<double> P(n + 1, 0.0);
        // Use the standard recursion. Compute P_n^m via:
        //   P_m^m = (-1)^m (2m-1)!! (1-x²)^(m/2)
        //   P_{m+1}^m = x (2m+1) P_m^m
        //   P_l^m = ((2l-1) x P_{l-1}^m - (l+m-1) P_{l-2}^m) / (l - m)
        const double sx = std::sqrt(std::max(0.0, 1.0 - xk * xk));
        for (int m = 0; m <= n; ++m) {
            // P_m^m = (-1)^m * (2m-1)!! * (sin θ)^m, where sin θ = sqrt(1-x²).
            double pmm = 1.0;
            for (int i = 1; i <= m; ++i)
                pmm *= -(2.0 * i - 1.0) * sx;
            if (n == m) { P[m] = pmm; continue; }
            // P_{m+1}^m = x (2m+1) P_m^m.
            double pmmp1 = xk * (2.0 * m + 1.0) * pmm;
            if (n == m + 1) { P[m] = pmmp1; continue; }
            // Recur up to l = n.
            double pll = 0.0;
            for (int l = m + 2; l <= n; ++l) {
                pll = ((2.0 * l - 1.0) * xk * pmmp1
                        - (l + m - 1.0) * pmm) / (l - m);
                pmm = pmmp1;
                pmmp1 = pll;
            }
            P[m] = pmmp1;
        }
        // Write column k.
        for (int m = 0; m <= n; ++m)
            dst[k * (n + 1) + m] = P[m];
    }
    return r;
}

// ── Pack 27: Bessel (C++17 std::cyl_* special math) ──────────────────

Value besselj(const Value &nu, const Value &x, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(nu, x,
        [](double n, double xx) { return special_compat::cyl_bessel_j(n, xx); }, mr);
}

Value bessely(const Value &nu, const Value &x, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(nu, x,
        [](double n, double xx) { return special_compat::cyl_neumann(n, xx); }, mr);
}

Value besseli(const Value &nu, const Value &x, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(nu, x,
        [](double n, double xx) { return special_compat::cyl_bessel_i(n, xx); }, mr);
}

Value besselk(const Value &nu, const Value &x, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(nu, x,
        [](double n, double xx) { return special_compat::cyl_bessel_k(n, xx); }, mr);
}

// ── Pack 28: Hankel + complete elliptic integrals ────────────────────

Value besselh(const Value &nu, int k, const Value &x, std::pmr::memory_resource *mr)
{
    if (k != 1 && k != 2)
        throw Error("besselh: k must be 1 or 2",
                     0, 0, "besselh", "", "numkit:besselh:badK");
    // Build a complex array of J_ν(x) + (k==1 ? +1 : −1) · i · Y_ν(x).
    // elementwiseComplex requires both operands to be complex; we build
    // the result in a real-imag pass instead.
    if (nu.isScalar() && x.isScalar()) {
        const double n = nu.toScalar();
        const double xx = x.toScalar();
        const double j = special_compat::cyl_bessel_j(n, xx);
        const double y = special_compat::cyl_neumann(n, xx);
        const double sign = (k == 1) ? 1.0 : -1.0;
        return Value::complexScalar(Complex(j, sign * y), mr);
    }
    // Use the reference operand for shape (whichever is non-scalar).
    const Value &shape = !nu.isScalar() ? nu : x;
    auto r = createLike(shape, ValueType::COMPLEX, mr);
    Complex *dst = r.complexDataMut();
    const size_t N = shape.numel();
    const double sign = (k == 1) ? 1.0 : -1.0;
    for (size_t i = 0; i < N; ++i) {
        const double n  = nu.isScalar() ? nu.toScalar()
                                         : nu.doubleData()[i];
        const double xx = x.isScalar() ? x.toScalar()
                                       : x.doubleData()[i];
        dst[i] = Complex(special_compat::cyl_bessel_j(n, xx),
                         sign * special_compat::cyl_neumann(n, xx));
    }
    return r;
}

namespace {
// Complete elliptic integrals K(m) and E(m) via the AGM.
//
// Setup: a_0 = 1, g_0 = sqrt(1-m), c_0² = m.
//   AGM step: a_{n+1} = (a_n+g_n)/2, g_{n+1} = sqrt(a_n g_n).
//   c_{n+1}² = ((a_n-g_n)/2)² = c_n² / 4·(...) → easier: c_{n+1} = (a_n-g_n)/2.
//   K(m) = π / (2·AGM)
//   E(m) = K(m) · (1 − Σ_{n=0}^{∞} 2^(n-1) c_n²)
std::pair<double, double> ellipKEScalar(double m)
{
    if (std::isnan(m)) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return {nan, nan};
    }
    if (m == 1.0) return {std::numeric_limits<double>::infinity(), 1.0};
    if (m < 0.0 || m > 1.0)
        return {std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::quiet_NaN()};

    double a = 1.0, g = std::sqrt(1.0 - m);
    // n = 0: term = 0.5 · m.
    double sum = 0.5 * m;
    // For n = 1, 2, ... we'll add 2^(n-1) · c_n², where
    // c_n = (a_{n-1} - g_{n-1}) / 2  →  c_n² = (a-g)² / 4.
    // So term_n = 2^(n-1) · (a-g)² / 4 = 2^(n-3) · (a-g)².
    // Build `scale` starting at 2^(1-3) = 0.25 and double each step.
    double scale = 0.25;
    constexpr int kMaxIter = 64;
    for (int i = 0; i < kMaxIter; ++i) {
        const double diff = a - g;
        if (std::abs(diff) < 1e-17 * a) break;
        sum += scale * diff * diff;
        const double aNew = 0.5 * (a + g);
        const double gNew = std::sqrt(a * g);
        a = aNew; g = gNew;
        scale *= 2.0;
    }
    const double K = 3.14159265358979323846 / (2.0 * a);
    const double E = K * (1.0 - sum);
    return {K, E};
}
} // anon

EllipKE ellipke(const Value &m, std::pmr::memory_resource *mr)
{
    auto K = createLike(m, ValueType::DOUBLE, mr);
    auto E = createLike(m, ValueType::DOUBLE, mr);
    if (m.isScalar()) {
        const auto [k, e] = ellipKEScalar(m.toScalar());
        return { Value::scalar(k, mr), Value::scalar(e, mr) };
    }
    const size_t n = m.numel();
    for (size_t i = 0; i < n; ++i) {
        const auto [k, e] = ellipKEScalar(m.doubleData()[i]);
        K.doubleDataMut()[i] = k;
        E.doubleDataMut()[i] = e;
    }
    return { std::move(K), std::move(E) };
}

// ── Pack 36: Airy functions via Bessel connection formulae ──────────
namespace {

// Airy at scalar x for kind k ∈ {0,1,2,3}.
//   k=0: Ai(x), k=1: Ai'(x), k=2: Bi(x), k=3: Bi'(x).
// Uses DLMF §9.6: connection to modified Bessel (x>0) and J Bessel (x<0).
// At x=0 we return the analytic constants (Bessel formulas have removable
// singularities there).
double airyScalar(int k, double x)
{
    if (std::isnan(x)) return x;

    // Constants at x = 0 (DLMF 9.2):
    //   Ai(0)  = 1 / (3^{2/3} Γ(2/3))
    //   Ai'(0) = -1 / (3^{1/3} Γ(1/3))
    //   Bi(0)  = 1 / (3^{1/6} Γ(2/3))
    //   Bi'(0) = 3^{1/6} / Γ(1/3)
    if (x == 0.0) {
        constexpr double kAi0  =  0.35502805388781723926;
        constexpr double kAip0 = -0.25881940379280679841;
        constexpr double kBi0  =  0.61492662744600073516;
        constexpr double kBip0 =  0.44828835735382635791;
        switch (k) {
            case 0: return kAi0;
            case 1: return kAip0;
            case 2: return kBi0;
            case 3: return kBip0;
            default: return std::nan("");
        }
    }

    constexpr double kSqrt3   = 1.7320508075688772;
    constexpr double kInvPi   = 0.31830988618379067;
    constexpr double kInvSqrt3= 0.57735026918962576;

    if (x > 0.0) {
        // ζ = (2/3) x^{3/2}
        const double zeta = (2.0 / 3.0) * x * std::sqrt(x);
        const double sx3  = std::sqrt(x / 3.0);          // sqrt(x/3)
        const double i13p = special_compat::cyl_bessel_i( 1.0/3.0, zeta);
        const double i13m = special_compat::cyl_bessel_i(-1.0/3.0, zeta);
        const double i23p = special_compat::cyl_bessel_i( 2.0/3.0, zeta);
        const double i23m = special_compat::cyl_bessel_i(-2.0/3.0, zeta);
        const double k13  = special_compat::cyl_bessel_k( 1.0/3.0, zeta);
        const double k23  = special_compat::cyl_bessel_k( 2.0/3.0, zeta);
        switch (k) {
            case 0: return kInvPi * sx3 * k13;
            case 1: return -(x * kInvSqrt3) * kInvPi * k23;
            case 2: return sx3 * (i13p + i13m);
            case 3: return (x * kInvSqrt3) * (i23p + i23m);
            default: return std::nan("");
        }
    }

    // x < 0
    const double ax   = -x;
    const double zeta = (2.0 / 3.0) * ax * std::sqrt(ax);
    const double sax  = std::sqrt(ax);
    const double j13p = special_compat::cyl_bessel_j( 1.0/3.0, zeta);
    const double j13m = special_compat::cyl_bessel_j(-1.0/3.0, zeta);
    const double j23p = special_compat::cyl_bessel_j( 2.0/3.0, zeta);
    const double j23m = special_compat::cyl_bessel_j(-2.0/3.0, zeta);
    // Sign of Ai'(x<0) follows Abramowitz & Stegun 10.4.18:
    //   Ai'(-z) = (z/3) (J_{2/3}(ζ) - J_{-2/3}(ζ)).
    // (DLMF 9.6.7 prepends an extra minus that disagrees with MATLAB
    // and the standard tabulated values; A&S sign matches MATLAB.)
    switch (k) {
        case 0: return (sax / 3.0) * (j13p + j13m);
        case 1: return (ax  / 3.0) * (j23p - j23m);
        case 2: return (sax * kInvSqrt3) * (j13m - j13p);
        case 3: return (ax  * kInvSqrt3) * (j23p + j23m);
        default: return std::nan("");
    }
}

} // namespace

Value airy(int k, const Value &x, std::pmr::memory_resource *mr)
{
    if (k < 0 || k > 3)
        throw Error("airy: kind k must be 0..3 (got " + std::to_string(k) + ")",
                     0, 0, "airy", "", "numkit:airy:badK");
    return unaryDouble(x, [k](double v) { return airyScalar(k, v); }, mr);
}

// ── Pack 36: gammaincinv / betaincinv / ellipj ──────────────────────
namespace {

// Inverse regularized lower incomplete gamma. Solves gammainc(x, a) = P.
// Uses Wilson-Hilferty initial guess, then Newton.
double gammaincinvScalar(double P, double a)
{
    if (std::isnan(P) || std::isnan(a))            return std::nan("");
    if (a <= 0.0)                                  return std::nan("");
    if (P < 0.0 || P > 1.0)                        return std::nan("");
    if (P == 0.0)                                  return 0.0;
    if (P == 1.0)                                  return std::numeric_limits<double>::infinity();

    // Wilson-Hilferty: chi^2_{2a} ≈ 2a (1 - 1/(9a) + Z/sqrt(9a))^3,
    // where Z = sqrt(2)*erfinv(2P-1) is the standard-normal quantile.
    const double Z   = std::sqrt(2.0) * erfinvScalar(2.0 * P - 1.0);
    const double t   = 1.0 - 1.0 / (9.0 * a) + Z / std::sqrt(9.0 * a);
    double x = a * t * t * t;
    if (!std::isfinite(x) || x <= 0.0) x = 0.5 * a;  // fallback

    // Newton iterations on f(x) = gammainc(x, a) - P. Density:
    //   f'(x) = x^{a-1} exp(-x) / Γ(a)
    // Compute log-density to avoid over/underflow.
    const double lgA = std::lgamma(a);
    for (int it = 0; it < 60; ++it) {
        const double f = gammaincScalar(x, a) - P;
        if (std::abs(f) < 1e-15) break;
        const double logf = (a - 1.0) * std::log(x) - x - lgA;
        const double df   = std::exp(logf);
        if (df < 1e-300) break;
        double dx = f / df;
        // Damp if step would overshoot beyond [0, ∞).
        if (dx >= x) dx = 0.5 * x;
        x -= dx;
        if (x <= 0.0) x = 1e-300;
        if (std::abs(dx) < 1e-13 * (std::abs(x) + 1.0)) break;
    }
    return x;
}

// Inverse regularized incomplete beta. Solves betainc(x, a, b) = P.
// Uses normal-approximation initial guess, then Newton with bisection
// fallback (since betainc can be very flat).
double betaincinvScalar(double P, double a, double b)
{
    if (std::isnan(P) || std::isnan(a) || std::isnan(b)) return std::nan("");
    if (a <= 0.0 || b <= 0.0)                            return std::nan("");
    if (P < 0.0 || P > 1.0)                              return std::nan("");
    if (P == 0.0) return 0.0;
    if (P == 1.0) return 1.0;

    // Initial guess: AS 26.5.22 — for symmetric a≈b uses normal; otherwise
    // a small/large-tail guess. Simple bracketing suffices.
    double lo = 0.0, hi = 1.0;
    double x = a / (a + b);  // mode-ish

    // Pre-compute log B(a,b) for the derivative.
    const double lbeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);

    for (int it = 0; it < 80; ++it) {
        const double f = betaincScalar(x, a, b) - P;
        if (std::abs(f) < 1e-15) break;

        // Update bracket.
        if (f > 0) hi = x;
        else        lo = x;

        // Density: f'(x) = x^{a-1} (1-x)^{b-1} / B(a,b). Use logs.
        const double logf = (a - 1.0) * std::log(x)
                          + (b - 1.0) * std::log1p(-x)
                          - lbeta;
        const double df   = std::exp(logf);
        double xNew;
        if (df > 1e-300) {
            xNew = x - f / df;
        } else {
            xNew = 0.5 * (lo + hi);
        }
        // If Newton step leaves bracket, bisect.
        if (!(xNew > lo && xNew < hi))
            xNew = 0.5 * (lo + hi);

        if (std::abs(xNew - x) < 1e-14 * (std::abs(x) + 1.0)) {
            x = xNew;
            break;
        }
        x = xNew;
    }
    return x;
}

// Jacobi elliptic functions (sn, cn, dn) at u with parameter m.
// Implementation: descending Landen via the AGM. Standard A&S 16.4.
// Returns 3 doubles by reference; m must satisfy 0 ≤ m ≤ 1.
void ellipjScalar(double u, double m, double &sn, double &cn, double &dn)
{
    if (std::isnan(u) || std::isnan(m) || m < 0.0 || m > 1.0) {
        sn = cn = dn = std::nan("");
        return;
    }
    if (m == 0.0) { sn = std::sin(u); cn = std::cos(u); dn = 1.0; return; }
    if (m == 1.0) {
        const double t = std::tanh(u);
        sn = t; cn = 1.0 / std::cosh(u); dn = cn;
        return;
    }

    // Landen sequence: a_n+1 = (a_n + b_n)/2, b_n+1 = sqrt(a_n*b_n),
    // c_n+1 = (a_n - b_n)/2. Stop when c_n is below tol or n maxed.
    constexpr int kMax = 16;
    double a[kMax + 1], c[kMax + 1];
    a[0] = 1.0;
    double b = std::sqrt(1.0 - m);
    c[0] = std::sqrt(m);
    int n = 0;
    for (int i = 1; i <= kMax; ++i) {
        const double an = 0.5 * (a[i - 1] + b);
        const double bn = std::sqrt(a[i - 1] * b);
        const double cn_= 0.5 * (a[i - 1] - b);
        a[i] = an;
        c[i] = cn_;
        b    = bn;
        n    = i;
        if (std::abs(cn_) < 1e-15 * std::abs(an)) break;
    }
    // Compute φ_n = 2^n * a_n * u, then unwind via
    //   φ_{i-1} = (1/2) (φ_i + asin((c_i / a_i) * sin(φ_i)))
    double phi = std::ldexp(a[n] * u, n);  // 2^n * a_n * u
    for (int i = n; i >= 1; --i) {
        phi = 0.5 * (phi + std::asin((c[i] / a[i]) * std::sin(phi)));
    }
    sn = std::sin(phi);
    cn = std::cos(phi);
    dn = std::sqrt(1.0 - m * sn * sn);
}

} // namespace

Value gammaincinv(const Value &P, const Value &a, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(P, a,
        [](double pp, double aa) { return gammaincinvScalar(pp, aa); }, mr);
}

Value betaincinv(const Value &P, const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    if (P.isScalar() && a.isScalar() && b.isScalar()) {
        return Value::scalar(
            betaincinvScalar(P.toScalar(), a.toScalar(), b.toScalar()), mr);
    }
    const size_t np = P.numel(), na = a.numel(), nb = b.numel();
    const size_t n = std::max({np, na, nb});
    auto pickShape = [&]() -> const Value & {
        if (np == n) return P;
        if (na == n) return a;
        return b;
    };
    auto r = createLike(pickShape(), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < n; ++i) {
        const double pi = (np == 1) ? P.toScalar() : P.doubleData()[i];
        const double ai = (na == 1) ? a.toScalar() : a.doubleData()[i];
        const double bi = (nb == 1) ? b.toScalar() : b.doubleData()[i];
        r.doubleDataMut()[i] = betaincinvScalar(pi, ai, bi);
    }
    return r;
}

EllipJ ellipj(const Value &u, const Value &m, std::pmr::memory_resource *mr)
{
    if (u.isScalar() && m.isScalar()) {
        double sn, cn, dn;
        ellipjScalar(u.toScalar(), m.toScalar(), sn, cn, dn);
        return { Value::scalar(sn, mr), Value::scalar(cn, mr), Value::scalar(dn, mr) };
    }
    const size_t nu = u.numel(), nm = m.numel();
    const size_t n  = std::max(nu, nm);
    const Value &shape = (nu == n) ? u : m;
    auto sn = createLike(shape, ValueType::DOUBLE, mr);
    auto cn = createLike(shape, ValueType::DOUBLE, mr);
    auto dn = createLike(shape, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < n; ++i) {
        const double ui = (nu == 1) ? u.toScalar() : u.doubleData()[i];
        const double mi = (nm == 1) ? m.toScalar() : m.doubleData()[i];
        double s, c, d;
        ellipjScalar(ui, mi, s, c, d);
        sn.doubleDataMut()[i] = s;
        cn.doubleDataMut()[i] = c;
        dn.doubleDataMut()[i] = d;
    }
    return { std::move(sn), std::move(cn), std::move(dn) };
}

} // namespace numkit::math
