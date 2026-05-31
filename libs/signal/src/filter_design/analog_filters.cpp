// libs/signal/src/filter_design/analog_filters.cpp
//
// Analog prototypes + frequency transforms + bilinear / impinvar +
// freqs. Top-level cheby1 / cheby2 / ellip / besself functions
// compose these.

#include <numkit/signal/filter_design/analog_filters.hpp>

#include <numkit/builtin/math/poly/polynomials.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/signal/filter_implementation/conversions_extras.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>
#include <limits>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

using Cd = std::complex<double>;

std::vector<Cd> readComplexVec(const Value &v)
{
    const size_t n = v.numel();
    std::vector<Cd> out(n);
    if (v.type() == ValueType::COMPLEX) {
        const Cd *cd = v.complexData();
        for (size_t i = 0; i < n; ++i) out[i] = cd[i];
    } else {
        for (size_t i = 0; i < n; ++i) out[i] = Cd(v.elemAsDouble(i), 0.0);
    }
    return out;
}

std::vector<double> readVec(const Value &v)
{
    const size_t n = v.numel();
    std::vector<double> out(n);
    for (size_t i = 0; i < n; ++i) out[i] = v.elemAsDouble(i);
    return out;
}

Value packComplexCol(const std::vector<Cd> &v, std::pmr::memory_resource *mr)
{
    auto out = Value::complexMatrix(v.size(), 1, mr);
    if (v.empty()) return out;
    Cd *cd = out.complexDataMut();
    std::memcpy(cd, v.data(), v.size() * sizeof(Cd));
    return out;
}

Value packDoubleRow(const std::vector<double> &v, std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::memcpy(out.doubleDataMut(), v.data(), v.size() * sizeof(double));
    return out;
}

} // anonymous

// ════════════════════════════════════════════════════════════════════
// Analog prototypes
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value>
buttap(int N, std::pmr::memory_resource *mr)
{
    // Butterworth poles: equally-spaced on the left half of the unit
    // circle. p_k = exp(j π (2k - 1) / (2N) + j π/2) for k = 1..N.
    std::vector<Cd> z;  // no zeros
    std::vector<Cd> p(N);
    for (int k = 1; k <= N; ++k) {
        const double theta = M_PI * (2.0 * k - 1.0) / (2.0 * N) + 0.5 * M_PI;
        p[k - 1] = std::exp(Cd(0.0, theta));
    }
    return std::make_tuple(packComplexCol(z, mr),
                           packComplexCol(p, mr),
                           Value::scalar(1.0, mr));
}

std::tuple<Value, Value, Value>
cheb1ap(int N, double Rp, std::pmr::memory_resource *mr)
{
    // Chebyshev type I prototype. Passband ripple Rp dB.
    // ε² = 10^(Rp/10) - 1.
    // Poles: pₖ = -sinh(μ) sin(θₖ) + j cosh(μ) cos(θₖ),
    //   where μ = asinh(1/ε)/N, θₖ = π(2k-1)/(2N).
    const double eps = std::sqrt(std::pow(10.0, Rp / 10.0) - 1.0);
    const double mu = std::asinh(1.0 / eps) / static_cast<double>(N);
    std::vector<Cd> z;
    std::vector<Cd> p(N);
    for (int k = 1; k <= N; ++k) {
        const double theta = M_PI * (2.0 * k - 1.0) / (2.0 * N);
        p[k - 1] = Cd(-std::sinh(mu) * std::sin(theta),
                      std::cosh(mu) * std::cos(theta));
    }
    // Gain: prod(-poles) / sqrt(1+ε²)  [for odd N: prod(-poles)]
    Cd kc(1.0, 0.0);
    for (auto &pp : p) kc *= -pp;
    double k = kc.real();
    if ((N % 2) == 0) k /= std::sqrt(1.0 + eps * eps);
    return std::make_tuple(packComplexCol(z, mr),
                           packComplexCol(p, mr),
                           Value::scalar(k, mr));
}

// ════════════════════════════════════════════════════════════════════
// Elliptic (Cauer) prototype helpers — local scalar primitives so we
// don't pull libs/builtin internals into libs/signal.
// ════════════════════════════════════════════════════════════════════

namespace ellip_detail {

// Complete elliptic integral K(m) via arithmetic-geometric mean.
// m is the parameter (m = k², 0 <= m <= 1).
inline double ellipKm(double m)
{
    if (m >= 1.0) return std::numeric_limits<double>::infinity();
    if (m <= 0.0) return M_PI / 2.0;
    double a = 1.0;
    double b = std::sqrt(1.0 - m);
    for (int i = 0; i < 64; ++i) {
        const double an = 0.5 * (a + b);
        const double bn = std::sqrt(a * b);
        if (std::abs(an - bn) < 1e-17 * an) { a = an; break; }
        a = an; b = bn;
    }
    return M_PI / (2.0 * a);
}

// Jacobi sn/cn/dn at REAL u with REAL modulus k (m = k²).
struct SnCnDn { double sn, cn, dn; };
inline SnCnDn ellipjReal(double u, double k)
{
    const double m = k * k;
    if (m == 0.0) return { std::sin(u), std::cos(u), 1.0 };
    if (m == 1.0) {
        const double t  = std::tanh(u);
        const double s  = 1.0 / std::cosh(u);
        return { t, s, s };
    }
    constexpr int kMax = 16;
    double a[kMax + 1], c[kMax + 1];
    a[0] = 1.0;
    double b = std::sqrt(1.0 - m);
    c[0] = std::sqrt(m);
    int n = 0;
    for (int i = 1; i <= kMax; ++i) {
        const double an  = 0.5 * (a[i - 1] + b);
        const double bn  = std::sqrt(a[i - 1] * b);
        const double cn_ = 0.5 * (a[i - 1] - b);
        a[i] = an;
        c[i] = cn_;
        b    = bn;
        n    = i;
        if (std::abs(cn_) < 1e-15 * std::abs(an)) break;
    }
    double phi = std::ldexp(a[n] * u, n);
    for (int i = n; i >= 1; --i) {
        phi = 0.5 * (phi + std::asin((c[i] / a[i]) * std::sin(phi)));
    }
    const double sn = std::sin(phi);
    return { sn, std::cos(phi), std::sqrt(1.0 - m * sn * sn) };
}

// Inverse Jacobi sn at real argument: solve sn(u, k) = z for u in
// absolute units (range [0, K(k)] for z in [0, 1]). Bisection -- robust
// even when k is very close to 1 (where Newton diverges because sn
// saturates at ±1 and cn*dn -> 0).
inline double asnReal(double z, double k)
{
    if (z == 0.0) return 0.0;
    const double sign = (z < 0.0) ? -1.0 : 1.0;
    z = std::abs(z);
    if (z >= 1.0) {
        // sn = 1 at u = K(k); for z > 1 saturate there.
        return sign * ellipKm(k * k);
    }
    // Special-case k = 1: sn(u, 1) = tanh(u), so asn = atanh.
    if (k >= 1.0 - 1e-12) {
        return sign * std::atanh(z);
    }
    const double Kval = ellipKm(k * k);
    double lo = 0.0, hi = Kval;
    for (int it = 0; it < 200; ++it) {
        const double mid = 0.5 * (lo + hi);
        auto j = ellipjReal(mid, k);
        if (j.sn < z) lo = mid;
        else          hi = mid;
        if ((hi - lo) < 1e-15 * (1.0 + Kval)) break;
    }
    return sign * 0.5 * (lo + hi);
}

// Solve degree equation for k given (N, k1):
//   K(k')/K(k) = (1/N) * K(k1')/K(k1)
// K(k')/K(k) is monotonically DECREASING in k on (0, 1), so bisect.
inline double ellipdeg(int N, double k1)
{
    const double K1  = ellipKm(k1 * k1);
    const double K1p = ellipKm(1.0 - k1 * k1);
    const double target = K1p / K1 / static_cast<double>(N);
    double lo = 1e-15, hi = 1.0 - 1e-15;
    for (int it = 0; it < 200; ++it) {
        const double mid = 0.5 * (lo + hi);
        const double Kmid  = ellipKm(mid * mid);
        const double Kpmid = ellipKm(1.0 - mid * mid);
        const double ratio = Kpmid / Kmid;
        if (ratio > target) lo = mid;
        else                hi = mid;
        if (hi - lo < 1e-15 * (1.0 + std::abs(mid))) break;
    }
    return 0.5 * (lo + hi);
}

// Jacobi cd(u, k) = cn(u, k) / dn(u, k) at COMPLEX u, real modulus k.
// Uses addition formula via real-arg sn/cn/dn at u.real() with k and at
// u.imag() with complementary modulus k' = sqrt(1 - k^2).
//   sn(x+jy, k) = (sx*dy + j*cx*dx*sy*cy) / D
//   cn(x+jy, k) = (cx*cy - j*sx*dx*sy*dy) / D
//   dn(x+jy, k) = (dx*cy*dy - j*k^2*sx*cx*sy) / D
//   D = cy^2 + k^2*sx^2*sy^2
// where (sx,cx,dx) = ellipjReal(x, k) and (sy,cy,dy) = ellipjReal(y, k').
inline Cd cdComplex(Cd uc, double k)
{
    const double x = uc.real();
    const double y = uc.imag();
    const double kp = std::sqrt(1.0 - k * k);
    const auto X = ellipjReal(x, k);
    const auto Y = ellipjReal(y, kp);
    const double D = Y.cn * Y.cn + k * k * X.sn * X.sn * Y.sn * Y.sn;
    if (std::abs(D) < 1e-300) return Cd(0.0, 0.0);
    Cd cn(  X.cn * Y.cn / D,                 -X.sn * X.dn * Y.sn * Y.dn / D);
    Cd dn(  X.dn * Y.cn * Y.dn / D,          -k * k * X.sn * X.cn * Y.sn / D);
    return cn / dn;
}

} // namespace ellip_detail

// ── ellipap ───────────────────────────────────────────────────────────
// Cauer (elliptic) analog prototype. Order N, passband ripple Rp dB,
// stopband attenuation Rs dB. Returns (z, p, k_gain) for normalized
// cutoff = 1 rad/s.
//
// Algorithm: Sophocleous / Orfanidis / Antoniou.
//   1. epsilon = sqrt(10^(Rp/10) - 1)              passband ripple coeff
//   2. k1      = epsilon / sqrt(10^(Rs/10) - 1)    selectivity factor
//   3. k       = ellipdeg(N, k1)                   modulus from degree eq
//   4. v0      = asn(1/sqrt(1+eps^2), k1') / N     real (via imag-arg ident.)
//   5. K       = K(k)                              quarter-period
//   6. For i=1..L (L=N/2): u_i = (2i-1)/N
//        zeta_i = cd(u_i*K, k)
//        zero_i = j / (k * zeta_i)                 imag pair
//        pole_i = j * cd(u_i*K - j*v0, k)          complex pair
//      For odd N: extra real pole at -sn(v0, k')/cn(v0, k')
//   7. gain = real(prod(-poles) / prod(-zeros)),
//      divide by sqrt(1+eps^2) when N is even.
std::tuple<Value, Value, Value>
ellipap(int N, double Rp, double Rs, std::pmr::memory_resource *mr)
{
    if (N <= 0)
        return std::make_tuple(packComplexCol(std::vector<Cd>{}, mr),
                               packComplexCol(std::vector<Cd>{}, mr),
                               Value::scalar(std::pow(10.0, -Rp / 20.0), mr));
    if (Rp <= 0.0 || Rs <= 0.0)
        return std::make_tuple(packComplexCol(std::vector<Cd>{}, mr),
                               packComplexCol(std::vector<Cd>{}, mr),
                               Value::scalar(1.0, mr));

    const double eps2  = std::pow(10.0, Rp / 10.0) - 1.0;
    const double eps   = std::sqrt(eps2);
    const double Astop = std::pow(10.0, Rs / 10.0) - 1.0;
    const double k1    = eps / std::sqrt(Astop);
    const double k     = ellip_detail::ellipdeg(N, k1);
    const double K     = ellip_detail::ellipKm(k * k);
    const double k1p   = std::sqrt(1.0 - k1 * k1);
    // v0: solve sn(j*v_abs, k1) = j/eps  via imaginary-arg identity:
    //   sn(j*v, k1) = j * sn(v, k1') / cn(v, k1')
    // setting that = j/eps gives sn(v, k1') = 1/sqrt(1+eps^2).
    // v0 in absolute K(k)-units. The MATLAB/Octave reference works in
    // [0, 1] units relative to each modulus's K, so it computes
    //   v0_norm = asn(arg, k1') / N / K(k1)
    // Multiplying by K(k) brings v0 to absolute K(k)-units, which is
    // what the pole formulas below expect.
    const double K1 = ellip_detail::ellipKm(k1 * k1);
    const double v0 =
        ellip_detail::asnReal(1.0 / std::sqrt(1.0 + eps2), k1p)
        * K / (static_cast<double>(N) * K1);

    const int L = N / 2;
    std::vector<Cd> zeros, poles;
    zeros.reserve(2 * L);
    poles.reserve(N);

    for (int i = 1; i <= L; ++i) {
        const double ui_abs = (2.0 * i - 1.0) / N * K;
        const auto Z = ellip_detail::ellipjReal(ui_abs, k);
        const double zeta = Z.cn / Z.dn;       // cd(ui_abs, k)
        const double zim = 1.0 / (k * zeta);   // 1/(k*cd)
        zeros.emplace_back(0.0,  zim);
        zeros.emplace_back(0.0, -zim);

        const Cd cd_p = ellip_detail::cdComplex(Cd(ui_abs, -v0), k);
        const Cd pole = Cd(0.0, 1.0) * cd_p;   // j * cd
        poles.push_back(pole);
        poles.emplace_back(std::conj(pole));
    }
    if ((N % 2) == 1) {
        // Odd N: real pole = j * sn(j*v0, k) = j * j * sn(v0, k')/cn(v0, k')
        //       = -sn(v0, k')/cn(v0, k')   (negative real)
        const double kp = std::sqrt(1.0 - k * k);
        const auto V = ellip_detail::ellipjReal(v0, kp);
        const double real_pole = -V.sn / V.cn;
        poles.emplace_back(real_pole, 0.0);
    }

    // Gain to normalize: real(prod(-p) / prod(-z)). For even N divide
    // by sqrt(1+eps^2) so the passband peaks at 1 (the equiripple
    // pattern for even-order elliptic sits at 1/sqrt(1+eps^2) at DC).
    Cd num(1.0, 0.0), den(1.0, 0.0);
    for (auto &p : poles) num *= -p;
    for (auto &z : zeros) den *= -z;
    double gain = (std::abs(den) > 0) ? (num / den).real() : num.real();
    if ((N % 2) == 0) gain /= std::sqrt(1.0 + eps2);

    return std::make_tuple(packComplexCol(zeros, mr),
                           packComplexCol(poles, mr),
                           Value::scalar(gain, mr));
}

std::tuple<Value, Value, Value>
cheb2ap(int N, double Rs, std::pmr::memory_resource *mr)
{
    // Chebyshev type II ("inverse"): zeros on imaginary axis.
    // Stopband attenuation Rs dB.
    // δ = 1/√(10^(Rs/10) - 1)
    // μ = asinh(1/δ)/N
    // Type-I poles (then inverted to type-II), zeros on imag axis.
    const double delta = 1.0 / std::sqrt(std::pow(10.0, Rs / 10.0) - 1.0);
    const double mu = std::asinh(1.0 / delta) / static_cast<double>(N);

    std::vector<Cd> p(N), z;
    for (int k = 1; k <= N; ++k) {
        const double theta = M_PI * (2.0 * k - 1.0) / (2.0 * N);
        // Type-I pole, then invert.
        const Cd p1(-std::sinh(mu) * std::sin(theta),
                     std::cosh(mu) * std::cos(theta));
        p[k - 1] = 1.0 / p1;
    }
    // Zeros at j / cos((2k-1)π/(2N)) for k=1..N (excluding θ=π/2 which
    // corresponds to a zero at infinity — drops out for odd N at the
    // centre k = (N+1)/2).
    for (int k = 1; k <= N; ++k) {
        const double theta = M_PI * (2.0 * k - 1.0) / (2.0 * N);
        const double c = std::cos(theta);
        if (std::abs(c) < 1e-12) continue;
        z.push_back(Cd(0.0, 1.0 / c));
    }
    // Gain: real(prod(-poles) / prod(-zeros)) — preserves the DC value
    // of the analog prototype.
    Cd num(1.0, 0.0), den(1.0, 0.0);
    for (auto &pp : p) num *= -pp;
    for (auto &zz : z) den *= -zz;
    const double k = (std::abs(den) > 0)
                     ? (num / den).real() : num.real();
    return std::make_tuple(packComplexCol(z, mr),
                           packComplexCol(p, mr),
                           Value::scalar(k, mr));
}

std::tuple<Value, Value, Value>
besselap(int N, std::pmr::memory_resource *mr)
{
    // For modest N (≤ 25, MATLAB's documented support) we use the
    // hard-coded Bessel-polynomial-pole tables. This file ships only
    // up to N = 8; higher orders just zero-fill (rare in practice).
    static const std::vector<std::vector<Cd>> tables = {
        {},                                                                  // N=0
        { Cd(-1.0, 0.0) },                                                   // 1
        { Cd(-0.866025403784438, 0.5),                                       // 2
          Cd(-0.866025403784438,-0.5) },
        { Cd(-0.745640385848077, 0.711366624972835),                         // 3
          Cd(-0.745640385848077,-0.711366624972835),
          Cd(-0.941600026533207, 0.0) },
        { Cd(-0.657211171671882, 0.830161435843259),                         // 4
          Cd(-0.657211171671882,-0.830161435843259),
          Cd(-0.904758796788245, 0.270918733003839),
          Cd(-0.904758796788245,-0.270918733003839) },
        { Cd(-0.590575944611919, 0.907206756409664),                         // 5
          Cd(-0.590575944611919,-0.907206756409664),
          Cd(-0.851553619368782, 0.442717463809432),
          Cd(-0.851553619368782,-0.442717463809432),
          Cd(-0.926174717377150, 0.0) },
        { Cd(-0.538552681578156, 0.961864022747488),                         // 6
          Cd(-0.538552681578156,-0.961864022747488),
          Cd(-0.799654185832948, 0.562171734879845),
          Cd(-0.799654185832948,-0.562171734879845),
          Cd(-0.909390683404036, 0.185696439038819),
          Cd(-0.909390683404036,-0.185696439038819) },
        { Cd(-0.496691725667232, 1.002508508454420),                         // 7
          Cd(-0.496691725667232,-1.002508508454420),
          Cd(-0.752735543409167, 0.650469630987972),
          Cd(-0.752735543409167,-0.650469630987972),
          Cd(-0.880253434201683, 0.321665276230762),
          Cd(-0.880253434201683,-0.321665276230762),
          Cd(-0.919487155649029, 0.0) },
        { Cd(-0.462174041253212, 1.034388681126410),                         // 8
          Cd(-0.462174041253212,-1.034388681126410),
          Cd(-0.711138180848463, 0.718651731365058),
          Cd(-0.711138180848463,-0.718651731365058),
          Cd(-0.847396946514931, 0.425901753827867),
          Cd(-0.847396946514931,-0.425901753827867),
          Cd(-0.909286970144471, 0.141238678192833),
          Cd(-0.909286970144471,-0.141238678192833) },
    };
    std::vector<Cd> p;
    if (N >= 0 && N < static_cast<int>(tables.size())) p = tables[N];
    std::vector<Cd> z;
    return std::make_tuple(packComplexCol(z, mr),
                           packComplexCol(p, mr),
                           Value::scalar(1.0, mr));
}

// ════════════════════════════════════════════════════════════════════
// Lowpass → X transformations
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value, Value>
lp2lp(const Value &z, const Value &p, double k, double Wo, std::pmr::memory_resource *mr)
{
    // s → s/Wo: poles and zeros scale by Wo; gain scales by Wo^(N-M).
    auto zv = readComplexVec(z);
    auto pv = readComplexVec(p);
    const int M = static_cast<int>(zv.size());
    const int N = static_cast<int>(pv.size());
    for (auto &zz : zv) zz *= Wo;
    for (auto &pp : pv) pp *= Wo;
    const double kn = k * std::pow(Wo, N - M);
    return std::make_tuple(packComplexCol(zv, mr),
                           packComplexCol(pv, mr),
                           Value::scalar(kn, mr));
}

std::tuple<Value, Value, Value>
lp2hp(const Value &z, const Value &p, double k, double Wo, std::pmr::memory_resource *mr)
{
    // s → Wo/s: each zero/pole becomes Wo/old. Add (N-M) zeros at 0.
    auto zv = readComplexVec(z);
    auto pv = readComplexVec(p);
    const int M = static_cast<int>(zv.size());
    const int N = static_cast<int>(pv.size());
    Cd Pz(1.0, 0.0), Pp(1.0, 0.0);
    for (auto &zz : zv) { Pz *= -zz; zz = Wo / zz; }
    for (auto &pp : pv) { Pp *= -pp; pp = Wo / pp; }
    // Gain: k * prod(-z) / prod(-p)  (these are the constant terms of
    // the original prototype; flips sign when we substitute Wo/s).
    const double kn = k * (Pz / Pp).real();
    // Add zeros at origin to balance the pole count.
    for (int i = 0; i < N - M; ++i) zv.push_back(Cd(0.0, 0.0));
    return std::make_tuple(packComplexCol(zv, mr),
                           packComplexCol(pv, mr),
                           Value::scalar(kn, mr));
}

std::tuple<Value, Value, Value>
lp2bp(const Value &z, const Value &p, double k, double Wo, double Bw, std::pmr::memory_resource *mr)
{
    // s → (s² + Wo²) / (Bw·s). Each prototype zero/pole maps to TWO
    // new zeros/poles, plus M new zeros at origin.
    auto zv = readComplexVec(z);
    auto pv = readComplexVec(p);
    const int M = static_cast<int>(zv.size());
    const int N = static_cast<int>(pv.size());

    auto mapPair = [&](const Cd &x) {
        // s² - (Bw·x)·s + Wo² = 0  →  roots:
        const Cd b = Bw * x;
        const Cd disc = std::sqrt(b * b - 4.0 * Wo * Wo);
        return std::make_pair((b + disc) / 2.0, (b - disc) / 2.0);
    };

    std::vector<Cd> nz, np;
    for (auto &zz : zv) { auto [r1, r2] = mapPair(zz); nz.push_back(r1); nz.push_back(r2); }
    for (auto &pp : pv) { auto [r1, r2] = mapPair(pp); np.push_back(r1); np.push_back(r2); }
    // Add (N - M) zeros at origin.
    for (int i = 0; i < N - M; ++i) nz.push_back(Cd(0.0, 0.0));
    const double kn = k * std::pow(Bw, N - M);
    return std::make_tuple(packComplexCol(nz, mr),
                           packComplexCol(np, mr),
                           Value::scalar(kn, mr));
}

std::tuple<Value, Value, Value>
lp2bs(const Value &z, const Value &p, double k, double Wo, double Bw, std::pmr::memory_resource *mr)
{
    // s → Bw·s / (s² + Wo²). Each prototype root maps to two new
    // roots; M new zeros at ±jWo from finite-zero substitution.
    auto zv = readComplexVec(z);
    auto pv = readComplexVec(p);
    const int M = static_cast<int>(zv.size());
    const int N = static_cast<int>(pv.size());

    auto mapPair = [&](const Cd &x) {
        // x · s² - Bw·s + x·Wo² = 0  →  roots:
        const Cd a = x;
        const Cd b = -Bw;
        const Cd c = x * Wo * Wo;
        const Cd disc = std::sqrt(b * b - 4.0 * a * c);
        return std::make_pair((-b + disc) / (2.0 * a), (-b - disc) / (2.0 * a));
    };

    std::vector<Cd> nz, np;
    Cd Pz(1.0, 0.0), Pp(1.0, 0.0);
    for (auto &zz : zv) { Pz *= -zz; auto [r1, r2] = mapPair(zz); nz.push_back(r1); nz.push_back(r2); }
    for (auto &pp : pv) { Pp *= -pp; auto [r1, r2] = mapPair(pp); np.push_back(r1); np.push_back(r2); }
    // Add 2(N - M) zeros at ±jWo.
    for (int i = 0; i < N - M; ++i) {
        nz.push_back(Cd(0.0,  Wo));
        nz.push_back(Cd(0.0, -Wo));
    }
    const double kn = k * (Pz / Pp).real();
    return std::make_tuple(packComplexCol(nz, mr),
                           packComplexCol(np, mr),
                           Value::scalar(kn, mr));
}

// ════════════════════════════════════════════════════════════════════
// Bilinear transform
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value>
bilinear(const Value &b, const Value &a, double fs, double fp, std::pmr::memory_resource *mr)
{
    // Standard bilinear: substitute s ↔ 2·fs·(z-1)/(z+1) into b(s)/a(s).
    // With prewarp frequency fp, scale fs to preserve the response at fp.
    auto bv = readVec(b);
    auto av = readVec(a);
    const int Nb = static_cast<int>(bv.size()) - 1;
    const int Na = static_cast<int>(av.size()) - 1;
    if (Nb < 0 || Na < 0)
        return std::make_tuple(packDoubleRow({}, mr),
                               packDoubleRow({}, mr));

    double fsEff = fs;
    if (fp > 0.0) {
        // Prewarp: choose the bilinear scale so the analog response at fp
        // maps EXACTLY onto the digital response at fp. MATLAB's scale is
        //   K = 2π·fp / tan(π·fp/fs).
        // Since K is formed below as 2·fsEff, fsEff = π·fp / tan(π·fp/fs)
        //   = Wp / (2·tan(Wp/(2·fs))),  Wp = 2π·fp.
        // (The previous form omitted the /2, double-scaling K by 2×.)
        const double Wp = 2.0 * M_PI * fp;
        fsEff = Wp / (2.0 * std::tan(Wp / (2.0 * fs)));
    }
    const double K = 2.0 * fsEff;

    // Compose (z+1)^Na * b(K(z-1)/(z+1)) and (z+1)^Nb * a(K(z-1)/(z+1)),
    // then divide both by (z+1)^max so the digital coefficients have
    // matching length.
    const int M = std::max(Na, Nb);
    auto subst = [&](const std::vector<double> &c, int deg, int order) {
        // Build poly in z. deg = original degree of c.
        // (z+1)^order is multiplied out, then for each c[i] we add
        // c[i] * K^i * (z-1)^i * (z+1)^(order-i).
        std::vector<double> r(order + 1, 0.0);
        // Helper: polynomial power (z+a)^p where a = ±1.
        auto polyPow = [](double a, int p) {
            std::vector<double> r(p + 1, 0.0);
            r[0] = 1.0;
            for (int i = 0; i < p; ++i) {
                std::vector<double> nr(r.size() + 1, 0.0);
                for (size_t j = 0; j < r.size(); ++j) {
                    nr[j]     += r[j];
                    nr[j + 1] += a * r[j];
                }
                r = nr;
            }
            return r;
        };
        std::vector<double> Kpow(deg + 1);
        Kpow[0] = 1.0;
        for (int i = 1; i <= deg; ++i) Kpow[i] = Kpow[i - 1] * K;
        for (int i = 0; i <= deg; ++i) {
            const auto pa = polyPow(-1.0, i);          // (z-1)^i
            const auto pb = polyPow( 1.0, order - i);  // (z+1)^(order-i)
            // Convolve pa, pb, then add c[i]*Kpow[i] * conv to r.
            std::vector<double> conv(pa.size() + pb.size() - 1, 0.0);
            for (size_t j = 0; j < pa.size(); ++j)
                for (size_t k = 0; k < pb.size(); ++k)
                    conv[j + k] += pa[j] * pb[k];
            const double scale = c[deg - i] * Kpow[i];
            for (size_t j = 0; j < conv.size(); ++j) r[j] += scale * conv[j];
        }
        return r;
    };

    std::vector<double> Bd = subst(bv, Nb, M);
    std::vector<double> Ad = subst(av, Na, M);
    // Normalise so Ad[0] = 1 (matches MATLAB convention).
    const double a0 = Ad.empty() ? 1.0 : Ad[0];
    if (std::abs(a0) > 1e-300) {
        for (auto &x : Bd) x /= a0;
        for (auto &x : Ad) x /= a0;
    }
    return std::make_tuple(packDoubleRow(Bd, mr), packDoubleRow(Ad, mr));
}

// ════════════════════════════════════════════════════════════════════
// Impulse-invariance design (impinvar)
// ════════════════════════════════════════════════════════════════════
//
// For an analog filter b(s)/a(s) with N distinct simple poles pₖ, the
// partial-fraction expansion is b/a = Σ rₖ/(s - pₖ) with residues
// rₖ = b(pₖ)/a'(pₖ). Sampling the analog impulse response at rate fs
// gives the digital filter
//
//   H_d(z) = T · Σ rₖ / (1 - eᵖᵏᵀ · z⁻¹),    T = 1/fs
//
// which we re-combine into (b_d, a_d) form via:
//
//   a_d(z⁻¹) = ∏ (1 - αₖ · z⁻¹),                αₖ = eᵖᵏᵀ
//   b_d(z⁻¹) = T · Σ rₖ · ∏_{j≠k} (1 - αⱼ · z⁻¹)
//
// Imaginary parts in the final coefficients cancel to within roundoff
// when the analog filter is real (conjugate-pole pairing).

namespace {

// Horner evaluation of a real polynomial p (coefficients high → low) at
// a complex point.
inline Cd hornerReal(const std::vector<double> &p, Cd x) {
    Cd r(0.0, 0.0);
    for (auto c : p) r = r * x + Cd(c, 0.0);
    return r;
}

// poly(roots) for complex roots — returns coefficients high → low,
// length roots.size() + 1.
std::vector<Cd> polyFromRootsComplex(const std::vector<Cd> &roots) {
    std::vector<Cd> p = { Cd(1.0, 0.0) };
    for (auto rk : roots) {
        std::vector<Cd> np(p.size() + 1, Cd(0.0, 0.0));
        for (size_t i = 0; i < p.size(); ++i) {
            np[i]     += p[i];
            np[i + 1] -= rk * p[i];
        }
        p = std::move(np);
    }
    return p;
}

} // anonymous

std::tuple<Value, Value>
impinvar(const Value &b, const Value &a, double fs, double /*tol*/, std::pmr::memory_resource *mr)
{
    auto bv = readVec(b);
    auto av = readVec(a);
    if (av.size() < 2)
        throw std::runtime_error("impinvar: a must have degree ≥ 1");
    if (bv.size() >= av.size())
        throw std::runtime_error("impinvar: numerator degree must be less than denominator");
    const int N = static_cast<int>(av.size()) - 1;
    const double T = 1.0 / fs;

    // 1) Roots of a → analog poles.
    Value pV = ::numkit::builtin::roots(a, mr);
    auto pv = readComplexVec(pV);
    if (static_cast<int>(pv.size()) != N)
        throw std::runtime_error("impinvar: roots() returned unexpected count");

    // 2) Compute residues r_k = b(p_k) / a'(p_k).
    Value aprimeV = ::numkit::builtin::polyder(a, mr);
    auto apv = readVec(aprimeV);
    std::vector<Cd> r(N);
    for (int k = 0; k < N; ++k) {
        const Cd bp  = hornerReal(bv,  pv[k]);
        const Cd app = hornerReal(apv, pv[k]);
        r[k] = bp / app;
    }

    // 3) Digital poles α_k = exp(p_k · T).
    std::vector<Cd> alpha(N);
    for (int k = 0; k < N; ++k) alpha[k] = std::exp(pv[k] * T);

    // 4) a_d = poly(α) — built straight in complex; cast to double at end.
    std::vector<Cd> ad_c = polyFromRootsComplex(alpha);  // length N+1

    // 5) b_d = Σ r_k · T · ∏_{j≠k} (1 - α_j · q), polynomial in q = z⁻¹
    //    of degree N-1, length N. The ∏_{j≠k} (1 - α_j q) factor differs
    //    from polyFromRootsComplex which produces ∏ (q - α_j); we adjust
    //    by computing it on the fly.
    auto polyExceptK = [&](int k) {
        std::vector<Cd> p = { Cd(1.0, 0.0) };
        for (int j = 0; j < N; ++j) {
            if (j == k) continue;
            // multiply by (1 - α_j · q): (existing p) * (1, -α_j) in q-order
            std::vector<Cd> np(p.size() + 1, Cd(0.0, 0.0));
            for (size_t i = 0; i < p.size(); ++i) {
                np[i]     += p[i];                  // ·1
                np[i + 1] += -alpha[j] * p[i];      // · (-α_j · q)
            }
            p = std::move(np);
        }
        return p;
    };

    std::vector<Cd> bd_c(N, Cd(0.0, 0.0));
    for (int k = 0; k < N; ++k) {
        const std::vector<Cd> partial = polyExceptK(k);   // length N (in q-order)
        const Cd s = r[k] * T;
        for (int i = 0; i < N; ++i) bd_c[i] += s * partial[i];
    }

    // 6) Express a_d in q-form. polyFromRootsComplex produced the
    //    standard "high-to-low" polynomial coefficients of (z - α_k);
    //    converted to q = z⁻¹ form, ∏ (1 - α_k · q) has the SAME
    //    coefficients but ordered low-to-high. Concretely:
    //    ∏ (z - α_k) =     z^N      - Σα·z^(N-1) + ... + (-1)^N ∏α
    //    ∏ (1 - α_k q) =   1        - Σα·q       + ... + (-1)^N ∏α·q^N
    //    so the two are reverses of each other. We want MATLAB-style
    //    "high-z" first which equals the original ad_c (no reversal).
    std::vector<double> ad_d(N + 1);
    for (int i = 0; i <= N; ++i) ad_d[i] = ad_c[i].real();

    // bd_c is in q-form (low-to-high q). To match MATLAB row-vector
    // convention (high-to-low z, equivalently low-to-high z⁻¹ leading
    // with z^0 coefficient), keep low-to-high order for b — that puts
    // b[0] as the z^0 coefficient. Since the impulse-invariance design
    // is strictly proper (no z^0 term), b[0] should be ~0; the next
    // coefficients are the z⁻¹, z⁻², ... ones.
    std::vector<double> bd_d(N);
    for (int i = 0; i < N; ++i) bd_d[i] = bd_c[i].real();

    return std::make_tuple(packDoubleRow(bd_d, mr), packDoubleRow(ad_d, mr));
}

// ════════════════════════════════════════════════════════════════════
// Analog frequency response (freqs)
// ════════════════════════════════════════════════════════════════════

Value freqs(const Value &b, const Value &a, const Value &w, std::pmr::memory_resource *mr)
{
    // H(jw) = b(jw) / a(jw). Evaluate poly directly.
    auto bv = readVec(b);
    auto av = readVec(a);
    auto wv = readVec(w);
    const size_t M = wv.size();
    // MATLAB freqs returns a 1xM row vector (complex). Mirror that
    // shape; column-vector inputs for w still produce row outputs.
    auto out = Value::complexMatrix(1, M, mr);
    Cd *od = out.complexDataMut();
    for (size_t i = 0; i < M; ++i) {
        const Cd s(0.0, wv[i]);
        Cd num(0.0, 0.0), den(0.0, 0.0);
        for (size_t k = 0; k < bv.size(); ++k)
            num = num * s + Cd(bv[k], 0.0);
        for (size_t k = 0; k < av.size(); ++k)
            den = den * s + Cd(av[k], 0.0);
        od[i] = (std::abs(den) > 0) ? num / den : Cd(0.0, 0.0);
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

#define NK_PROTO0_REG(name)                                                     \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires N",                                    \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const int N = static_cast<int>(args[0].toScalar());                     \
        auto [z, p, k] = name(N, ctx.engine->resource());                       \
        outs[0] = std::move(z);                                                  \
        if (nargout > 1) outs[1] = std::move(p);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

#define NK_PROTO1_REG(name)                                                     \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.size() < 2)                                                     \
            throw Error(#name ": requires (N, ripple_or_atten_dB)",             \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const int N = static_cast<int>(args[0].toScalar());                     \
        const double r = args[1].toScalar();                                    \
        auto [z, p, k] = name(N, r, ctx.engine->resource());                    \
        outs[0] = std::move(z);                                                  \
        if (nargout > 1) outs[1] = std::move(p);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

NK_PROTO0_REG(buttap)
NK_PROTO0_REG(besselap)
NK_PROTO1_REG(cheb1ap)
NK_PROTO1_REG(cheb2ap)

// ellipap takes 3 args (N, Rp, Rs).
void ellipap_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ellipap: requires (N, Rp, Rs)",
                     0, 0, "ellipap", "", "numkit:ellipap:nargin");
    const int N     = static_cast<int>(args[0].toScalar());
    const double Rp = args[1].toScalar();
    const double Rs = args[2].toScalar();
    auto [z, p, k] = ellipap(N, Rp, Rs, ctx.engine->resource());
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(k);
}

#undef NK_PROTO0_REG
#undef NK_PROTO1_REG

// MATLAB's lp2* TF form returns the numerator at its true polynomial degree
// (length = #zeros + 1), NOT zero-padded to the denominator length the way
// zp2tf does. e.g. lp2lp of a Butterworth prototype (no zeros) returns
// bt = [Wo^N] (length 1), while zp2tf would give [0 … 0 Wo^N]. Strip the
// leading exact-zeros that zp2tf inserted so the TF form matches MATLAB.
// (No-op for lp2hp/lp2bs, whose numerators are already full length.)
static Value lp2StripLeadingZeros(const Value &b, std::pmr::memory_resource *mr)
{
    const size_t n = b.numel();
    size_t s = 0;
    while (s + 1 < n && b.elemAsDouble(s) == 0.0) ++s;
    if (s == 0) return b;
    auto r = Value::matrix(1, n - s, ValueType::DOUBLE, mr);
    double *d = r.doubleDataMut();
    for (size_t i = s; i < n; ++i) d[i - s] = b.elemAsDouble(i);
    return r;
}

// MATLAB lp2lp/lp2hp accept BOTH:
//   [zt, pt, kt] = lp2lp(z, p, k, Wo)   -- ZPK form (4 args)
//   [bt, at]     = lp2lp(b, a, Wo)      -- TF form  (3 args)
// Same for lp2hp. The TF form is identified by the 3-arg call and
// dispatches via tf2zpk -> ZPK lp2lp -> zp2tf.
#define NK_LP2X1_REG(name, fn)                                                  \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        auto *mr = ctx.engine->resource();                                       \
        if (args.size() == 3) {                                                  \
            /* TF form: (b, a, Wo) -> (bt, at). */                               \
            const double Wo = args[2].toScalar();                                \
            auto [z0, p0, k0] = tf2zpk(args[0], args[1], mr);                    \
            auto [zt, pt, kt] = fn(z0, p0, k0, Wo, mr);                          \
            auto [bt, at] = builtin::zp2tf(zt, pt, kt.toScalar(), mr);           \
            bt = lp2StripLeadingZeros(bt, mr);                                   \
            outs[0] = std::move(bt);                                             \
            if (nargout > 1) outs[1] = std::move(at);                            \
            return;                                                              \
        }                                                                        \
        if (args.size() < 4)                                                     \
            throw Error(#name ": requires (z, p, k, Wo) or (b, a, Wo)",         \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const double Wo = args[3].toScalar();                                   \
        auto [z, p, k] = fn(args[0], args[1], args[2].toScalar(), Wo, mr);      \
        outs[0] = std::move(z);                                                  \
        if (nargout > 1) outs[1] = std::move(p);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

NK_LP2X1_REG(lp2lp, lp2lp)
NK_LP2X1_REG(lp2hp, lp2hp)

#undef NK_LP2X1_REG

// MATLAB lp2bp/lp2bs accept BOTH:
//   [zt, pt, kt] = lp2bp(z, p, k, Wo, Bw)   -- ZPK form (5 args)
//   [bt, at]     = lp2bp(b, a, Wo, Bw)      -- TF form  (4 args)
#define NK_LP2X2_REG(name, fn)                                                  \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        auto *mr = ctx.engine->resource();                                       \
        if (args.size() == 4) {                                                  \
            /* TF form: (b, a, Wo, Bw) -> (bt, at). */                           \
            const double Wo = args[2].toScalar();                                \
            const double Bw = args[3].toScalar();                                \
            auto [z0, p0, k0] = tf2zpk(args[0], args[1], mr);                    \
            auto [zt, pt, kt] = fn(z0, p0, k0, Wo, Bw, mr);                      \
            auto [bt, at] = builtin::zp2tf(zt, pt, kt.toScalar(), mr);           \
            bt = lp2StripLeadingZeros(bt, mr);                                   \
            outs[0] = std::move(bt);                                             \
            if (nargout > 1) outs[1] = std::move(at);                            \
            return;                                                              \
        }                                                                        \
        if (args.size() < 5)                                                     \
            throw Error(#name ": requires (z, p, k, Wo, Bw) or (b, a, Wo, Bw)", \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const double Wo = args[3].toScalar();                                   \
        const double Bw = args[4].toScalar();                                   \
        auto [z, p, k] = fn(args[0], args[1], args[2].toScalar(), Wo, Bw, mr);  \
        outs[0] = std::move(z);                                                  \
        if (nargout > 1) outs[1] = std::move(p);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

NK_LP2X2_REG(lp2bp, lp2bp)
NK_LP2X2_REG(lp2bs, lp2bs)

#undef NK_LP2X2_REG

void bilinear_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("bilinear: requires (b, a, fs[, fp])",
                     0, 0, "bilinear", "", "numkit:bilinear:nargin");
    const double fs = args[2].toScalar();
    const double fp = (args.size() >= 4 && !args[3].isEmpty()) ? args[3].toScalar() : 0.0;
    auto [bd, ad] = bilinear(args[0], args[1], fs, fp, ctx.engine->resource());
    outs[0] = std::move(bd);
    if (nargout > 1) outs[1] = std::move(ad);
}

void impinvar_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("impinvar: requires (b, a, fs[, tol])",
                     0, 0, "impinvar", "", "numkit:impinvar:nargin");
    const double fs  = args[2].toScalar();
    const double tol = (args.size() >= 4 && !args[3].isEmpty()) ? args[3].toScalar() : 1e-3;
    auto [bd, ad] = impinvar(args[0], args[1], fs, tol, ctx.engine->resource());
    outs[0] = std::move(bd);
    if (nargout > 1) outs[1] = std::move(ad);
}

void freqs_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("freqs: requires (b, a, w)",
                     0, 0, "freqs", "", "numkit:freqs:nargin");
    outs[0] = freqs(args[0], args[1], args[2], ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::signal
