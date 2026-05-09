// libs/signal/src/digital_filtering/spec_driven.cpp
//
// lowpass / highpass / bandpass / bandstop -- wrappers over the
// MATLAB-default IIR filter design + filtfilt.
//
// MATLAB's lowpass(x, fpass) (and friends) defaults match
// designfilt("lowpassiir", "FilterOrder", N, "PassbandFrequency", fpass,
//            "PassbandRipple", 0.1, "StopbandAttenuation", 60,
//            "DesignMethod", "ellip") with cutoff = midpoint(fpass, fstop)
// where fstop = fpass + (1 - Steepness)*(1 - fpass), Steepness = 0.85.
// We compose ellip() + filtfilt() with the same parameters.

#include <numkit/signal/digital_filtering/spec_driven.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>
#include <numkit/signal/digital_filtering/filter.hpp>
#include <numkit/signal/digital_filtering/sosfilt.hpp>
#include <numkit/signal/filter_design/filter_design.hpp>
#include <numkit/signal/filter_design/iir_designs.hpp>
#include <numkit/signal/filter_implementation/conversions.hpp>

#include <cmath>

namespace numkit::signal {

namespace {

double normaliseW(double f, double fs, const char *fnName)
{
    if (!(fs > 0))
        throw Error(std::string(fnName) + ": fs must be positive",
                     0, 0, fnName, "", std::string("m:") + fnName + ":badFs");
    const double w = 2.0 * f / fs;
    if (!(w > 0.0) || !(w < 1.0))
        throw Error(std::string(fnName) + ": cutoff must be in (0, fs/2)",
                     0, 0, fnName, "", std::string("m:") + fnName + ":badCutoff");
    return w;
}

void validateOrder(int order, const char *fnName)
{
    if (order < 1)
        throw Error(std::string(fnName) + ": order must be >= 1",
                     0, 0, fnName, "", std::string("m:") + fnName + ":badOrder");
}

// MATLAB lowpass/highpass/bandpass/bandstop default parameters --
// these match what designfilt("lowpassiir", ..., "DesignMethod", "ellip")
// produces under the lowpass/highpass/etc front-end.
constexpr double kDefaultRp = 0.1;     // passband ripple, dB
constexpr double kDefaultRs = 60.0;    // stopband attenuation, dB
constexpr int    kDefaultIirOrder  = 7; // matches MATLAB IIR-branch default
// (Steepness=0.85 only affects the FIR branch's transition width; the
// IIR ellip path uses the passband edge directly as Wp.)

inline Value scalarWn(std::pmr::memory_resource *mr, double w) {
    return Value::scalar(w, mr);
}
inline Value pairWn(std::pmr::memory_resource *mr, double w1, double w2) {
    auto v = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    v.doubleDataMut()[0] = w1;
    v.doubleDataMut()[1] = w2;
    return v;
}

} // namespace

Value lowpass(std::pmr::memory_resource *mr, const Value &x,
              double fpass, double fs, int order)
{
    validateOrder(order, "lowpass");
    const double Wp = normaliseW(fpass, fs, "lowpass");
    // honour explicit order; remap legacy default 8 -> 7 to match MATLAB
    const int N = (order == 8) ? kDefaultIirOrder : order;
    auto [b, a] = ellip(mr, N, kDefaultRp, kDefaultRs,
                        scalarWn(mr, Wp), FilterType::Lowpass, /*analog=*/false);
    // SOS-form filtfilt is numerically stable for high-order IIR --
    // matches MATLAB filtfilt(d, x) for digitalFilter SOS objects.
    auto sos = tf2sos(mr, b, a);
    return sosfiltfilt(mr, sos, x);
}

Value highpass(std::pmr::memory_resource *mr, const Value &x,
               double fpass, double fs, int order)
{
    validateOrder(order, "highpass");
    const double Wp = normaliseW(fpass, fs, "highpass");
    const int N = (order == 8) ? kDefaultIirOrder : order;
    auto [b, a] = ellip(mr, N, kDefaultRp, kDefaultRs,
                        scalarWn(mr, Wp), FilterType::Highpass, /*analog=*/false);
    auto sos = tf2sos(mr, b, a);
    return sosfiltfilt(mr, sos, x);
}

Value bandpass(std::pmr::memory_resource *mr, const Value &x,
               double flo, double fhi, double fs, int order)
{
    validateOrder(order, "bandpass");
    if (!(flo < fhi))
        throw Error("bandpass: low cutoff must be < high cutoff",
                     0, 0, "bandpass", "", "m:bandpass:badRange");
    const double Wlo = normaliseW(flo, fs, "bandpass");
    const double Whi = normaliseW(fhi, fs, "bandpass");
    const int N = (order == 8) ? kDefaultIirOrder : order;
    auto [b, a] = ellip(mr, N, kDefaultRp, kDefaultRs,
                        pairWn(mr, Wlo, Whi),
                        FilterType::Bandpass, /*analog=*/false);
    auto sos = tf2sos(mr, b, a);
    return sosfiltfilt(mr, sos, x);
}

Value bandstop(std::pmr::memory_resource *mr, const Value &x,
               double flo, double fhi, double fs, int order)
{
    validateOrder(order, "bandstop");
    if (!(flo < fhi))
        throw Error("bandstop: low cutoff must be < high cutoff",
                     0, 0, "bandstop", "", "m:bandstop:badRange");
    const double Wlo = normaliseW(flo, fs, "bandstop");
    const double Whi = normaliseW(fhi, fs, "bandstop");
    const int N = (order == 8) ? kDefaultIirOrder : order;
    auto [b, a] = ellip(mr, N, kDefaultRp, kDefaultRs,
                        pairWn(mr, Wlo, Whi),
                        FilterType::Bandstop, /*analog=*/false);
    auto sos = tf2sos(mr, b, a);
    return sosfiltfilt(mr, sos, x);
}

namespace detail {

static double scalarOr(const Value &v, double dflt) {
    return v.numel() ? v.toScalar() : dflt;
}

// MATLAB lowpass/highpass/bandpass/bandstop: when fs is omitted, the
// cutoffs are interpreted as already normalized to Nyquist, equivalent
// to fs = 2 (so fpass in [0, 1] maps to [0, pi] in normalized rad/sample).
void lowpass_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lowpass: requires (x, fpass[, fs])",
                     0, 0, "lowpass", "", "m:lowpass:nargin");
    const double fs = (args.size() >= 3) ? args[2].toScalar() : 2.0;
    const int order = (args.size() >= 4) ? static_cast<int>(args[3].toScalar()) : 8;
    outs[0] = lowpass(ctx.engine->resource(), args[0],
                      args[1].toScalar(), fs, order);
}

void highpass_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("highpass: requires (x, fpass[, fs])",
                     0, 0, "highpass", "", "m:highpass:nargin");
    const double fs = (args.size() >= 3) ? args[2].toScalar() : 2.0;
    const int order = (args.size() >= 4) ? static_cast<int>(args[3].toScalar()) : 8;
    outs[0] = highpass(ctx.engine->resource(), args[0],
                       args[1].toScalar(), fs, order);
}

void bandpass_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bandpass: requires (x, [flo fhi][, fs])",
                     0, 0, "bandpass", "", "m:bandpass:nargin");
    if (args[1].numel() != 2)
        throw Error("bandpass: cutoff must be a 2-element [flo fhi]",
                     0, 0, "bandpass", "", "m:bandpass:badCutoff");
    const double fs = (args.size() >= 3) ? args[2].toScalar() : 2.0;
    const int order = (args.size() >= 4) ? static_cast<int>(args[3].toScalar()) : 8;
    outs[0] = bandpass(ctx.engine->resource(), args[0],
                       args[1].elemAsDouble(0), args[1].elemAsDouble(1),
                       fs, order);
}

void bandstop_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bandstop: requires (x, [flo fhi][, fs])",
                     0, 0, "bandstop", "", "m:bandstop:nargin");
    if (args[1].numel() != 2)
        throw Error("bandstop: cutoff must be a 2-element [flo fhi]",
                     0, 0, "bandstop", "", "m:bandstop:badCutoff");
    const double fs = (args.size() >= 3) ? args[2].toScalar() : 2.0;
    const int order = (args.size() >= 4) ? static_cast<int>(args[3].toScalar()) : 8;
    outs[0] = bandstop(ctx.engine->resource(), args[0],
                       args[1].elemAsDouble(0), args[1].elemAsDouble(1),
                       fs, order);
    (void)scalarOr;   // silence unused-helper warning
}

} // namespace detail

} // namespace numkit::signal
