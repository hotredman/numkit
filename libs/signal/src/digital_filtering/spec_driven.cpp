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
#include <numkit/value/value.hpp>
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
                     0, 0, fnName, "", std::string("numkit:") + fnName + ":badFs");
    const double w = 2.0 * f / fs;
    if (!(w > 0.0) || !(w < 1.0))
        throw Error(std::string(fnName) + ": cutoff must be in (0, fs/2)",
                     0, 0, fnName, "", std::string("numkit:") + fnName + ":badCutoff");
    return w;
}

void validateOrder(int order, const char *fnName)
{
    if (order < 1)
        throw Error(std::string(fnName) + ": order must be >= 1",
                     0, 0, fnName, "", std::string("numkit:") + fnName + ":badOrder");
}

// MATLAB lowpass/highpass/bandpass/bandstop default parameters --
// these match what designfilt("lowpassiir", ..., "DesignMethod", "ellip")
// produces under the lowpass/highpass/etc front-end.
constexpr double kDefaultRp = 0.1;     // passband ripple, dB
constexpr double kDefaultRs = 60.0;    // stopband attenuation, dB
constexpr int    kDefaultIirOrder  = 7; // matches MATLAB IIR-branch default
// (Steepness=0.85 only affects the FIR branch's transition width; the
// IIR ellip path uses the passband edge directly as Wp.)

inline Value scalarWn(double w, std::pmr::memory_resource *mr) {
    return Value::scalar(w, mr);
}
inline Value pairWn(double w1, double w2, std::pmr::memory_resource *mr) {
    auto v = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    v.doubleDataMut()[0] = w1;
    v.doubleDataMut()[1] = w2;
    return v;
}

} // namespace

Value lowpass(const Value &x, double fpass, double fs, int order, std::pmr::memory_resource *mr)
{
    validateOrder(order, "lowpass");
    const double Wp = normaliseW(fpass, fs, "lowpass");
    // honour explicit order; remap legacy default 8 -> 7 to match MATLAB
    const int N = (order == 8) ? kDefaultIirOrder : order;
    auto [b, a] = ellip(N, kDefaultRp, kDefaultRs, scalarWn(Wp, mr), FilterType::Lowpass, /*analog=*/false, mr);
    // SOS-form filtfilt is numerically stable for high-order IIR --
    // matches MATLAB filtfilt(d, x) for digitalFilter SOS objects.
    auto sos = tf2sos(b, a, mr);
    return sosfiltfilt(sos, x, mr);
}

Value highpass(const Value &x, double fpass, double fs, int order, std::pmr::memory_resource *mr)
{
    validateOrder(order, "highpass");
    const double Wp = normaliseW(fpass, fs, "highpass");
    const int N = (order == 8) ? kDefaultIirOrder : order;
    auto [b, a] = ellip(N, kDefaultRp, kDefaultRs, scalarWn(Wp, mr), FilterType::Highpass, /*analog=*/false, mr);
    auto sos = tf2sos(b, a, mr);
    return sosfiltfilt(sos, x, mr);
}

Value bandpass(const Value &x, double flo, double fhi, double fs, int order, std::pmr::memory_resource *mr)
{
    validateOrder(order, "bandpass");
    if (!(flo < fhi))
        throw Error("bandpass: low cutoff must be < high cutoff",
                     0, 0, "bandpass", "", "numkit:bandpass:badRange");
    const double Wlo = normaliseW(flo, fs, "bandpass");
    const double Whi = normaliseW(fhi, fs, "bandpass");
    const int N = (order == 8) ? kDefaultIirOrder : order;
    auto [b, a] = ellip(N, kDefaultRp, kDefaultRs, pairWn(Wlo, Whi, mr), FilterType::Bandpass, /*analog=*/false, mr);
    auto sos = tf2sos(b, a, mr);
    return sosfiltfilt(sos, x, mr);
}

Value bandstop(const Value &x, double flo, double fhi, double fs, int order, std::pmr::memory_resource *mr)
{
    validateOrder(order, "bandstop");
    if (!(flo < fhi))
        throw Error("bandstop: low cutoff must be < high cutoff",
                     0, 0, "bandstop", "", "numkit:bandstop:badRange");
    const double Wlo = normaliseW(flo, fs, "bandstop");
    const double Whi = normaliseW(fhi, fs, "bandstop");
    const int N = (order == 8) ? kDefaultIirOrder : order;
    auto [b, a] = ellip(N, kDefaultRp, kDefaultRs, pairWn(Wlo, Whi, mr), FilterType::Bandstop, /*analog=*/false, mr);
    auto sos = tf2sos(b, a, mr);
    return sosfiltfilt(sos, x, mr);
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
                     0, 0, "lowpass", "", "numkit:lowpass:nargin");
    const double fs = (args.size() >= 3) ? args[2].toScalar() : 2.0;
    const int order = (args.size() >= 4) ? static_cast<int>(args[3].toScalar()) : 8;
    outs[0] = lowpass(args[0], args[1].toScalar(), fs, order, ctx.engine->resource());
}

void highpass_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("highpass: requires (x, fpass[, fs])",
                     0, 0, "highpass", "", "numkit:highpass:nargin");
    const double fs = (args.size() >= 3) ? args[2].toScalar() : 2.0;
    const int order = (args.size() >= 4) ? static_cast<int>(args[3].toScalar()) : 8;
    outs[0] = highpass(args[0], args[1].toScalar(), fs, order, ctx.engine->resource());
}

void bandpass_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bandpass: requires (x, [flo fhi][, fs])",
                     0, 0, "bandpass", "", "numkit:bandpass:nargin");
    if (args[1].numel() != 2)
        throw Error("bandpass: cutoff must be a 2-element [flo fhi]",
                     0, 0, "bandpass", "", "numkit:bandpass:badCutoff");
    const double fs = (args.size() >= 3) ? args[2].toScalar() : 2.0;
    const int order = (args.size() >= 4) ? static_cast<int>(args[3].toScalar()) : 8;
    outs[0] = bandpass(args[0], args[1].elemAsDouble(0), args[1].elemAsDouble(1), fs, order, ctx.engine->resource());
}

void bandstop_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bandstop: requires (x, [flo fhi][, fs])",
                     0, 0, "bandstop", "", "numkit:bandstop:nargin");
    if (args[1].numel() != 2)
        throw Error("bandstop: cutoff must be a 2-element [flo fhi]",
                     0, 0, "bandstop", "", "numkit:bandstop:badCutoff");
    const double fs = (args.size() >= 3) ? args[2].toScalar() : 2.0;
    const int order = (args.size() >= 4) ? static_cast<int>(args[3].toScalar()) : 8;
    outs[0] = bandstop(args[0], args[1].elemAsDouble(0), args[1].elemAsDouble(1), fs, order, ctx.engine->resource());
    (void)scalarOr;   // silence unused-helper warning
}

} // namespace detail

} // namespace numkit::signal
