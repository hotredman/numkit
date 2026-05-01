// libs/signal/src/digital_filtering/spec_driven.cpp
//
// lowpass / highpass / bandpass / bandstop — wrappers over butter() +
// filtfilt(). Bandpass / bandstop are cascade approximations (the
// scalar-Wn butter() doesn't accept the [w1 w2] form yet).

#include <numkit/signal/digital_filtering/spec_driven.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/signal/digital_filtering/filter.hpp>
#include <numkit/signal/filter_design/filter_design.hpp>

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

} // namespace

Value lowpass(std::pmr::memory_resource *mr, const Value &x,
              double fpass, double fs, int order)
{
    validateOrder(order, "lowpass");
    const double Wn = normaliseW(fpass, fs, "lowpass");
    auto [b, a] = butter(mr, order, Wn, "low");
    return filtfilt(mr, b, a, x);
}

Value highpass(std::pmr::memory_resource *mr, const Value &x,
               double fpass, double fs, int order)
{
    validateOrder(order, "highpass");
    const double Wn = normaliseW(fpass, fs, "highpass");
    auto [b, a] = butter(mr, order, Wn, "high");
    return filtfilt(mr, b, a, x);
}

Value bandpass(std::pmr::memory_resource *mr, const Value &x,
               double flo, double fhi, double fs, int order)
{
    validateOrder(order, "bandpass");
    if (!(flo < fhi))
        throw Error("bandpass: low cutoff must be < high cutoff",
                     0, 0, "bandpass", "", "m:bandpass:badRange");
    auto stage1 = highpass(mr, x, flo, fs, order);
    return lowpass(mr, stage1, fhi, fs, order);
}

Value bandstop(std::pmr::memory_resource *mr, const Value &x,
               double flo, double fhi, double fs, int order)
{
    validateOrder(order, "bandstop");
    if (!(flo < fhi))
        throw Error("bandstop: low cutoff must be < high cutoff",
                     0, 0, "bandstop", "", "m:bandstop:badRange");
    // Stop = low pass below flo + high pass above fhi.
    auto lo = lowpass(mr, x, flo, fs, order);
    auto hi = highpass(mr, x, fhi, fs, order);
    // Sum the two paths element-wise.
    auto out = Value::matrix(x.dims().rows(), x.dims().cols(),
                              ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const double *l = lo.doubleData();
    const double *h = hi.doubleData();
    const size_t n = x.numel();
    for (size_t i = 0; i < n; ++i) dst[i] = l[i] + h[i];
    return out;
}

namespace detail {

static double scalarOr(const Value &v, double dflt) {
    return v.numel() ? v.toScalar() : dflt;
}

void lowpass_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("lowpass: requires (x, fpass, fs)",
                     0, 0, "lowpass", "", "m:lowpass:nargin");
    const int order = (args.size() >= 4) ? static_cast<int>(args[3].toScalar()) : 8;
    outs[0] = lowpass(ctx.engine->resource(), args[0],
                      args[1].toScalar(), args[2].toScalar(), order);
}

void highpass_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("highpass: requires (x, fpass, fs)",
                     0, 0, "highpass", "", "m:highpass:nargin");
    const int order = (args.size() >= 4) ? static_cast<int>(args[3].toScalar()) : 8;
    outs[0] = highpass(ctx.engine->resource(), args[0],
                       args[1].toScalar(), args[2].toScalar(), order);
}

void bandpass_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("bandpass: requires (x, [flo fhi], fs)",
                     0, 0, "bandpass", "", "m:bandpass:nargin");
    if (args[1].numel() != 2)
        throw Error("bandpass: cutoff must be a 2-element [flo fhi]",
                     0, 0, "bandpass", "", "m:bandpass:badCutoff");
    const int order = (args.size() >= 4) ? static_cast<int>(args[3].toScalar()) : 8;
    outs[0] = bandpass(ctx.engine->resource(), args[0],
                       args[1].elemAsDouble(0), args[1].elemAsDouble(1),
                       args[2].toScalar(), order);
}

void bandstop_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("bandstop: requires (x, [flo fhi], fs)",
                     0, 0, "bandstop", "", "m:bandstop:nargin");
    if (args[1].numel() != 2)
        throw Error("bandstop: cutoff must be a 2-element [flo fhi]",
                     0, 0, "bandstop", "", "m:bandstop:badCutoff");
    const int order = (args.size() >= 4) ? static_cast<int>(args[3].toScalar()) : 8;
    outs[0] = bandstop(ctx.engine->resource(), args[0],
                       args[1].elemAsDouble(0), args[1].elemAsDouble(1),
                       args[2].toScalar(), order);
    (void)scalarOr;   // silence unused-helper warning
}

} // namespace detail

} // namespace numkit::signal
