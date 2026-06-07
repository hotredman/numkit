// libs/signal/src/convolution/extras_reg.cpp
//
// Register half of the signal extras builtins: the CallContext wrappers
// delegating to the engine-free compute in extras.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/signal/convolution/extras.hpp>

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

void cconv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cconv: requires (x, y[, n])",
                     0, 0, "cconv", "", "numkit:cconv:nargin");
    const size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;
    outs[0] = cconv(args[0], args[1], n, ctx.engine->resource());
}

void convmtx_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("convmtx: requires (h, n)",
                     0, 0, "convmtx", "", "numkit:convmtx:nargin");
    outs[0] = convmtx(args[0], static_cast<size_t>(args[1].toScalar()), ctx.engine->resource());
}

void xcorr2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("xcorr2: requires at least 1 argument",
                     0, 0, "xcorr2", "", "numkit:xcorr2:nargin");
    const Value &A = args[0];
    const Value &B = (args.size() >= 2) ? args[1] : args[0];
    outs[0] = xcorr2(A, B, ctx.engine->resource());
}

void finddelay_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("finddelay: requires (x, y[, max_lag])",
                     0, 0, "finddelay", "", "numkit:finddelay:nargin");
    const long max_lag = (args.size() >= 3) ? static_cast<long>(args[2].toScalar()) : 0;
    const long d = finddelay(args[0], args[1], max_lag, ctx.engine->resource());
    outs[0] = Value::scalar(static_cast<double>(d), ctx.engine->resource());
}

void alignsignals_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("alignsignals: requires (x, y[, max_lag])",
                     0, 0, "alignsignals", "", "numkit:alignsignals:nargin");
    const long max_lag = (args.size() >= 3) ? static_cast<long>(args[2].toScalar()) : 0;
    auto [xa, ya] = alignsignals(args[0], args[1], max_lag, ctx.engine->resource());
    outs[0] = std::move(xa);
    if (nargout > 1) outs[1] = std::move(ya);
}

} // namespace detail

} // namespace numkit::signal
