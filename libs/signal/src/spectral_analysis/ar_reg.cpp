// libs/signal/src/spectral_analysis/ar_reg.cpp
//
// CallContext register half of spectral_analysis/ar.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/signal/spectral_analysis/periodogram_pwelch.hpp>
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

void pyulear_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pyulear: requires (x, p[, nfft])",
                    0, 0, "pyulear", "", "numkit:pyulear:nargin");
    const int p     = static_cast<int>(args[1].toScalar());
    const size_t nf = (args.size() >= 3 && !args[2].isEmpty())
                      ? static_cast<size_t>(args[2].toScalar()) : 0;
    auto [Pxx, F] = pyulear(args[0], p, nf, ctx.engine->resource());
    outs[0] = std::move(Pxx);
    if (nargout > 1) outs[1] = std::move(F);
}

void pburg_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pburg: requires (x, p[, nfft])",
                    0, 0, "pburg", "", "numkit:pburg:nargin");
    const int p     = static_cast<int>(args[1].toScalar());
    const size_t nf = (args.size() >= 3 && !args[2].isEmpty())
                      ? static_cast<size_t>(args[2].toScalar()) : 0;
    auto [Pxx, F] = pburg(args[0], p, nf, ctx.engine->resource());
    outs[0] = std::move(Pxx);
    if (nargout > 1) outs[1] = std::move(F);
}

// aryule_reg / lpc_reg live in signal_modeling.cpp.

} // namespace detail

} // namespace numkit::signal
