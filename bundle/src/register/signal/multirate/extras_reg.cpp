// toolboxes/signal/src/multirate/extras_reg.cpp
//
// Register half of the signal extras builtins: the CallContext wrappers
// delegating to the engine-free compute in extras.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/signal/multirate/extras.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

namespace detail {

void upfirdn_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("upfirdn: requires (x, h, p[, q])",
                     0, 0, "upfirdn", "", "numkit:upfirdn:nargin");
    const size_t p = static_cast<size_t>(args[2].toScalar());
    const size_t q = (args.size() >= 4) ? static_cast<size_t>(args[3].toScalar()) : 1;
    outs[0] = upfirdn(args[0], args[1], p, q, ctx.engine->resource());
}

void interp_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("interp: requires (x, r[, n[, alpha]])",
                     0, 0, "interp", "", "numkit:interp:nargin");
    const size_t r = static_cast<size_t>(args[1].toScalar());
    const size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 4;
    const double alpha = (args.size() >= 4) ? args[3].toScalar() : 0.5;
    outs[0] = interp(args[0], r, n, alpha, ctx.engine->resource());
}

void intfilt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1)
        throw Error("intfilt: requires (r[, n[, alpha]])",
                     0, 0, "intfilt", "", "numkit:intfilt:nargin");
    const size_t r = static_cast<size_t>(args[0].toScalar());
    const size_t n = (args.size() >= 2) ? static_cast<size_t>(args[1].toScalar()) : 4;
    const double alpha = (args.size() >= 3) ? args[2].toScalar() : 0.5;
    outs[0] = intfilt(r, n, alpha, ctx.engine->resource());
}

void fftfilt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fftfilt: requires (b, x[, nfft])",
                     0, 0, "fftfilt", "", "numkit:fftfilt:nargin");
    const size_t nfft = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;
    outs[0] = fftfilt(args[0], args[1], nfft, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
