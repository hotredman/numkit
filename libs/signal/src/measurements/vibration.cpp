// libs/signal/src/measurements/vibration.cpp
//
// envspectrum / tachorpm / rainflow / tsa. Higher-order vibration
// functions (modal*, order tracking, rpm*maps) are not implemented
// here — they need a full state-space / RPM-resampling stack.

#include <numkit/signal/measurements/vibration.hpp>

#include <numkit/signal/transforms/fft.hpp>
#include <numkit/signal/transforms/hilbert.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

std::vector<double> readVec(const Value &x)
{
    const size_t n = x.numel();
    std::vector<double> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = x.elemAsDouble(i);
    return v;
}

Value vecCol(std::pmr::memory_resource *mr, const std::vector<double> &v)
{
    auto out = Value::matrix(v.size(), 1, ValueType::DOUBLE, mr);
    if (!v.empty())
        std::memcpy(out.doubleDataMut(), v.data(), v.size() * sizeof(double));
    return out;
}

} // anonymous

// ── envspectrum ────────────────────────────────────────────────────

std::tuple<Value, Value>
envspectrum(std::pmr::memory_resource *mr, const Value &x, double fs)
{
    const size_t N = x.numel();
    if (N == 0)
        return std::make_tuple(Value::matrix(0, 1, ValueType::DOUBLE, mr),
                               Value::matrix(0, 1, ValueType::DOUBLE, mr));

    // 1. Analytic-signal envelope.
    Value env = envelope(mr, x);
    // 2. AC-couple by subtracting the mean.
    auto e = readVec(env);
    double m = 0.0;
    for (double v : e) m += v;
    m /= e.size();
    for (auto &v : e) v -= m;

    // 3. FFT of the AC envelope, magnitude on the one-sided half.
    Value envAc = vecCol(mr, e);
    Value Z = fft(mr, envAc, /*n=*/-1, /*dim=*/0);
    const size_t nFft = Z.numel();
    const size_t nOne = nFft / 2 + 1;

    auto Es = Value::matrix(nOne, 1, ValueType::DOUBLE, mr);
    auto F  = Value::matrix(nOne, 1, ValueType::DOUBLE, mr);
    double *esd = Es.doubleDataMut();
    double *fd  = F.doubleDataMut();
    const Complex *zd = Z.complexData();

    const double fNyq = (fs > 0.0) ? fs * 0.5 : M_PI;
    const double scale = 2.0 / static_cast<double>(N);  // single-sided amp.
    for (size_t i = 0; i < nOne; ++i) {
        const double a = std::abs(zd[i]) * scale;
        esd[i] = (i == 0 || i == nOne - 1) ? 0.5 * a : a;
        fd[i]  = fNyq * static_cast<double>(i) / static_cast<double>(nOne - 1);
    }
    return std::make_tuple(std::move(Es), std::move(F));
}

// ── tachorpm ───────────────────────────────────────────────────────

std::tuple<Value, Value>
tachorpm(std::pmr::memory_resource *mr, const Value &x, double fs,
         const Value *threshold, int ppr)
{
    auto v = readVec(x);
    const size_t N = v.size();
    double thr;
    if (threshold && !threshold->isEmpty()) {
        thr = threshold->toScalar();
    } else {
        double lo = v.empty() ? 0.0 : v[0], hi = lo;
        for (double y : v) { if (y < lo) lo = y; if (y > hi) hi = y; }
        thr = 0.5 * (lo + hi);
    }
    // Detect rising-edge crossings of `thr`. Linear-interpolate the
    // sub-sample crossing instant for each.
    std::vector<double> times;
    for (size_t i = 0; i + 1 < N; ++i) {
        if (v[i] < thr && v[i + 1] >= thr) {
            const double a = v[i], b = v[i + 1];
            const double frac = (b == a) ? 0.0 : (thr - a) / (b - a);
            times.push_back((i + frac) / fs);
        }
    }
    // RPM from inter-pulse periods. With `ppr` pulses per revolution,
    // RPM = 60 / (period * ppr). Output one RPM per pulse (carry the
    // last estimate forward to the final pulse).
    std::vector<double> rpm(times.size(), 0.0);
    for (size_t i = 0; i + 1 < times.size(); ++i) {
        const double dt = times[i + 1] - times[i];
        if (dt > 0.0) rpm[i] = 60.0 / (dt * ppr);
    }
    if (rpm.size() >= 2) rpm.back() = rpm[rpm.size() - 2];

    return std::make_tuple(vecCol(mr, rpm), vecCol(mr, times));
}

// ── rainflow ───────────────────────────────────────────────────────
// ASTM E1049-85 cycle counting. Implementation follows the standard
// "four-point" rule applied to a sequence of peaks-and-valleys.

namespace {

// Turning points + their original-signal indices (1-based, MATLAB
// convention).
struct Turn { double v; double idx; };

std::vector<Turn> turningPointsWithIndex(const std::vector<double> &v)
{
    std::vector<Turn> out;
    if (v.empty()) return out;
    out.push_back({v.front(), 1.0});
    for (size_t i = 1; i + 1 < v.size(); ++i) {
        const double a = v[i - 1], b = v[i], c = v[i + 1];
        if ((b > a && b > c) || (b < a && b < c))
            out.push_back({b, static_cast<double>(i + 1)});
    }
    out.push_back({v.back(), static_cast<double>(v.size())});
    return out;
}

} // anonymous

Value rainflow(std::pmr::memory_resource *mr, const Value &x)
{
    auto v = readVec(x);
    auto tp = turningPointsWithIndex(v);

    // ASTM E1049-85 four-point algorithm. Each cycle yields a row
    //   [count, range, mean, start_idx, end_idx]
    // matching MATLAB R2025b's rainflow() return shape.
    struct Cycle { double count, range, mean, idx0, idx1; };
    std::vector<Cycle> cycles;
    std::vector<Turn> S;
    for (const auto &y : tp) {
        S.push_back(y);
        // Apply the rule until no more cycles can be extracted.
        while (S.size() >= 3) {
            const size_t n = S.size();
            const double X = std::abs(S[n - 1].v - S[n - 2].v);
            const double Y = std::abs(S[n - 2].v - S[n - 3].v);
            if (X < Y) break;
            if (n == 3) {
                // Half cycle: pop the bottom of the stack.
                const double rng = std::abs(S[0].v - S[1].v);
                const double mn  = 0.5 * (S[0].v + S[1].v);
                cycles.push_back({0.5, rng, mn, S[0].idx, S[1].idx});
                S.erase(S.begin());
                break;
            }
            // Full cycle on the inner pair.
            const double rng = Y;
            const double mn  = 0.5 * (S[n - 2].v + S[n - 3].v);
            cycles.push_back({1.0, rng, mn, S[n - 3].idx, S[n - 2].idx});
            S.erase(S.end() - 3, S.end() - 1);
        }
    }
    // Drain residual: every consecutive pair on the residual stack is
    // a half-cycle.
    for (size_t i = 0; i + 1 < S.size(); ++i) {
        cycles.push_back({0.5,
                          std::abs(S[i].v - S[i + 1].v),
                          0.5 * (S[i].v + S[i + 1].v),
                          S[i].idx,
                          S[i + 1].idx});
    }
    auto out = Value::matrix(cycles.size(), 5, ValueType::DOUBLE, mr);
    if (cycles.empty()) return out;
    double *d = out.doubleDataMut();
    const size_t rows = cycles.size();
    for (size_t i = 0; i < rows; ++i) {
        d[i + 0 * rows] = cycles[i].count;
        d[i + 1 * rows] = cycles[i].range;
        d[i + 2 * rows] = cycles[i].mean;
        d[i + 3 * rows] = cycles[i].idx0;
        d[i + 4 * rows] = cycles[i].idx1;
    }
    return out;
}

// ── tsa ────────────────────────────────────────────────────────────

std::tuple<Value, Value>
tsa(std::pmr::memory_resource *mr, const Value &x, double fs,
    const Value &rpm, double fs_rpm, int n_per_rev)
{
    auto signal_v = readVec(x);
    auto rpm_v    = readVec(rpm);
    if (signal_v.empty() || rpm_v.empty() || n_per_rev <= 0) {
        return std::make_tuple(
            Value::matrix(0, 1, ValueType::DOUBLE, mr),
            Value::matrix(0, 1, ValueType::DOUBLE, mr));
    }

    // 1. Compute the cumulative angle θ(t_signal) at each signal sample
    //    by integrating ω(t) = 2π·rpm/60 against time.
    //    rpm is sampled at fs_rpm, signal at fs; we interpolate rpm
    //    onto the signal-sample grid first.
    const size_t N = signal_v.size();
    std::vector<double> theta(N, 0.0);
    {
        double acc = 0.0;
        for (size_t i = 0; i < N; ++i) {
            const double t = static_cast<double>(i) / fs;
            // Linear-interpolate rpm at time t.
            const double idxR = t * fs_rpm;
            const size_t i0 = static_cast<size_t>(std::floor(idxR));
            const size_t i1 = std::min(i0 + 1, rpm_v.size() - 1);
            const double frac = std::min(std::max(idxR - i0, 0.0), 1.0);
            const double rpmI =
                (i0 < rpm_v.size())
                ? rpm_v[i0] + frac * (rpm_v[i1] - rpm_v[i0])
                : rpm_v.back();
            const double omega = 2.0 * M_PI * rpmI / 60.0;
            theta[i] = acc;
            acc += omega / fs;
        }
    }

    // 2. Re-sample onto a uniform angle grid by accumulating into
    //    n_per_rev bins per revolution.
    const double twoPi = 2.0 * M_PI;
    std::vector<double> sumBin(n_per_rev, 0.0);
    std::vector<int> cntBin(n_per_rev, 0);
    for (size_t i = 0; i < N; ++i) {
        double ang = std::fmod(theta[i], twoPi);
        if (ang < 0) ang += twoPi;
        int b = static_cast<int>(ang / twoPi * n_per_rev);
        if (b >= n_per_rev) b = n_per_rev - 1;
        sumBin[b] += signal_v[i];
        cntBin[b]++;
    }
    std::vector<double> avg(n_per_rev);
    for (int b = 0; b < n_per_rev; ++b)
        avg[b] = (cntBin[b] > 0) ? sumBin[b] / cntBin[b] : 0.0;

    std::vector<double> th(n_per_rev);
    for (int b = 0; b < n_per_rev; ++b)
        th[b] = twoPi * b / n_per_rev;

    return std::make_tuple(vecCol(mr, avg), vecCol(mr, th));
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void envspectrum_reg(Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("envspectrum: requires 1 argument",
                     0, 0, "envspectrum", "", "m:envspectrum:nargin");
    double fs = 0.0;
    if (args.size() >= 2 && !args[1].isEmpty()) fs = args[1].toScalar();
    auto [Es, F] = envspectrum(ctx.engine->resource(), args[0], fs);
    outs[0] = std::move(Es);
    if (nargout > 1) outs[1] = std::move(F);
}

void tachorpm_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tachorpm: requires (x, fs[, threshold[, ppr]])",
                     0, 0, "tachorpm", "", "m:tachorpm:nargin");
    const double fs = args[1].toScalar();
    const Value *thr = (args.size() >= 3 && !args[2].isEmpty()) ? &args[2] : nullptr;
    int ppr = 1;
    if (args.size() >= 4 && !args[3].isEmpty()) ppr = static_cast<int>(args[3].toScalar());
    auto [rpm, t] = tachorpm(ctx.engine->resource(), args[0], fs, thr, ppr);
    outs[0] = std::move(rpm);
    if (nargout > 1) outs[1] = std::move(t);
}

void rainflow_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rainflow: requires 1 argument",
                     0, 0, "rainflow", "", "m:rainflow:nargin");
    outs[0] = rainflow(ctx.engine->resource(), args[0]);
}

// MATLAB tsa convention: tsa(x, fs, tPulse) where tPulse is a vector
// of pulse arrival times (seconds) defining revolutions. Output is
// the average waveform over those revolutions.
//
// numkit historic convention: tsa(x, fs, rpm, fs_rpm[, n_per_rev])
// where rpm is a continuously-sampled rotation-rate signal.
//
// Dispatcher: 3-arg call -> MATLAB form; 4+ arg call -> numkit form.
namespace {

std::tuple<Value, Value>
tsa_matlab_form(std::pmr::memory_resource *mr, const Value &x, double fs,
                const Value &tPulse)
{
    const size_t nx = x.numel();
    const size_t np = tPulse.numel();
    if (nx == 0 || np < 2) {
        return std::make_tuple(
            Value::matrix(0, 1, ValueType::DOUBLE, mr),
            Value::matrix(0, 1, ValueType::DOUBLE, mr));
    }
    const double *xd = x.doubleData();
    std::vector<double> tv(np);
    for (size_t i = 0; i < np; ++i) tv[i] = tPulse.elemAsDouble(i);

    // Decide output length L = nominal samples per revolution. Use the
    // floor of the average inter-pulse interval times fs, matching
    // MATLAB's default behaviour.
    const double T_total = tv[np - 1] - tv[0];
    const size_t nRev = np - 1;
    const double T_avg = T_total / static_cast<double>(nRev);
    size_t L = static_cast<size_t>(std::floor(T_avg * fs));
    if (L < 1) L = 1;

    // For each revolution, linear-interpolate x onto an L-point grid
    // that spans [tv[i], tv[i+1]) and accumulate.
    std::vector<double> avg(L, 0.0);
    size_t nValidRevs = 0;
    for (size_t r = 0; r < nRev; ++r) {
        const double t0 = tv[r];
        const double t1 = tv[r + 1];
        const double dur = t1 - t0;
        if (dur <= 0.0) continue;
        bool revOK = true;
        std::vector<double> sample(L, 0.0);
        for (size_t k = 0; k < L; ++k) {
            const double tk = t0 + dur * static_cast<double>(k)
                                    / static_cast<double>(L);
            const double idxF = tk * fs;
            const long  i0   = static_cast<long>(std::floor(idxF));
            const double frac = idxF - static_cast<double>(i0);
            if (i0 < 0 || static_cast<size_t>(i0 + 1) >= nx) { revOK = false; break; }
            sample[k] = (1.0 - frac) * xd[i0] + frac * xd[i0 + 1];
        }
        if (!revOK) continue;
        for (size_t k = 0; k < L; ++k) avg[k] += sample[k];
        ++nValidRevs;
    }
    if (nValidRevs > 0) {
        for (size_t k = 0; k < L; ++k)
            avg[k] /= static_cast<double>(nValidRevs);
    }

    auto Ya = Value::matrix(L, 1, ValueType::DOUBLE, mr);
    auto Ta = Value::matrix(L, 1, ValueType::DOUBLE, mr);
    double *ya = Ya.doubleDataMut();
    double *ta = Ta.doubleDataMut();
    for (size_t k = 0; k < L; ++k) {
        ya[k] = avg[k];
        ta[k] = T_avg * static_cast<double>(k) / static_cast<double>(L);
    }
    return std::make_tuple(std::move(Ya), std::move(Ta));
}

} // namespace

void tsa_reg(Span<const Value> args, size_t nargout,
             Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("tsa: requires (x, fs, tPulse) or (x, fs, rpm, fs_rpm[, n_per_rev])",
                     0, 0, "tsa", "", "m:tsa:nargin");
    const double fs = args[1].toScalar();
    if (args.size() == 3) {
        // MATLAB form: tsa(x, fs, tPulse)
        auto [avg, th] = tsa_matlab_form(ctx.engine->resource(),
                                         args[0], fs, args[2]);
        outs[0] = std::move(avg);
        if (nargout > 1) outs[1] = std::move(th);
        return;
    }
    // 4+ args -> legacy numkit form (continuous rpm signal).
    const double fs_rpm = args[3].toScalar();
    int npr = 1024;
    if (args.size() >= 5 && !args[4].isEmpty()) npr = static_cast<int>(args[4].toScalar());
    auto [avg, th] = tsa(ctx.engine->resource(), args[0], fs, args[2], fs_rpm, npr);
    outs[0] = std::move(avg);
    if (nargout > 1) outs[1] = std::move(th);
}

} // namespace detail
} // namespace numkit::signal
