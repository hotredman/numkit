// libs/signal/src/spectral_analysis/periodogram_pwelch_reg.cpp
//
// CallContext register half of spectral_analysis/periodogram_pwelch.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/signal/spectral_analysis/periodogram_pwelch.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
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

// MATLAB DSP-spectrum default fs = 2*pi (treats input as normalised
// radian frequency). Explicit fs as the last positional argument
// overrides this.
constexpr double kDefaultFs = 2.0 * 3.14159265358979323846;

void periodogram_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("periodogram: requires at least 1 argument",
                     0, 0, "periodogram", "", "numkit:periodogram:nargin");

    Value window = Value();
    if (args.size() >= 2 && !args[1].isChar())
        window = args[1];
    const size_t nfft = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;
    const double fs   = (args.size() >= 4) ? args[3].toScalar() : kDefaultFs;

    auto [Pxx, F] = periodogram(args[0], window, nfft, fs, ctx.engine->resource());
    outs[0] = std::move(Pxx);
    if (nargout > 1)
        outs[1] = std::move(F);
}

void pwelch_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pwelch: requires at least 1 argument",
                     0, 0, "pwelch", "", "numkit:pwelch:nargin");

    Value window = Value();
    if (args.size() >= 2 && !args[1].isChar())
        window = args[1];
    // Empty [] placeholders select the default (MATLAB pwelch(x,[],[],nfft)).
    const size_t noverlap = (args.size() >= 3 && !args[2].isEmpty()) ? static_cast<size_t>(args[2].toScalar()) : 0;
    const size_t nfft = (args.size() >= 4 && !args[3].isEmpty()) ? static_cast<size_t>(args[3].toScalar()) : 0;
    const double fs   = (args.size() >= 5 && !args[4].isEmpty()) ? args[4].toScalar() : kDefaultFs;

    auto [Pxx, F] = pwelch(args[0], window, noverlap, nfft, fs, ctx.engine->resource());
    outs[0] = std::move(Pxx);
    if (nargout > 1)
        outs[1] = std::move(F);
}

void cpsd_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cpsd: requires (x, y[, window, noverlap, nfft, fs])",
                    0, 0, "cpsd", "", "numkit:cpsd:nargin");
    Value window = Value();
    if (args.size() >= 3 && !args[2].isChar()) window = args[2];
    const size_t noverlap = (args.size() >= 4 && !args[3].isEmpty()) ? static_cast<size_t>(args[3].toScalar()) : 0;
    const size_t nfft     = (args.size() >= 5 && !args[4].isEmpty()) ? static_cast<size_t>(args[4].toScalar()) : 0;
    const double fs       = (args.size() >= 6 && !args[5].isEmpty()) ? args[5].toScalar() : kDefaultFs;
    auto [Pxy, F] = cpsd(args[0], args[1], window, noverlap, nfft, fs, ctx.engine->resource());
    outs[0] = std::move(Pxy);
    if (nargout > 1) outs[1] = std::move(F);
}

void mscohere_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mscohere: requires (x, y[, window, noverlap, nfft, fs])",
                    0, 0, "mscohere", "", "numkit:mscohere:nargin");
    Value window = Value();
    if (args.size() >= 3 && !args[2].isChar()) window = args[2];
    const size_t noverlap = (args.size() >= 4 && !args[3].isEmpty()) ? static_cast<size_t>(args[3].toScalar()) : 0;
    const size_t nfft     = (args.size() >= 5 && !args[4].isEmpty()) ? static_cast<size_t>(args[4].toScalar()) : 0;
    const double fs       = (args.size() >= 6 && !args[5].isEmpty()) ? args[5].toScalar() : kDefaultFs;
    auto [Cxy, F] = mscohere(args[0], args[1], window, noverlap, nfft, fs, ctx.engine->resource());
    outs[0] = std::move(Cxy);
    if (nargout > 1) outs[1] = std::move(F);
}

void tfestimate_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tfestimate: requires (x, y[, window, noverlap, nfft, fs])",
                    0, 0, "tfestimate", "", "numkit:tfestimate:nargin");
    Value window = Value();
    if (args.size() >= 3 && !args[2].isChar()) window = args[2];
    const size_t noverlap = (args.size() >= 4 && !args[3].isEmpty()) ? static_cast<size_t>(args[3].toScalar()) : 0;
    const size_t nfft     = (args.size() >= 5 && !args[4].isEmpty()) ? static_cast<size_t>(args[4].toScalar()) : 0;
    const double fs       = (args.size() >= 6 && !args[5].isEmpty()) ? args[5].toScalar() : kDefaultFs;
    auto [Txy, F] = tfestimate(args[0], args[1], window, noverlap, nfft, fs, ctx.engine->resource());
    outs[0] = std::move(Txy);
    if (nargout > 1) outs[1] = std::move(F);
}

} // namespace detail

} // namespace numkit::signal
