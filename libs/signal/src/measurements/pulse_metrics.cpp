// libs/signal/src/measurements/pulse_metrics.cpp
//
// Pulse and transition metrics. Reference levels via histogram-mode
// statelevels; transitions detected via crossings of the lower / upper
// state boundaries (10% / 90% by default).

#include <numkit/signal/measurements/pulse_metrics.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace numkit::signal {

namespace {

constexpr double kLowPct  = 0.10;   // lower state boundary fraction
constexpr double kHighPct = 0.90;   // upper state boundary fraction
constexpr double kMidPct  = 0.50;   // mid-reference level
constexpr int    kHistBins = 100;

double scalarOrDefault(const Value &v, double dflt)
{
    return v.isEmpty() ? dflt : v.toScalar();
}

// Read x as a contiguous double vector. If x is not DOUBLE, copy via
// elemAsDouble. Returns the buffer (heap) and length.
std::vector<double> readVec(const Value &x)
{
    const size_t n = x.numel();
    std::vector<double> v(n);
    if (n == 0) return v;
    if (x.type() == ValueType::DOUBLE) {
        std::memcpy(v.data(), x.doubleData(), n * sizeof(double));
    } else {
        for (size_t i = 0; i < n; ++i) v[i] = x.elemAsDouble(i);
    }
    return v;
}

Value colVec(const std::vector<double> &v, std::pmr::memory_resource *mr)
{
    if (v.empty()) return Value::matrix(0, 1, ValueType::DOUBLE, mr);
    auto out = Value::matrix(v.size(), 1, ValueType::DOUBLE, mr);
    std::memcpy(out.doubleDataMut(), v.data(), v.size() * sizeof(double));
    return out;
}

// Histogram-mode state level estimate. Returns [low, high]. For flat
// inputs (range 0) returns [val, val].
struct LowHigh { double low; double high; };

LowHigh stateLevelsCalc(const double *x, size_t n)
{
    if (n == 0) return {0.0, 0.0};
    double xmin = x[0], xmax = x[0];
    for (size_t i = 1; i < n; ++i) {
        if (x[i] < xmin) xmin = x[i];
        if (x[i] > xmax) xmax = x[i];
    }
    if (!(xmax > xmin)) return {xmin, xmax};
    const double width = (xmax - xmin) / kHistBins;
    std::vector<int> hist(kHistBins, 0);
    for (size_t i = 0; i < n; ++i) {
        int b = static_cast<int>((x[i] - xmin) / width);
        if (b >= kHistBins) b = kHistBins - 1;
        if (b < 0) b = 0;
        hist[b]++;
    }
    const double mid = (xmin + xmax) * 0.5;
    const int splitBin = static_cast<int>((mid - xmin) / width);
    int lowBin = 0, highBin = kHistBins - 1;
    int lowMax = -1, highMax = -1;
    for (int b = 0; b < splitBin; ++b)
        if (hist[b] > lowMax) { lowMax = hist[b]; lowBin = b; }
    for (int b = splitBin; b < kHistBins; ++b)
        if (hist[b] > highMax) { highMax = hist[b]; highBin = b; }
    return {
        xmin + (lowBin  + 0.5) * width,
        xmin + (highBin + 0.5) * width,
    };
}

// Linearly interpolate the sample index where x crosses `level` between
// indices i and i+1 (assumes x[i] and x[i+1] straddle level). Returns
// fractional 0-based index.
double crossingIdx(const double *x, size_t i, double level)
{
    const double a = x[i], b = x[i + 1];
    if (a == b) return static_cast<double>(i);
    return i + (level - a) / (b - a);
}

// Convert a fractional sample index to time given a sample rate. With
// fs nullptr/empty, returns the 1-based fractional sample number
// (matches MATLAB's "no fs" idiom: index returned as t).
double idxToTime(double idx, double fs)
{
    if (fs > 0.0) return idx / fs;
    return idx + 1.0;  // 1-based sample index, fractional
}

// Find every monotonic transition between the lower and upper state
// boundaries. A transition has a `sign` (+1 = rising, -1 = falling),
// `start_idx` (frac sample where x crossed the trailing boundary),
// `end_idx` (frac sample where x crossed the leading boundary). The
// monotonic-trip rule mirrors MATLAB: between the trailing-boundary
// cross and the leading-boundary cross, x must not re-cross the
// trailing boundary.
struct Transition {
    int sign;        // +1 rising, -1 falling
    double startIdx; // crossing of trailing boundary (10% for rising)
    double endIdx;   // crossing of leading boundary (90% for rising)
};

std::vector<Transition>
findTransitions(const double *x, size_t n, double low, double high)
{
    std::vector<Transition> out;
    if (n < 2 || !(high > low)) return out;
    const double range = high - low;
    const double trail = low + kLowPct * range;
    const double lead  = low + kHighPct * range;

    enum class State { Below, Mid, Above };
    auto classify = [&](double v) -> State {
        if (v <= trail) return State::Below;
        if (v >= lead)  return State::Above;
        return State::Mid;
    };

    State curState = classify(x[0]);
    double pendingStart = 0.0;
    bool inRise = false, inFall = false;
    for (size_t i = 1; i < n; ++i) {
        State s = classify(x[i]);
        // Rising-transition tracking: arm when crossing from Below to
        // Mid (record the trail crossing); commit on Mid→Above (lead
        // crossing); cancel on any return to Below.
        if (s == State::Below) {
            inRise = false;
            // Falling: commit if we were tracking a fall
            if (inFall) {
                out.push_back({-1, pendingStart, crossingIdx(x, i - 1, trail)});
                inFall = false;
            }
        } else if (s == State::Above) {
            inFall = false;
            if (inRise) {
                out.push_back({+1, pendingStart, crossingIdx(x, i - 1, lead)});
                inRise = false;
            }
        }

        // Detect entry into a transition from Below (start of rise) or from
        // Above (start of fall).
        if (curState == State::Below && s != State::Below && !inRise) {
            // Crossed trailing boundary going up.
            inRise = true;
            pendingStart = crossingIdx(x, i - 1, trail);
            // Sharp edge: a single-sample jump Below->Above crosses BOTH the
            // trailing and leading boundaries within this same interval
            // [i-1, i]. Commit now — the flat region after has no leading
            // crossing, so the delayed-commit path would otherwise pin the
            // leading crossing to the wrong (flat) interval.
            if (s == State::Above) {
                out.push_back({+1, pendingStart, crossingIdx(x, i - 1, lead)});
                inRise = false;
            }
        }
        if (curState == State::Above && s != State::Above && !inFall) {
            // Crossed leading boundary going down.
            inFall = true;
            pendingStart = crossingIdx(x, i - 1, lead);
            if (s == State::Below) {  // sharp falling edge — same interval
                out.push_back({-1, pendingStart, crossingIdx(x, i - 1, trail)});
                inFall = false;
            }
        }
        curState = s;
    }
    return out;
}

// Midcross helper: every fractional sample where x crosses `level`.
// Each crossing is counted once (no duplicate if x stays on level).
std::vector<double>
midcrossesAt(const double *x, size_t n, double level)
{
    std::vector<double> out;
    for (size_t i = 0; i + 1 < n; ++i) {
        const double a = x[i], b = x[i + 1];
        if ((a < level && b >= level) || (a > level && b <= level)
            || (a == level && b != level && i == 0)) {
            out.push_back(crossingIdx(x, i, level));
        }
    }
    return out;
}

} // anonymous

// ── statelevels ────────────────────────────────────────────────────

Value statelevels(const Value &x, std::pmr::memory_resource *mr)
{
    auto v = readVec(x);
    auto lh = stateLevelsCalc(v.data(), v.size());
    auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    out.doubleDataMut()[0] = lh.low;
    out.doubleDataMut()[1] = lh.high;
    return out;
}

// ── midcross ───────────────────────────────────────────────────────

Value midcross(const Value &x, const Value &fs, std::pmr::memory_resource *mr)
{
    auto v = readVec(x);
    auto lh = stateLevelsCalc(v.data(), v.size());
    const double mid = lh.low + kMidPct * (lh.high - lh.low);
    const auto idx = midcrossesAt(v.data(), v.size(), mid);
    const double f = scalarOrDefault(fs, 0.0);
    std::vector<double> times(idx.size());
    for (size_t i = 0; i < idx.size(); ++i) times[i] = idxToTime(idx[i], f);
    return colVec(times, mr);
}

// ── risetime / falltime / slewrate ─────────────────────────────────

Value risetime(const Value &x, const Value &fs, std::pmr::memory_resource *mr)
{
    auto v = readVec(x);
    auto lh = stateLevelsCalc(v.data(), v.size());
    auto trs = findTransitions(v.data(), v.size(), lh.low, lh.high);
    const double f = scalarOrDefault(fs, 1.0);
    std::vector<double> out;
    for (const auto &t : trs)
        if (t.sign > 0) out.push_back((t.endIdx - t.startIdx) / std::max(f, 1.0));
    return colVec(out, mr);
}

Value falltime(const Value &x, const Value &fs, std::pmr::memory_resource *mr)
{
    auto v = readVec(x);
    auto lh = stateLevelsCalc(v.data(), v.size());
    auto trs = findTransitions(v.data(), v.size(), lh.low, lh.high);
    const double f = scalarOrDefault(fs, 1.0);
    std::vector<double> out;
    for (const auto &t : trs)
        if (t.sign < 0) out.push_back((t.endIdx - t.startIdx) / std::max(f, 1.0));
    return colVec(out, mr);
}

Value slewrate(const Value &x, const Value &fs, std::pmr::memory_resource *mr)
{
    auto v = readVec(x);
    auto lh = stateLevelsCalc(v.data(), v.size());
    auto trs = findTransitions(v.data(), v.size(), lh.low, lh.high);
    const double f = scalarOrDefault(fs, 1.0);
    const double range = (lh.high - lh.low) * (kHighPct - kLowPct);
    std::vector<double> out;
    for (const auto &t : trs) {
        const double dur = (t.endIdx - t.startIdx) / std::max(f, 1.0);
        if (dur > 0.0) out.push_back(t.sign * range / dur);
        else           out.push_back(0.0);
    }
    return colVec(out, mr);
}

// ── overshoot / undershoot ─────────────────────────────────────────
// MATLAB: percent OS = 100 * (peak_after_transition - high_state) /
// (high_state - low_state). The peak window is from the end of the
// transition until the next opposite transition (or end of signal).

Value overshoot(const Value &x, const Value &fs, std::pmr::memory_resource *mr)
{
    (void)fs; // overshoot uses transition list only, no time arg needed
    auto v = readVec(x);
    auto lh = stateLevelsCalc(v.data(), v.size());
    auto trs = findTransitions(v.data(), v.size(), lh.low, lh.high);
    const double range = lh.high - lh.low;
    std::vector<double> out;
    if (range <= 0.0) return colVec(out, mr);
    for (size_t k = 0; k < trs.size(); ++k) {
        const auto &t = trs[k];
        if (t.sign <= 0) continue;
        // Window: from end of rise to next transition (or end of signal).
        const size_t wbeg = static_cast<size_t>(std::ceil(t.endIdx));
        size_t wend = v.size();
        for (size_t j = k + 1; j < trs.size(); ++j) {
            wend = static_cast<size_t>(std::floor(trs[j].startIdx));
            break;
        }
        if (wbeg >= wend) continue;
        double peak = v[wbeg];
        for (size_t i = wbeg + 1; i < wend; ++i)
            if (v[i] > peak) peak = v[i];
        out.push_back(100.0 * (peak - lh.high) / range);
    }
    return colVec(out, mr);
}

Value undershoot(const Value &x, const Value &fs, std::pmr::memory_resource *mr)
{
    (void)fs;
    auto v = readVec(x);
    auto lh = stateLevelsCalc(v.data(), v.size());
    auto trs = findTransitions(v.data(), v.size(), lh.low, lh.high);
    const double range = lh.high - lh.low;
    std::vector<double> out;
    if (range <= 0.0) return colVec(out, mr);
    for (size_t k = 0; k < trs.size(); ++k) {
        const auto &t = trs[k];
        if (t.sign >= 0) continue;
        const size_t wbeg = static_cast<size_t>(std::ceil(t.endIdx));
        size_t wend = v.size();
        for (size_t j = k + 1; j < trs.size(); ++j) {
            wend = static_cast<size_t>(std::floor(trs[j].startIdx));
            break;
        }
        if (wbeg >= wend) continue;
        double trough = v[wbeg];
        for (size_t i = wbeg + 1; i < wend; ++i)
            if (v[i] < trough) trough = v[i];
        out.push_back(100.0 * (lh.low - trough) / range);
    }
    return colVec(out, mr);
}

// ── settlingtime ──────────────────────────────────────────────────
// Time from the start of each transition until x stays within `tol`
// of the destination state for the rest of the post-transition window.

Value settlingtime(const Value &x, const Value &fs, double tol, std::pmr::memory_resource *mr)
{
    auto v = readVec(x);
    auto lh = stateLevelsCalc(v.data(), v.size());
    auto trs = findTransitions(v.data(), v.size(), lh.low, lh.high);
    const double range = lh.high - lh.low;
    const double f = scalarOrDefault(fs, 1.0);
    std::vector<double> out;
    if (range <= 0.0) return colVec(out, mr);
    for (size_t k = 0; k < trs.size(); ++k) {
        const auto &t = trs[k];
        const double dest = (t.sign > 0) ? lh.high : lh.low;
        const double band = tol * range;
        size_t wend = v.size();
        for (size_t j = k + 1; j < trs.size(); ++j) {
            wend = static_cast<size_t>(std::floor(trs[j].startIdx));
            break;
        }
        // Walk backwards from wend-1 to find the LAST sample outside band.
        size_t lastOut = static_cast<size_t>(std::floor(t.startIdx));
        for (size_t i = static_cast<size_t>(std::floor(t.startIdx));
             i + 1 < wend && i < v.size(); ++i) {
            if (std::abs(v[i] - dest) > band) lastOut = i;
        }
        const double settled = static_cast<double>(lastOut) - t.startIdx;
        out.push_back(std::max(0.0, settled) / std::max(f, 1.0));
    }
    return colVec(out, mr);
}

// ── pulsewidth / pulseperiod / pulsesep / dutycycle ────────────────
// All built from mid-state crossings.

namespace {

// Build a list of (idx, dir) pairs: dir = +1 for rising mid-cross, -1
// for falling mid-cross. Used by the pulse-shape metrics below.
std::vector<std::pair<double, int>>
midCrossesDirected(const double *x, size_t n, double mid)
{
    std::vector<std::pair<double, int>> out;
    for (size_t i = 0; i + 1 < n; ++i) {
        const double a = x[i], b = x[i + 1];
        if (a < mid && b >= mid)      out.emplace_back(crossingIdx(x, i, mid), +1);
        else if (a > mid && b <= mid) out.emplace_back(crossingIdx(x, i, mid), -1);
    }
    return out;
}

} // anonymous

Value pulsewidth(const Value &x, const Value &fs, std::pmr::memory_resource *mr)
{
    auto v = readVec(x);
    auto lh = stateLevelsCalc(v.data(), v.size());
    const double mid = lh.low + kMidPct * (lh.high - lh.low);
    auto crs = midCrossesDirected(v.data(), v.size(), mid);
    const double f = scalarOrDefault(fs, 1.0);
    std::vector<double> out;
    // A pulse runs from a +1 crossing to the next -1 crossing.
    for (size_t i = 0; i + 1 < crs.size(); ++i) {
        if (crs[i].second == +1 && crs[i + 1].second == -1)
            out.push_back((crs[i + 1].first - crs[i].first) / std::max(f, 1.0));
    }
    return colVec(out, mr);
}

Value pulseperiod(const Value &x, const Value &fs, std::pmr::memory_resource *mr)
{
    auto v = readVec(x);
    auto lh = stateLevelsCalc(v.data(), v.size());
    const double mid = lh.low + kMidPct * (lh.high - lh.low);
    auto crs = midCrossesDirected(v.data(), v.size(), mid);
    const double f = scalarOrDefault(fs, 1.0);
    std::vector<double> out;
    // Period: from one rising mid-cross to the next rising mid-cross.
    std::vector<double> rising;
    for (const auto &c : crs) if (c.second == +1) rising.push_back(c.first);
    for (size_t i = 0; i + 1 < rising.size(); ++i)
        out.push_back((rising[i + 1] - rising[i]) / std::max(f, 1.0));
    return colVec(out, mr);
}

Value pulsesep(const Value &x, const Value &fs, std::pmr::memory_resource *mr)
{
    auto v = readVec(x);
    auto lh = stateLevelsCalc(v.data(), v.size());
    const double mid = lh.low + kMidPct * (lh.high - lh.low);
    auto crs = midCrossesDirected(v.data(), v.size(), mid);
    const double f = scalarOrDefault(fs, 1.0);
    std::vector<double> out;
    // Separation: trailing edge of one pulse to leading edge of the next.
    for (size_t i = 0; i + 1 < crs.size(); ++i) {
        if (crs[i].second == -1 && crs[i + 1].second == +1)
            out.push_back((crs[i + 1].first - crs[i].first) / std::max(f, 1.0));
    }
    return colVec(out, mr);
}

Value dutycycle(const Value &x, const Value &fs, std::pmr::memory_resource *mr)
{
    auto v = readVec(x);
    auto lh = stateLevelsCalc(v.data(), v.size());
    const double mid = lh.low + kMidPct * (lh.high - lh.low);
    auto crs = midCrossesDirected(v.data(), v.size(), mid);
    const double f = scalarOrDefault(fs, 1.0);
    (void)f;  // ratio is unitless; fs cancels.
    std::vector<double> out;
    // Duty cycle: pulsewidth / pulseperiod for each pulse.
    // Walk +1, -1, +1 triples.
    for (size_t i = 0; i + 2 < crs.size(); ++i) {
        if (crs[i].second == +1 && crs[i + 1].second == -1
            && crs[i + 2].second == +1) {
            const double width  = crs[i + 1].first - crs[i].first;
            const double period = crs[i + 2].first - crs[i].first;
            if (period > 0) out.push_back(width / period);
        }
    }
    return colVec(out, mr);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

#define NK_PULSE_REG(name, fn)                                                  \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires at least 1 argument",                 \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const Value &fs = (args.size() >= 2) ? args[1] : Value::Empty;         \
        outs[0] = fn(args[0], fs, ctx.engine->resource());                      \
    }

void statelevels_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("statelevels: requires 1 argument",
                     0, 0, "statelevels", "", "numkit:statelevels:nargin");
    outs[0] = statelevels(args[0], ctx.engine->resource());
}

void settlingtime_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("settlingtime: requires at least 1 argument",
                     0, 0, "settlingtime", "", "numkit:settlingtime:nargin");
    const Value &fs = (args.size() >= 2) ? args[1] : Value::Empty;
    double tol = 0.02;
    if (args.size() >= 3 && !args[2].isEmpty()) tol = args[2].toScalar();
    outs[0] = settlingtime(args[0], fs, tol, ctx.engine->resource());
}

// risetime / falltime expose up to 5 outputs (MATLAB):
//   [R, LT, UT, LL, UL] — duration, lower(10%) crossing time, upper(90%)
//   crossing time, and the lower/upper reference LEVELS (scalars). For a
//   fall the lower crossing happens last, the upper first (UT < LT).
void pulseRiseFall(Span<const Value> args, size_t nargout, Span<Value> outs,
                   CallContext &ctx, bool rising, const char *fnname)
{
    if (args.empty())
        throw Error(std::string(fnname) + ": requires at least 1 argument",
                     0, 0, fnname, "", std::string("numkit:") + fnname + ":nargin");
    auto *mr = ctx.engine->resource();
    const Value &fsv = (args.size() >= 2) ? args[1] : Value::Empty;
    auto v = readVec(args[0]);
    auto lh = stateLevelsCalc(v.data(), v.size());
    auto trs = findTransitions(v.data(), v.size(), lh.low, lh.high);
    const double f = scalarOrDefault(fsv, 1.0);
    const double range = lh.high - lh.low;
    const double LL = lh.low + kLowPct  * range;   // lower (10%) reference level
    const double UL = lh.low + kHighPct * range;   // upper (90%) reference level

    std::vector<double> dur, ltv, utv;
    for (const auto &t : trs) {
        if (rising && t.sign <= 0) continue;
        if (!rising && t.sign >= 0) continue;
        // Transition struct stores startIdx = first crossing in time,
        // endIdx = second. For a rise that is (lower, upper); for a fall it
        // is (upper, lower). Map to MATLAB's lower/upper crossing times.
        const double lowerCrossIdx = rising ? t.startIdx : t.endIdx;
        const double upperCrossIdx = rising ? t.endIdx   : t.startIdx;
        dur.push_back((t.endIdx - t.startIdx) / std::max(f, 1.0));
        ltv.push_back(idxToTime(lowerCrossIdx, f));
        utv.push_back(idxToTime(upperCrossIdx, f));
    }
    outs[0] = colVec(dur, mr);
    if (nargout > 1) outs[1] = colVec(ltv, mr);
    if (nargout > 2) outs[2] = colVec(utv, mr);
    if (nargout > 3) outs[3] = Value::scalar(LL, mr);
    if (nargout > 4) outs[4] = Value::scalar(UL, mr);
}

void risetime_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{ pulseRiseFall(args, nargout, outs, ctx, /*rising=*/true, "risetime"); }

void falltime_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{ pulseRiseFall(args, nargout, outs, ctx, /*rising=*/false, "falltime"); }

NK_PULSE_REG(midcross,    midcross)
NK_PULSE_REG(slewrate,    slewrate)
NK_PULSE_REG(overshoot,   overshoot)
NK_PULSE_REG(undershoot,  undershoot)
NK_PULSE_REG(pulsewidth,  pulsewidth)
NK_PULSE_REG(pulseperiod, pulseperiod)
NK_PULSE_REG(pulsesep,    pulsesep)
NK_PULSE_REG(dutycycle,   dutycycle)

#undef NK_PULSE_REG

} // namespace detail
} // namespace numkit::signal
