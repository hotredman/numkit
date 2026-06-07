// libs/signal/src/filter_analysis/responses_reg.cpp
//
// Register half of the signal responses builtins: the CallContext wrappers
// delegating to the engine-free compute in responses.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/signal/filter_analysis/responses.hpp>

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

void impz_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("impz: requires at least 1 argument (b)",
                     0, 0, "impz", "", "numkit:impz:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;
    auto [h, t] = impz(b, a, n, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(t);
}

void impzlength_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("impzlength: requires at least 1 argument (b)",
                     0, 0, "impzlength", "", "numkit:impzlength:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    const size_t n = impzlength(b, a, ctx.engine->resource());
    outs[0] = Value::scalar(static_cast<double>(n), ctx.engine->resource());
}

void stepz_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("stepz: requires at least 1 argument (b)",
                     0, 0, "stepz", "", "numkit:stepz:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;
    auto [s, t] = stepz(b, a, n, ctx.engine->resource());
    outs[0] = std::move(s);
    if (nargout > 1) outs[1] = std::move(t);
}

void phasedelay_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("phasedelay: requires at least 1 argument (b)",
                     0, 0, "phasedelay", "", "numkit:phasedelay:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 512;
    auto [pd, w] = phasedelay(b, a, n, ctx.engine->resource());
    outs[0] = std::move(pd);
    if (nargout > 1) outs[1] = std::move(w);
}

void zerophase_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("zerophase: requires at least 1 argument (b)",
                     0, 0, "zerophase", "", "numkit:zerophase:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 512;
    auto [Hr, w] = zerophase(b, a, n, ctx.engine->resource());
    outs[0] = std::move(Hr);
    if (nargout > 1) outs[1] = std::move(w);
}

} // namespace detail

} // namespace numkit::signal
