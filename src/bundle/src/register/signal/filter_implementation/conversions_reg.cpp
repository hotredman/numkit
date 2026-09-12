// toolboxes/signal/src/filter_implementation/conversions_reg.cpp
//
// Register half of the signal conversions builtins: the CallContext wrappers
// delegating to the engine-free compute in conversions.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <nk/signal/filter_implementation/conversions.hpp>

#include <nk/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <nk/value/value.hpp>
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/span.hpp>

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

namespace nk::signal {

namespace detail {

void zp2sos_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args.size() > 3)
        throw Error("zp2sos: requires (z, p[, k])",
                     0, 0, "zp2sos", "", "nk:zp2sos:nargin");
    const double gain = (args.size() >= 3 && !args[2].isEmpty())
                            ? args[2].toScalar()
                            : 1.0;
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [sos, g] = zp2sosWithGain(args[0], args[1], gain, mr);
        outs[0] = std::move(sos);
        outs[1] = Value::scalar(g, mr);
    } else {
        outs[0] = zp2sos(args[0], args[1], gain, mr);
    }
}

void tf2sos_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 2)
        throw Error("tf2sos: requires (b, a)",
                     0, 0, "tf2sos", "", "nk:tf2sos:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [sos, g] = tf2sosWithGain(args[0], args[1], mr);
        outs[0] = std::move(sos);
        outs[1] = Value::scalar(g, mr);
    } else {
        outs[0] = tf2sos(args[0], args[1], mr);
    }
}

} // namespace detail

} // namespace nk::signal
