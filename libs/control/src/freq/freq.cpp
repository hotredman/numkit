// libs/control/src/freq/freq.cpp
//
// Frequency-domain responses. Reduces every LTI form to (num, den)
// coefficient rows via the existing libs/builtin (zp2tf) and
// libs/control (ss2tf) primitives, then evaluates the rational
// directly using Horner's method on std::complex<double>.
//
// Continuous: H(jω) = num(jω) / den(jω)
// Discrete  : H(e^{jωTs}) = num(z) / den(z) at z = exp(jωTs)

#include <numkit/control/freq/freq.hpp>
#include <numkit/control/conversion/conversion.hpp>
#include <numkit/control/props/props.hpp>

#include <numkit/builtin/math/poly/polynomials.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::control {

namespace {

using Cd = std::complex<double>;

bool hasKind(const Value &sys, const char *want) {
    if (!sys.isStruct() || !sys.hasField("kind")) return false;
    return sys.field("kind").toString() == want;
}

double sampleTime(const Value &sys) {
    if (sys.isStruct() && sys.hasField("Ts"))
        return sys.field("Ts").toScalar();
    return 0.0;
}

std::vector<double> coeffsReal(const Value &v) {
    std::vector<double> out(v.numel());
    for (size_t i = 0; i < v.numel(); ++i) out[i] = v.elemAsDouble(i);
    return out;
}

struct NumDen { std::vector<double> num, den; };

NumDen toNumDen(const Value &sys, std::pmr::memory_resource *mr) {
    if (hasKind(sys, "tf"))
        return {coeffsReal(sys.field("num")), coeffsReal(sys.field("den"))};
    if (hasKind(sys, "zpk")) {
        auto [num, den] = zp2tf(sys.field("z"), sys.field("p"), sys.field("k"), mr);
        return {coeffsReal(num), coeffsReal(den)};
    }
    if (hasKind(sys, "ss")) {
        auto [num, den] = ss2tf(sys.field("A"), sys.field("B"),
                                 sys.field("C"), sys.field("D"), /*iu=*/1, mr);
        return {coeffsReal(num), coeffsReal(den)};
    }
    throw Error("control freq: expected an LTI struct (tf/zpk/ss)",
                0, 0, "freq", "", "m:control:kind");
}

// Horner evaluation of a polynomial whose coefficients are stored in
// MATLAB convention (highest power first).
Cd hornerCx(const std::vector<double> &p, Cd x) {
    Cd y(0.0, 0.0);
    for (double c : p) y = y * x + c;
    return y;
}

// H(s) at s for the given (num, den).
Cd hAt(const std::vector<double> &num, const std::vector<double> &den, Cd s) {
    Cd n = hornerCx(num, s);
    Cd d = hornerCx(den, s);
    return n / d;
}

// Default log-spaced ω grid spanning ~ 2 decades around the
// pole/zero magnitudes. For discrete systems the upper bound is the
// Nyquist frequency π / Ts.
std::vector<double> pickW(const Value &sys, size_t N, std::pmr::memory_resource *mr)
{
    Value pV = pole(sys, mr);
    Value zV = zero(sys, mr);
    auto magsOf = [](const Value &v) {
        std::vector<double> mags;
        const size_t k = v.numel();
        if (v.type() == ValueType::COMPLEX) {
            const Cd *src = v.complexData();
            for (size_t i = 0; i < k; ++i) {
                const double m = std::abs(src[i]);
                if (m > 0.0) mags.push_back(m);
            }
        } else {
            for (size_t i = 0; i < k; ++i) {
                const double m = std::abs(v.elemAsDouble(i));
                if (m > 0.0) mags.push_back(m);
            }
        }
        return mags;
    };
    auto pm = magsOf(pV);
    auto zm = magsOf(zV);
    pm.insert(pm.end(), zm.begin(), zm.end());

    double lo = 1e-2, hi = 1e2;
    if (!pm.empty()) {
        double minM = pm[0], maxM = pm[0];
        for (double m : pm) {
            if (m < minM) minM = m;
            if (m > maxM) maxM = m;
        }
        lo = minM / 100.0;
        hi = maxM * 100.0;
    }
    const double Ts = sampleTime(sys);
    if (Ts > 0.0) {
        // Discrete: clamp to (0, π/Ts).
        const double nyq = M_PI / Ts;
        if (hi > nyq) hi = nyq;
        if (lo >= hi) lo = hi / 1000.0;
    }
    if (lo <= 0.0) lo = 1e-3;
    if (hi <= lo) hi = lo * 100.0;

    std::vector<double> w(N);
    const double a = std::log10(lo);
    const double b = std::log10(hi);
    for (size_t i = 0; i < N; ++i) {
        const double t = (N == 1) ? 0.0 : double(i) / double(N - 1);
        w[i] = std::pow(10.0, a + (b - a) * t);
    }
    return w;
}

std::vector<double> readW(const Value &sys, const Value &wArg, std::pmr::memory_resource *mr)
{
    if (wArg.numel() == 0) return pickW(sys, /*N=*/200, mr);
    std::vector<double> w(wArg.numel());
    for (size_t i = 0; i < wArg.numel(); ++i) w[i] = wArg.elemAsDouble(i);
    return w;
}

Value colDouble(const std::vector<double> &v, std::pmr::memory_resource *mr) {
    Value r = Value::matrix(v.size(), 1, ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), r.doubleDataMut());
    return r;
}

Value colComplex(const std::vector<Cd> &v, std::pmr::memory_resource *mr) {
    Value r = Value::matrix(v.size(), 1, ValueType::COMPLEX, mr);
    if (!v.empty()) {
        Cd *dst = r.complexDataMut();
        std::copy(v.begin(), v.end(), dst);
    }
    return r;
}

// Map ω → s (continuous) or z (discrete).
Cd freqArg(double w, double Ts) {
    if (Ts > 0.0) return std::polar(1.0, w * Ts);   // exp(jωTs)
    return Cd(0.0, w);                               // jω
}

} // anonymous

Value evalfr(const Value &sys, double f, std::pmr::memory_resource *mr)
{
    auto nd = toNumDen(sys, mr);
    const double Ts = sampleTime(sys);
    const Cd h = hAt(nd.num, nd.den, freqArg(f, Ts));
    Value out = Value::matrix(1, 1, ValueType::COMPLEX, mr);
    out.complexDataMut()[0] = h;
    return out;
}

Value freqresp(const Value &sys, const Value &w, std::pmr::memory_resource *mr)
{
    auto nd = toNumDen(sys, mr);
    const double Ts = sampleTime(sys);
    auto wv = readW(sys, w, mr);
    std::vector<Cd> h(wv.size());
    for (size_t i = 0; i < wv.size(); ++i)
        h[i] = hAt(nd.num, nd.den, freqArg(wv[i], Ts));
    return colComplex(h, mr);
}

BodeResult bode(const Value &sys, const Value &wArg, std::pmr::memory_resource *mr)
{
    auto nd = toNumDen(sys, mr);
    const double Ts = sampleTime(sys);
    auto w = readW(sys, wArg, mr);
    std::vector<double> mag(w.size()), phase(w.size());
    double prev = 0.0;
    for (size_t i = 0; i < w.size(); ++i) {
        Cd h = hAt(nd.num, nd.den, freqArg(w[i], Ts));
        mag[i] = std::abs(h);
        // Unwrap phase to avoid 2π jumps.
        double ph = std::arg(h) * (180.0 / M_PI);
        if (i > 0) {
            while (ph - prev > 180.0) ph -= 360.0;
            while (ph - prev < -180.0) ph += 360.0;
        }
        phase[i] = ph;
        prev = ph;
    }
    return {colDouble(mag, mr),
            colDouble(phase, mr),
            colDouble(w, mr)};
}

NyquistResult nyquist(const Value &sys, const Value &wArg, std::pmr::memory_resource *mr)
{
    auto nd = toNumDen(sys, mr);
    const double Ts = sampleTime(sys);
    auto w = readW(sys, wArg, mr);
    std::vector<double> re(w.size()), im(w.size());
    for (size_t i = 0; i < w.size(); ++i) {
        Cd h = hAt(nd.num, nd.den, freqArg(w[i], Ts));
        re[i] = h.real();
        im[i] = h.imag();
    }
    return {colDouble(re, mr), colDouble(im, mr), colDouble(w, mr)};
}

std::pair<Value, Value>
rlocus(const Value &sys, const Value &kArg, std::pmr::memory_resource *mr)
{
    auto nd = toNumDen(sys, mr);
    // Strip any leading-zero padding so the polynomial sums align on
    // the trailing (constant) term.
    auto strip = [](std::vector<double> v) {
        size_t i = 0;
        while (i + 1 < v.size() && v[i] == 0.0) ++i;
        return std::vector<double>(v.begin() + i, v.end());
    };
    auto num = strip(nd.num);
    auto den = strip(nd.den);
    if (den.empty())
        throw Error("rlocus: denominator must be non-empty",
                    0, 0, "rlocus", "", "m:rlocus:den");
    const size_t n = den.size() - 1;   // closed-loop order

    // Build gain vector.
    std::vector<double> ks;
    if (kArg.numel() == 0) {
        // 0 + 100 log-spaced points from 1e-2 to 1e3.
        ks.reserve(101);
        ks.push_back(0.0);
        const double a = -2.0, b = 3.0;
        for (size_t i = 0; i < 100; ++i) {
            const double t = double(i) / 99.0;
            ks.push_back(std::pow(10.0, a + (b - a) * t));
        }
    } else {
        ks.resize(kArg.numel());
        for (size_t i = 0; i < kArg.numel(); ++i) ks[i] = kArg.elemAsDouble(i);
    }

    const size_t K = ks.size();
    // Allocate K × n complex matrix.
    Value R = Value::matrix(K, n, ValueType::COMPLEX, mr);
    if (n > 0 && K > 0) {
        Cd *rd = R.complexDataMut();

        // Right-align num with den so adding den + k·num is a per-coefficient
        // sum. den has length n+1 (after strip), num has length numNumel ≤ n+1.
        std::vector<double> numAligned(den.size(), 0.0);
        const size_t pad = den.size() - num.size();
        for (size_t i = 0; i < num.size(); ++i)
            numAligned[pad + i] = num[i];

        std::vector<double> charPoly(den.size(), 0.0);
        for (size_t row = 0; row < K; ++row) {
            const double k = ks[row];
            for (size_t i = 0; i < den.size(); ++i)
                charPoly[i] = den[i] + k * numAligned[i];
            // Pack as a row Value and run builtin::roots.
            Value cp = Value::matrix(1, charPoly.size(),
                                     ValueType::DOUBLE, mr);
            std::copy(charPoly.begin(), charPoly.end(),
                      cp.doubleDataMut());
            Value rs = builtin::roots(mr, cp);
            // rs is a column of length n (for this n-th order polynomial).
            // Pack into row `row` of R (column-major: R[row, j] is at
            // index j*K + row).
            const size_t M = rs.numel();
            for (size_t j = 0; j < n; ++j) {
                Cd val(0.0, 0.0);
                if (j < M) {
                    if (rs.type() == ValueType::COMPLEX)
                        val = rs.complexData()[j];
                    else
                        val = Cd(rs.elemAsDouble(j), 0.0);
                }
                rd[j * K + row] = val;
            }
        }
    }
    return {std::move(R), colDouble(ks, mr)};
}

namespace detail {

void evalfr_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("evalfr: requires (sys, f)",
                    0, 0, "evalfr", "", "m:evalfr:nargin");
    o[0] = evalfr(a[0], a[1].toScalar(), c.engine->resource());
}

void freqresp_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("freqresp: requires (sys, w)",
                    0, 0, "freqresp", "", "m:freqresp:nargin");
    o[0] = freqresp(a[0], a[1], c.engine->resource());
}

void bode_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("bode: requires (sys [, w])",
                    0, 0, "bode", "", "m:bode:nargin");
    Value wArg = (a.size() >= 2) ? a[1]
                : Value::matrix(0, 0, ValueType::DOUBLE, c.engine->resource());
    auto b = bode(a[0], wArg, c.engine->resource());
    if (o.size() >= 1) o[0] = std::move(b.mag);
    if (o.size() >= 2) o[1] = std::move(b.phase);
    if (o.size() >= 3) o[2] = std::move(b.w);
}

void nyquist_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("nyquist: requires (sys [, w])",
                    0, 0, "nyquist", "", "m:nyquist:nargin");
    Value wArg = (a.size() >= 2) ? a[1]
                : Value::matrix(0, 0, ValueType::DOUBLE, c.engine->resource());
    auto n = nyquist(a[0], wArg, c.engine->resource());
    if (o.size() >= 1) o[0] = std::move(n.re);
    if (o.size() >= 2) o[1] = std::move(n.im);
    if (o.size() >= 3) o[2] = std::move(n.w);
}

void rlocus_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("rlocus: requires (sys [, k])",
                    0, 0, "rlocus", "", "m:rlocus:nargin");
    Value kArg = (a.size() >= 2) ? a[1]
                : Value::matrix(0, 0, ValueType::DOUBLE, c.engine->resource());
    auto [r, k] = rlocus(a[0], kArg, c.engine->resource());
    if (o.size() >= 1) o[0] = std::move(r);
    if (o.size() >= 2) o[1] = std::move(k);
}

} // namespace detail

} // namespace numkit::control
