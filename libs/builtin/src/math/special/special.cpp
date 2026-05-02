// libs/builtin/src/math/elementary/special.cpp
//
// Special functions — gamma / gammaln / erf / erfc / erfinv.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/math/special/special.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>
#include <limits>

namespace numkit::builtin {

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

Value gammaFn(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryDouble(x, [](double v) { return std::tgamma(v); }, mr);
}

Value gammaln(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryDouble(x, [](double v) { return std::lgamma(v); }, mr);
}

Value erf(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryDouble(x, [](double v) { return std::erf(v); }, mr);
}

Value erfc(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryDouble(x, [](double v) { return std::erfc(v); }, mr);
}

Value erfinv(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryDouble(x, [](double v) { return erfinvScalar(v); }, mr);
}

// ── Pack 19: beta / betaln / expint / psi ────────────────────────────

Value beta(std::pmr::memory_resource *mr, const Value &z, const Value &w)
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

Value betaln(std::pmr::memory_resource *mr, const Value &z, const Value &w)
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
    // Pole at non-positive integers.
    if (x == std::floor(x) && x <= 0.0)
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

Value expint(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryDouble(x, [](double v) { return expintScalar(v); }, mr);
}

Value psi(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryDouble(x, [](double v) { return psiScalar(v); }, mr);
}

// ── Engine adapters ──────────────────────────────────────────────────
namespace detail {

#define NK_UNARY_ADAPTER(name, fn)                                              \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "m:" #name ":nargin");                 \
        outs[0] = fn(ctx.engine->resource(), args[0]);                          \
    }

NK_UNARY_ADAPTER(gamma,   gammaFn)
NK_UNARY_ADAPTER(gammaln, gammaln)
NK_UNARY_ADAPTER(erf,     erf)
NK_UNARY_ADAPTER(erfc,    erfc)
NK_UNARY_ADAPTER(erfinv,  erfinv)
NK_UNARY_ADAPTER(expint,  expint)
NK_UNARY_ADAPTER(psi,     psi)

#undef NK_UNARY_ADAPTER

void beta_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("beta: requires (Z, W)",
                     0, 0, "beta", "", "m:beta:nargin");
    outs[0] = beta(ctx.engine->resource(), args[0], args[1]);
}

void betaln_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("betaln: requires (Z, W)",
                     0, 0, "betaln", "", "m:betaln:nargin");
    outs[0] = betaln(ctx.engine->resource(), args[0], args[1]);
}

} // namespace detail

} // namespace numkit::builtin
