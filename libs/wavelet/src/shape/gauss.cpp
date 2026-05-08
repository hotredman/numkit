// libs/wavelet/src/shape/gauss.cpp
//
// Real and complex Gaussian wavelets (gauswavf / cgauwavf).
//
// Real form (gauswavf, p ∈ ℕ⁺, default 1):
//   ψ_p(t) = sgn_p · |α_p| · H_p(t) · exp(-t²)
//   |α_p| = 1 / sqrt((2p-1)!! · sqrt(π/2))                  (analytical L²)
//   sgn_p = (-1)^ceil(p/2)                                  (alternates in pairs:
//                                                            -, -, +, +, -, -, +, +, …)
//   H_p   = physicist's Hermite polynomial
//
// Complex form (cgauwavf, p ∈ ℕ⁺, default 1):
//   ψ_p(t) = (-1)^p · H_p(t + i/2) · exp(-t²) · exp(-i·t) / sqrt(N²)
//   N²     = trapezoidal integral of |...|² over [LB, UB] on the same grid
//
// Verified vs MATLAB R2025b for [LB, UB, N] = [-5, 5, 11], p = 1..8 (real)
// and p = 1, 2 (complex). The complex form's normalization is grid-dependent
// (trapezoidal on [LB, UB]) — gauswavf's is not. Same convention as MATLAB.

#include <numkit/wavelet/shape/shape.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cctype>
#include <cmath>
#include <complex>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::wavelet {

namespace {

Value linspace_row(std::pmr::memory_resource *mr, double lb, double ub, size_t N)
{
    Value xv = Value::matrix(1, N, ValueType::DOUBLE, mr);
    if (N == 0) return xv;
    double *xd = xv.doubleDataMut();
    if (N == 1) { xd[0] = ub; return xv; }
    const double step = (ub - lb) / static_cast<double>(N - 1);
    for (size_t i = 0; i < N; ++i)
        xd[i] = lb + step * static_cast<double>(i);
    xd[N - 1] = ub;
    return xv;
}

// Physicist's Hermite polynomial H_p evaluated at real t via the
// recurrence H_{n+1}(t) = 2 t H_n(t) - 2 n H_{n-1}(t).
double hermite_real(int p, double t)
{
    if (p == 0) return 1.0;
    if (p == 1) return 2.0 * t;
    double Hm = 1.0, Hc = 2.0 * t;
    for (int n = 1; n < p; ++n) {
        const double Hn = 2.0 * t * Hc - 2.0 * static_cast<double>(n) * Hm;
        Hm = Hc;
        Hc = Hn;
    }
    return Hc;
}

Complex hermite_complex(int p, Complex z)
{
    if (p == 0) return Complex(1.0, 0.0);
    if (p == 1) return 2.0 * z;
    Complex Hm(1.0, 0.0), Hc = 2.0 * z;
    for (int n = 1; n < p; ++n) {
        const Complex Hn = 2.0 * z * Hc - 2.0 * static_cast<double>(n) * Hm;
        Hm = Hc;
        Hc = Hn;
    }
    return Hc;
}

// (2p-1)!! · sqrt(π/2): analytical norm² of H_p(t)·exp(-t²) on (-∞, ∞).
double dblfact_norm_sq(int p)
{
    double r = std::sqrt(M_PI / 2.0);
    for (int k = 1; k <= p; ++k)
        r *= static_cast<double>(2 * k - 1);
    return r;
}

} // anonymous

std::tuple<Value, Value>
gauswavf(std::pmr::memory_resource *mr, double lb, double ub, size_t N, int p)
{
    Value xv = linspace_row(mr, lb, ub, N);
    Value pv = Value::matrix(1, N, ValueType::DOUBLE, mr);
    if (N == 0) return {std::move(pv), std::move(xv)};
    if (p < 1)
        throw Error("gauswavf: derivative order p must be ≥ 1",
                    0, 0, "gauswavf", "", "m:gauswavf:p");

    const double *xd = xv.doubleData();
    double *pd = pv.doubleDataMut();
    const double alpha_mag = 1.0 / std::sqrt(dblfact_norm_sq(p));
    const int sgn = (((p + 1) / 2) % 2 == 0) ? +1 : -1;
    const double scale = static_cast<double>(sgn) * alpha_mag;

    for (size_t i = 0; i < N; ++i) {
        const double t = xd[i];
        const double Hp = hermite_real(p, t);
        pd[i] = scale * Hp * std::exp(-t * t);
    }
    return {std::move(pv), std::move(xv)};
}

std::tuple<Value, Value>
cgauwavf(std::pmr::memory_resource *mr, double lb, double ub, size_t N, int p)
{
    Value xv = linspace_row(mr, lb, ub, N);
    Value pv = Value::matrix(1, N, ValueType::COMPLEX, mr);
    if (N == 0) return {std::move(pv), std::move(xv)};
    if (p < 1)
        throw Error("cgauwavf: derivative order p must be ≥ 1",
                    0, 0, "cgauwavf", "", "m:cgauwavf:p");

    const double *xd = xv.doubleData();
    Complex *pd = pv.complexDataMut();
    const double sgn_p = (p % 2 == 0) ? 1.0 : -1.0;

    // Stage 1 — raw f_p(t) into the complex output buffer.
    std::vector<double> mag2(N);
    for (size_t i = 0; i < N; ++i) {
        const double t = xd[i];
        const Complex z(t, 0.5);
        const Complex Hp = hermite_complex(p, z);
        // exp(-t² - i·t) = exp(-t²) · (cos(t) - i·sin(t))
        const double e = std::exp(-t * t);
        const Complex phase(std::cos(t), -std::sin(t));
        const Complex f = sgn_p * Hp * (e * phase);
        pd[i] = f;
        mag2[i] = std::norm(f);
    }

    // Stage 2 — trapezoidal L² norm² over [lb, ub] on the same grid.
    double norm_sq = 0.0;
    if (N >= 2) {
        const double h = (ub - lb) / static_cast<double>(N - 1);
        for (size_t i = 0; i < N; ++i) {
            const double w = (i == 0 || i == N - 1) ? 0.5 : 1.0;
            norm_sq += w * mag2[i];
        }
        norm_sq *= h;
    } else {
        norm_sq = mag2[0];
    }
    const double inv_n = (norm_sq > 0.0) ? 1.0 / std::sqrt(norm_sq) : 0.0;
    for (size_t i = 0; i < N; ++i) pd[i] *= inv_n;

    return {std::move(pv), std::move(xv)};
}

namespace detail {

// Parse the optional `p` argument: either an integer (1..8) or a string
// of the form `'gausN'` (real form) / `'cgauN'` (complex form). The
// `prefix` is the expected wname stem.
static int parseGaussOrder(const Value &arg, const char *prefix, const char *fn)
{
    if (arg.isChar() || arg.isString()) {
        std::string s = arg.toString();
        // Lowercase the prefix portion for comparison.
        for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const size_t plen = std::char_traits<char>::length(prefix);
        if (s.size() <= plen || s.compare(0, plen, prefix) != 0)
            throw Error(std::string(fn) + ": bad wname '" + s +
                        "' (expected " + prefix + "N)",
                         0, 0, fn, "", "m:wname");
        try { return std::stoi(s.substr(plen)); }
        catch (...) {
            throw Error(std::string(fn) + ": bad wname '" + s +
                        "' (cannot parse order N)",
                         0, 0, fn, "", "m:wname");
        }
    }
    return static_cast<int>(arg.toScalar());
}

void gauswavf_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("gauswavf: requires (LB, UB, N[, p|'gausN'])",
                    0, 0, "gauswavf", "", "m:gauswavf:nargin");
    const double lb = args[0].toScalar();
    const double ub = args[1].toScalar();
    const size_t N  = static_cast<size_t>(args[2].toScalar());
    int p = 1;
    if (args.size() >= 4) p = parseGaussOrder(args[3], "gaus", "gauswavf");
    auto [psi, x] = gauswavf(ctx.engine->resource(), lb, ub, N, p);
    outs[0] = std::move(psi);
    if (nargout > 1) outs[1] = std::move(x);
}

void cgauwavf_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("cgauwavf: requires (LB, UB, N[, p|'cgauN'])",
                    0, 0, "cgauwavf", "", "m:cgauwavf:nargin");
    const double lb = args[0].toScalar();
    const double ub = args[1].toScalar();
    const size_t N  = static_cast<size_t>(args[2].toScalar());
    int p = 1;
    if (args.size() >= 4) p = parseGaussOrder(args[3], "cgau", "cgauwavf");
    auto [psi, x] = cgauwavf(ctx.engine->resource(), lb, ub, N, p);
    outs[0] = std::move(psi);
    if (nargout > 1) outs[1] = std::move(x);
}

} // namespace detail
} // namespace numkit::wavelet
