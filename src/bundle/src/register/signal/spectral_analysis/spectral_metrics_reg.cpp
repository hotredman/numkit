// toolboxes/signal/src/spectral_analysis/spectral_metrics_reg.cpp
//
// CallContext register half of spectral_analysis/spectral_metrics.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/signal/spectral_analysis/periodogram_pwelch.hpp>
#include <numkit/signal/spectral_analysis/spectral_metrics.hpp>
#include <numkit/signal/transforms/hilbert.hpp>
#include <numkit/signal/windows/windows.hpp>
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

void bandpower_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bandpower: requires at least 1 argument",
                     0, 0, "bandpower", "", "numkit:bandpower:nargin");
    const Value &fs = (args.size() >= 2) ? args[1] : Value::Empty;
    const Value &fr = (args.size() >= 3) ? args[2] : Value::Empty;
    outs[0] = bandpower(args[0], fs, fr, ctx.engine->resource());
}

void obw_reg(Span<const Value> args, size_t /*nargout*/,
             Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("obw: requires at least 1 argument",
                     0, 0, "obw", "", "numkit:obw:nargin");
    const Value &fs = (args.size() >= 2) ? args[1] : Value::Empty;
    double p = 0.99;
    if (args.size() >= 3 && !args[2].isEmpty()) p = args[2].toScalar();
    outs[0] = obw(args[0], fs, p, ctx.engine->resource());
}

#define NK_SPEC1_REG(name, fn)                                                  \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires at least 1 argument",                 \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const Value &fs = (args.size() >= 2) ? args[1] : Value::Empty;          \
        outs[0] = fn(args[0], fs, ctx.engine->resource());                      \
    }

NK_SPEC1_REG(meanfreq,         meanfreq)
NK_SPEC1_REG(medfreq,          medfreq)
NK_SPEC1_REG(enbw,             enbw)
NK_SPEC1_REG(powerbw,          powerbw)
NK_SPEC1_REG(spectralcrest,    spectralcrest)
NK_SPEC1_REG(spectralflatness, spectralflatness)
NK_SPEC1_REG(spectralentropy,  spectralentropy)
NK_SPEC1_REG(spectralkurtosis, spectralkurtosis)
NK_SPEC1_REG(spectralskewness, spectralskewness)
NK_SPEC1_REG(snr,              snr)
NK_SPEC1_REG(sinad,            sinad)
NK_SPEC1_REG(thd,              thd)
NK_SPEC1_REG(sfdr,             sfdr)
NK_SPEC1_REG(instfreq,         instfreq)
NK_SPEC1_REG(instbw,           instbw)

#undef NK_SPEC1_REG

} // namespace detail

} // namespace numkit::signal
