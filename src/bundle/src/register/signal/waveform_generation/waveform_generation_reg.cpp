// toolboxes/signal/src/waveform_generation/waveform_generation_reg.cpp
//
// CallContext register half of waveform_generation/waveform_generation.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/signal/digital_filtering/filter.hpp>        // filtfilt
#include <numkit/signal/filter_design/filter_design.hpp>     // butter
#include <numkit/signal/transforms/hilbert.hpp>              // hilbert
#include <numkit/signal/waveform_generation/waveform_generation.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

namespace detail {

void demod_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("demod: requires (y, Fc, Fs, method [, opt])",
                    0, 0, "demod", "", "numkit:demod:nargin");
    const double Fc = args[1].toScalar();
    const double Fs = args[2].toScalar();
    if (!args[3].isChar() && !args[3].isString())
        throw Error("demod: method must be a string",
                    0, 0, "demod", "", "numkit:demod:BadMethodType");
    std::string method = args[3].toString();
    const Value &opt = (args.size() >= 5) ? args[4] : Value::Empty;
    outs[0] = demod(args[0], Fc, Fs, method, opt, ctx.engine->resource());
}

void modulate_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("modulate: requires (x, Fc, Fs, method [, opt])",
                    0, 0, "modulate", "", "numkit:modulate:nargin");
    const double Fc = args[1].toScalar();
    const double Fs = args[2].toScalar();
    if (!args[3].isChar() && !args[3].isString())
        throw Error("modulate: method must be a string",
                    0, 0, "modulate", "", "numkit:modulate:BadMethodType");
    std::string method = args[3].toString();
    const Value &opt = (args.size() >= 5) ? args[4] : Value::Empty;
    outs[0] = modulate(args[0], Fc, Fs, method, opt, ctx.engine->resource());
}

void vco_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("vco: requires (x [, range [, fs]])",
                    0, 0, "vco", "", "numkit:vco:nargin");
    auto *mr = ctx.engine->resource();
    double fs = 1.0;
    if (args.size() >= 3 && !args[2].isEmpty()) fs = args[2].toScalar();
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &rng = args[1];
        if (rng.numel() == 1)
            outs[0] = vco(args[0], rng.toScalar(), fs, mr);
        else if (rng.numel() == 2)
            outs[0] = vco(args[0], rng.elemAsDouble(0), rng.elemAsDouble(1),
                          fs, mr);
        else
            throw Error("vco: range must be scalar Fc or [Fmin Fmax]",
                        0, 0, "vco", "", "numkit:vco:BadRange");
    } else {
        outs[0] = vco(args[0], fs / 4.0, fs, mr);
    }
}

void rectpuls_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rectpuls: requires at least 1 argument",
                     0, 0, "rectpuls", "", "numkit:rectpuls:nargin");
    const double w = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    outs[0] = rectpuls(args[0], w, ctx.engine->resource());
}

void tripuls_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("tripuls: requires at least 1 argument",
                     0, 0, "tripuls", "", "numkit:tripuls:nargin");
    const double w = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    outs[0] = tripuls(args[0], w, ctx.engine->resource());
}

void gauspuls_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("gauspuls: requires at least 2 arguments (t, fc)",
                     0, 0, "gauspuls", "", "numkit:gauspuls:nargin");
    const double fc = args[1].toScalar();
    const double bw = (args.size() >= 3) ? args[2].toScalar() : 0.5;
    outs[0] = gauspuls(args[0], fc, bw, ctx.engine->resource());
}

void pulstran_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("pulstran: requires at least 3 arguments (t, d, fn)",
                     0, 0, "pulstran", "", "numkit:pulstran:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args[2].isFuncHandle()) {
        const auto &handle = args[2];
        auto cb = [&ctx, &handle](Span<const Value> ar, Span<Value> ou,
                                   std::pmr::memory_resource * /*mr*/) {
            auto r = ctx.engine->callFunctionHandleMulti(handle, ar, ou.size());
            for (size_t i = 0; i < ou.size() && i < r.size(); ++i)
                ou[i] = std::move(r[i]);
        };
        // Extract t and d into double buffers; library takes Span.
        const Value &tv = args[0];
        const Value &dv = args[1];
        const size_t nt = tv.numel();
        const size_t nd = dv.numel();
        ScratchArena scratch(mr);
        ScratchVec<double> tbuf(nt, &scratch);
        ScratchVec<double> dbuf(nd, &scratch);
        for (size_t i = 0; i < nt; ++i) tbuf[i] = tv.elemAsDouble(i);
        for (size_t i = 0; i < nd; ++i) dbuf[i] = dv.elemAsDouble(i);

        Value r = pulstranHandle(
            Span<const double>(tbuf.data(), nt),
            Span<const double>(dbuf.data(), nd),
            cb, mr);

        // MATLAB convention: output shape mirrors t's shape.
        // Library returns a column; if t was a row, reshape.
        if (tv.dims().rows() == 1 && tv.dims().cols() >= 1) {
            Value row = Value::matrix(1, nt, ValueType::DOUBLE, mr);
            for (size_t i = 0; i < nt; ++i)
                row.doubleDataMut()[i] = r.doubleData()[i];
            r = std::move(row);
        }
        outs[0] = std::move(r);
        return;
    }
    if (!args[2].isChar() && !args[2].isString())
        throw Error("pulstran: 3rd argument must be a string name or a function handle",
                     0, 0, "pulstran", "", "numkit:pulstran:fnType");
    const std::string fnName = args[2].toString();
    const double fcOrW = (args.size() >= 4) ? args[3].toScalar() : 1.0;
    const double bw    = (args.size() >= 5) ? args[4].toScalar() : 0.5;
    outs[0] = pulstran(args[0], args[1], fnName, fcOrW, bw, mr);
}

void square_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("square: requires at least 1 argument",
                     0, 0, "square", "", "numkit:square:nargin");
    const double duty = (args.size() >= 2) ? args[1].toScalar() : 50.0;
    outs[0] = square(args[0], duty, ctx.engine->resource());
}

void sawtooth_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sawtooth: requires at least 1 argument",
                     0, 0, "sawtooth", "", "numkit:sawtooth:nargin");
    const double width = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    outs[0] = sawtooth(args[0], width, ctx.engine->resource());
}

void sinc_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sinc: requires 1 argument",
                     0, 0, "sinc", "", "numkit:sinc:nargin");
    outs[0] = sinc(args[0], ctx.engine->resource());
}

void gmonopuls_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("gmonopuls: requires 2 arguments (t, fc)",
                     0, 0, "gmonopuls", "", "numkit:gmonopuls:nargin");
    outs[0] = gmonopuls(args[0], args[1].toScalar(), ctx.engine->resource());
}

void diric_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("diric: requires 2 arguments (x, n)",
                     0, 0, "diric", "", "numkit:diric:nargin");
    const int n = static_cast<int>(args[1].toScalar());
    outs[0] = diric(args[0], n, ctx.engine->resource());
}

void chirp_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("chirp: requires at least 4 arguments (t, f0, t1, f1)",
                     0, 0, "chirp", "", "numkit:chirp:nargin");
    const double f0 = args[1].toScalar();
    const double t1 = args[2].toScalar();
    const double f1 = args[3].toScalar();
    std::string method = "linear";
    if (args.size() >= 5) {
        if (!args[4].isChar() && !args[4].isString())
            throw Error("chirp: method must be a string",
                         0, 0, "chirp", "", "numkit:chirp:badMethodType");
        method = args[4].toString();
    }
    outs[0] = chirp(args[0], f0, t1, f1, method, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
