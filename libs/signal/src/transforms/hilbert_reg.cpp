// libs/signal/src/transforms/hilbert_reg.cpp
//
// CallContext register half of transforms/hilbert.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/interp/interp.hpp>     // interp1 (spline)
#include <numkit/signal/transforms/hilbert.hpp>
#include <numkit/signal/windows/windows.hpp>          // kaiser
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

void hilbert_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hilbert: requires 1 argument",
                     0, 0, "hilbert", "", "numkit:hilbert:nargin");
    outs[0] = hilbert(args[0], ctx.engine->resource());
}

void envelope_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("envelope: requires (x[, n[, method]])",
                     0, 0, "envelope", "", "numkit:envelope:nargin");
    auto *mr = ctx.engine->resource();
    int mode = 0;     // 0=default, 1=analytic, 2=rms, 3=peak
    size_t n = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        n = (size_t)args[1].toScalar();
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString())) {
        std::string m = args[2].toString();
        for (auto &c : m) c = (char)std::tolower((unsigned char)c);
        if      (m == "analytic")           mode = 1;
        else if (m == "rms")                mode = 2;
        else if (m == "peak" || m == "peaks") mode = 3;
        else throw Error("envelope: unknown method '" + m +
                         "' (expected 'analytic', 'rms', or 'peak')",
                         0, 0, "envelope", "", "numkit:envelope:badmethod");
    } else if (args.size() >= 2) {
        // n given but no method → MATLAB defaults to 'analytic'.
        mode = 1;
    }
    if ((mode == 1 || mode == 2 || mode == 3) && n == 0)
        throw Error("envelope: n must be a positive integer for "
                    "'analytic'/'rms'/'peak' modes",
                    0, 0, "envelope", "", "numkit:envelope:badn");
    auto [up, lo] = envelope_full(args[0], mode, n, mr);
    outs[0] = std::move(up);
    if (nargout > 1) outs[1] = std::move(lo);
}

} // namespace detail

} // namespace numkit::signal
