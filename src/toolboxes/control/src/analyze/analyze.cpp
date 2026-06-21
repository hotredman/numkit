// toolboxes/control/src/analyze/analyze.cpp
//
// dcgain  — H(0) (continuous) or H(1) (discrete) via evalfr.
// margin  — gain & phase margins from a dense bode grid + linear
//           interpolation around the crossover frequencies.
// stepinfo — rise/settling/peak metrics computed off the existing
//           step response.

#include <numkit/control/analyze/analyze.hpp>
#include <numkit/control/freq/freq.hpp>
#include <numkit/control/response/response.hpp>
#include <numkit/control/conversion/conversion.hpp>   // allmargin: open-loop num/den
#include <numkit/math/poly/polynomials.hpp>            // allmargin: roots for Stable

// Compute-only TU: Value substrate + Error, no engine. The dcgain/margin/
// stepinfo builtins (CallContext wrappers) live in analyze_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::control {

namespace {

double sampleTime(const Value &sys) {
    if (sys.isStruct() && sys.hasField("Ts"))
        return sys.field("Ts").toScalar();
    return 0.0;
}

std::vector<double> readCol(const Value &v) {
    std::vector<double> out(v.numel());
    for (size_t i = 0; i < v.numel(); ++i) out[i] = v.elemAsDouble(i);
    return out;
}

// Find the first index k where signal `s[k]` crosses level `lvl`
// (linear interpolation between samples). Returns -1 if no crossing.
struct CrossResult { bool found; double w; double yAtCross; };

CrossResult crossAt(const std::vector<double> &x,
                    const std::vector<double> &y,
                    double lvl)
{
    for (size_t i = 1; i < y.size(); ++i) {
        const double a = y[i - 1] - lvl;
        const double b = y[i] - lvl;
        if (a == 0.0)
            return {true, x[i - 1], y[i - 1]};
        if ((a < 0.0) != (b < 0.0)) {
            const double t = a / (a - b);   // 0..1
            const double w = x[i - 1] + t * (x[i] - x[i - 1]);
            const double yv = y[i - 1] + t * (y[i] - y[i - 1]);
            return {true, w, yv};
        }
    }
    return {false, 0.0, 0.0};
}

// Interpolate y at x = x0 (assumes x sorted ascending).
double interpAt(const std::vector<double> &x,
                const std::vector<double> &y,
                double x0)
{
    if (x.empty()) return 0.0;
    if (x0 <= x.front()) return y.front();
    if (x0 >= x.back()) return y.back();
    auto it = std::upper_bound(x.begin(), x.end(), x0);
    const size_t j = it - x.begin();
    const double t = (x0 - x[j - 1]) / (x[j] - x[j - 1]);
    return y[j - 1] + t * (y[j] - y[j - 1]);
}

} // anonymous

Value dcgain(const Value &sys, std::pmr::memory_resource *mr)
{
    // evalfr already does the right thing: at f=0 it evaluates s = j0
    // = 0 for continuous, and z = exp(j·0·Ts) = 1 for discrete.
    return evalfr(sys, 0.0, mr);
}

MarginResult margin(const Value &sys, std::pmr::memory_resource *mr)
{
    // Build a dense default Bode grid (the auto-pick chooses ~200
    // points spanning two decades around the dominant poles).
    Value emptyW = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto bodeOut = bode(sys, emptyW, mr);

    auto w = readCol(bodeOut.w);
    auto mag = readCol(bodeOut.mag);
    auto phase = readCol(bodeOut.phase);

    // Phase crossover (where phase = -180°). On a typical loop the
    // phase descends through -180; we scan from low to high frequency.
    auto pc = crossAt(w, phase, -180.0);
    // Gain crossover (where |H| = 1, equivalently 0 dB). We scan the
    // *log* magnitude so a linear interpolation matches the bode plot.
    std::vector<double> magLog(mag.size());
    for (size_t i = 0; i < mag.size(); ++i)
        magLog[i] = (mag[i] > 0.0) ? 20.0 * std::log10(mag[i])
                                   : -300.0;
    auto gc = crossAt(w, magLog, 0.0);

    const double inf = std::numeric_limits<double>::infinity();
    double Gm = inf, Pm = inf, Wcg = inf, Wcp = inf;
    if (pc.found) {
        Wcg = pc.w;
        const double magAtPc = interpAt(w, mag, Wcg);
        Gm = (magAtPc > 0.0) ? 1.0 / magAtPc : inf;
    }
    if (gc.found) {
        Wcp = gc.w;
        const double phAtGc = interpAt(w, phase, Wcp);
        Pm = phAtGc + 180.0;
    }

    return {Value::scalar(Gm,  mr),
            Value::scalar(Pm,  mr),
            Value::scalar(Wcg, mr),
            Value::scalar(Wcp, mr)};
}

namespace {

// Open-loop (num, den) of an LTI struct as descending-power coefficient rows.
std::pair<std::vector<double>, std::vector<double>>
openLoopNumDen(const Value &sys, std::pmr::memory_resource *mr) {
    auto coeffs = [](const Value &v) {
        std::vector<double> c(v.numel());
        for (size_t i = 0; i < v.numel(); ++i) c[i] = v.elemAsDouble(i);
        return c;
    };
    if (sys.isStruct() && sys.hasField("kind")) {
        const std::string k = sys.field("kind").toString();
        if (k == "tf")
            return {coeffs(sys.field("num")), coeffs(sys.field("den"))};
        if (k == "zpk") {
            auto [num, den] = zp2tf(sys.field("z"), sys.field("p"), sys.field("k"), mr);
            return {coeffs(num), coeffs(den)};
        }
        if (k == "ss") {
            auto [num, den] = ss2tf(sys.field("A"), sys.field("B"),
                                    sys.field("C"), sys.field("D"), 1, mr);
            return {coeffs(num), coeffs(den)};
        }
    }
    throw Error("allmargin: expected an LTI struct (tf/zpk/ss)",
                0, 0, "allmargin", "", "numkit:allmargin:kind");
}

Value rowFrom(const std::vector<double> &v, std::pmr::memory_resource *mr) {
    Value r = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < v.size(); ++i) r.doubleDataMut()[i] = v[i];
    return r;
}

// Horner evaluation of a descending-power real polynomial at a complex point.
std::complex<double> polyvalC(const std::vector<double> &c, std::complex<double> x) {
    std::complex<double> r(0.0, 0.0);
    for (double a : c) r = r * x + a;
    return r;
}

// Exact open-loop frequency response G(jω) = num(jω) / den(jω).
std::complex<double> evalG(const std::vector<double> &num,
                           const std::vector<double> &den, double w) {
    const std::complex<double> jw(0.0, w);
    const std::complex<double> d = polyvalC(den, jw);
    if (d == std::complex<double>(0.0, 0.0))
        return {std::numeric_limits<double>::infinity(), 0.0};
    return polyvalC(num, jw) / d;
}

// Bisect a scalar f(ω) for its root in [a,b] (sign change assumed), ~60 iters.
template <class F>
double bisectRoot(F f, double a, double b) {
    double fa = f(a);
    for (int it = 0; it < 80; ++it) {
        const double m = 0.5 * (a + b);
        const double fm = f(m);
        if (fm == 0.0 || (b - a) <= 1e-13 * (1.0 + b)) return m;
        if ((fa < 0.0) != (fm < 0.0)) b = m;
        else { a = m; fa = fm; }
    }
    return 0.5 * (a + b);
}

} // anonymous

Value allmargin(const Value &sys, std::pmr::memory_resource *mr)
{
    // Exact open-loop response G(jω) = num(jω)/den(jω) (no bode-grid
    // interpolation): scan a fine log-frequency grid for sign changes of
    // |G|−1 (gain crossovers) and Im(G) with Re(G)<0 (phase crossovers),
    // then bisect each bracket on the exact response.
    auto [num, den] = openLoopNumDen(sys, mr);

    const double inf = std::numeric_limits<double>::infinity();
    const double PI = 3.14159265358979323846;

    // Frequency span from the pole/zero magnitudes (±3 decades).
    double wlo = 1e-3, whi = 1e3;
    {
        std::vector<double> mags;
        auto addRoots = [&](const std::vector<double> &c) {
            if (c.size() < 2) return;
            Value rts = numkit::math::roots(rowFrom(c, mr), mr);
            const size_t n = rts.numel();
            if (rts.type() == ValueType::COMPLEX) {
                const std::complex<double> *p = rts.complexData();
                for (size_t i = 0; i < n; ++i) { double m = std::abs(p[i]); if (m > 1e-9) mags.push_back(m); }
            } else {
                for (size_t i = 0; i < n; ++i) { double m = std::abs(rts.elemAsDouble(i)); if (m > 1e-9) mags.push_back(m); }
            }
        };
        addRoots(num);
        addRoots(den);
        if (!mags.empty()) {
            wlo = *std::min_element(mags.begin(), mags.end()) * 1e-3;
            whi = *std::max_element(mags.begin(), mags.end()) * 1e3;
        }
    }

    std::vector<double> gmVal, gmFreq, pmVal, pmFreq, dmVal, dmFreq;
    const int N = 4000;
    const double llo = std::log10(wlo), lhi = std::log10(whi);
    double wPrev = wlo;
    std::complex<double> gPrev = evalG(num, den, wPrev);
    for (int i = 1; i <= N; ++i) {
        const double w = std::pow(10.0, llo + (lhi - llo) * (double)i / N);
        const std::complex<double> g = evalG(num, den, w);

        // Gain crossover: |G| − 1 changes sign → phase + delay margin.
        const double a = std::abs(gPrev) - 1.0, b = std::abs(g) - 1.0;
        if (std::isfinite(a) && std::isfinite(b) && a != 0.0 && (a < 0.0) != (b < 0.0)) {
            const double wc = bisectRoot(
                [&](double x) { return std::abs(evalG(num, den, x)) - 1.0; }, wPrev, w);
            const std::complex<double> gc = evalG(num, den, wc);
            double PM = std::arg(gc) * 180.0 / PI + 180.0;
            while (PM > 180.0) PM -= 360.0;
            while (PM <= -180.0) PM += 360.0;
            pmVal.push_back(PM);
            pmFreq.push_back(wc);
            double pmRad = PM * PI / 180.0;
            while (pmRad < 0.0) pmRad += 2.0 * PI;
            dmVal.push_back(wc > 0.0 ? pmRad / wc : inf);
            dmFreq.push_back(wc);
        }

        // Phase crossover: Im(G) changes sign and Re(G) < 0 → ∠G = ±180°.
        const double ia = gPrev.imag(), ib = g.imag();
        if (std::isfinite(ia) && std::isfinite(ib) && ia != 0.0 && (ia < 0.0) != (ib < 0.0)) {
            const double wc = bisectRoot(
                [&](double x) { return evalG(num, den, x).imag(); }, wPrev, w);
            const std::complex<double> gc = evalG(num, den, wc);
            if (gc.real() < 0.0) {
                const double m = std::abs(gc);
                gmVal.push_back(m > 0.0 ? 1.0 / m : inf);
                gmFreq.push_back(wc);
            }
        }
        wPrev = w;
        gPrev = g;
    }

    // Closed-loop (negative unity feedback) stability: roots(den + num).
    bool stable = true;
    {
        const size_t L = std::max(num.size(), den.size());
        std::vector<double> cl(L, 0.0);
        for (size_t i = 0; i < den.size(); ++i) cl[L - den.size() + i] += den[i];
        for (size_t i = 0; i < num.size(); ++i) cl[L - num.size() + i] += num[i];
        size_t s0 = 0;
        while (s0 + 1 < cl.size() && cl[s0] == 0.0) ++s0;
        cl.erase(cl.begin(), cl.begin() + s0);
        if (cl.size() >= 2) {
            Value rts = numkit::math::roots(rowFrom(cl, mr), mr);
            const size_t n = rts.numel();
            if (rts.type() == ValueType::COMPLEX) {
                const std::complex<double> *p = rts.complexData();
                for (size_t i = 0; i < n; ++i)
                    if (p[i].real() >= -1e-9) { stable = false; break; }
            } else {
                for (size_t i = 0; i < n; ++i)
                    if (rts.elemAsDouble(i) >= -1e-9) { stable = false; break; }
            }
        }
    }

    Value s = Value::structure(mr);
    s.field("GainMargin")  = rowFrom(gmVal,  mr);
    s.field("GMFrequency") = rowFrom(gmFreq, mr);
    s.field("PhaseMargin") = rowFrom(pmVal,  mr);
    s.field("PMFrequency") = rowFrom(pmFreq, mr);
    s.field("DelayMargin") = rowFrom(dmVal,  mr);
    s.field("DMFrequency") = rowFrom(dmFreq, mr);
    s.field("Stable")      = Value::logicalScalar(stable, mr);
    return s;
}

Value stepinfo(const Value &sys, std::pmr::memory_resource *mr)
{
    // Use a deliberately long horizon (multiplier on top of the
    // default Tfinal pickup) so settling-time detection has enough
    // tail to register. We do that by pre-computing the default
    // grid via step(sys), then re-running on a 2× tFinal vector.
    Value emptyT = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto [y0, t0] = step_response(sys, emptyT, mr);
    auto t = readCol(t0);
    auto y = readCol(y0);
    if (t.size() < 2) {
        Value s = Value::structure(mr);
        return s;
    }
    const double Tfinal = t.back();
    // Re-grid out to 2× Tfinal.
    Value tArg = Value::scalar(2.0 * Tfinal, mr);
    auto [y1, t1] = step_response(sys, tArg, mr);
    t = readCol(t1);
    y = readCol(y1);

    const double yfinal = y.back();
    const double absYf = std::abs(yfinal);
    const double band = 0.02 * absYf;        // 2% settling band

    // Peak / peak time.
    double peak = std::abs(y[0]);
    double peakTime = t[0];
    for (size_t i = 0; i < y.size(); ++i) {
        const double a = std::abs(y[i]);
        if (a > peak) { peak = a; peakTime = t[i]; }
    }

    // Overshoot / undershoot (only if yfinal != 0).
    double overshoot = 0.0;
    double undershoot = 0.0;
    if (yfinal > 0.0) {
        double maxY = y[0];
        for (double v : y) maxY = std::max(maxY, v);
        overshoot = std::max(0.0, 100.0 * (maxY - yfinal) / yfinal);
        double minBeforePeak = 0.0;
        for (double v : y) {
            if (v < minBeforePeak) minBeforePeak = v;
        }
        undershoot = std::max(0.0, 100.0 * (-minBeforePeak) / yfinal);
    } else if (yfinal < 0.0) {
        double minY = y[0];
        for (double v : y) minY = std::min(minY, v);
        overshoot = std::max(0.0, 100.0 * (minY - yfinal) / yfinal);
    }

    // Rise time: 10 % → 90 % of yfinal (sign-aware).
    double riseTime = std::numeric_limits<double>::quiet_NaN();
    if (absYf > 0.0) {
        const double y10 = 0.1 * yfinal;
        const double y90 = 0.9 * yfinal;
        // Find first sample crossing y10, then first subsequent crossing y90.
        double t10 = std::numeric_limits<double>::quiet_NaN();
        double t90 = std::numeric_limits<double>::quiet_NaN();
        for (size_t i = 1; i < y.size(); ++i) {
            const double a = y[i - 1] - y10;
            const double b = y[i] - y10;
            if ((a < 0.0) != (b < 0.0) && std::isnan(t10)) {
                const double tt = a / (a - b);
                t10 = t[i - 1] + tt * (t[i] - t[i - 1]);
            }
            const double a9 = y[i - 1] - y90;
            const double b9 = y[i] - y90;
            if ((a9 < 0.0) != (b9 < 0.0)) {
                const double tt = a9 / (a9 - b9);
                t90 = t[i - 1] + tt * (t[i] - t[i - 1]);
                if (!std::isnan(t10)) break;
            }
        }
        if (!std::isnan(t10) && !std::isnan(t90))
            riseTime = t90 - t10;
    }

    // Settling time: last index where |y - yfinal| > band.
    double settlingTime = 0.0;
    for (size_t i = 0; i < y.size(); ++i) {
        if (std::abs(y[i] - yfinal) > band)
            settlingTime = t[i];
    }

    // Transient time (MATLAB R2025b's 2nd S field): like SettlingTime but the
    // 2% band is relative to the PEAK deviation max|y(t)-yfinal| rather than
    // |yinit-yfinal|. For a standard step the peak deviation occurs at t=0 and
    // equals |yfinal| (yinit=0), so TransientTime == SettlingTime; it can be
    // smaller when |y-yfinal| overshoots past |yfinal|.
    double peakDev = 0.0;
    for (size_t i = 0; i < y.size(); ++i)
        peakDev = std::max(peakDev, std::abs(y[i] - yfinal));
    const double transBand = 0.02 * peakDev;
    double transientTime = 0.0;
    for (size_t i = 0; i < y.size(); ++i) {
        if (std::abs(y[i] - yfinal) > transBand)
            transientTime = t[i];
    }
    // Min/max within the settling band (after settling).
    double sMin = yfinal, sMax = yfinal;
    for (size_t i = 0; i < y.size(); ++i) {
        if (t[i] >= settlingTime) {
            sMin = std::min(sMin, y[i]);
            sMax = std::max(sMax, y[i]);
        }
    }

    Value s = Value::structure(mr);
    s.field("RiseTime")      = Value::scalar(riseTime, mr);
    s.field("TransientTime") = Value::scalar(transientTime, mr);
    s.field("SettlingTime")  = Value::scalar(settlingTime, mr);
    s.field("SettlingMin")  = Value::scalar(sMin, mr);
    s.field("SettlingMax")  = Value::scalar(sMax, mr);
    s.field("Overshoot")    = Value::scalar(overshoot, mr);
    s.field("Undershoot")   = Value::scalar(undershoot, mr);
    s.field("Peak")         = Value::scalar(peak, mr);
    s.field("PeakTime")     = Value::scalar(peakTime, mr);
    return s;
}

} // namespace numkit::control
