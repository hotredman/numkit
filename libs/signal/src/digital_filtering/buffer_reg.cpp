// libs/signal/src/digital_filtering/buffer_reg.cpp
//
// Register half of the signal buffer builtins: the CallContext wrappers
// delegating to the engine-free compute in buffer.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/signal/digital_filtering/buffer.hpp>

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

void buffer_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("buffer: requires (x, n [, p [, opt]])",
                    0, 0, "buffer", "", "numkit:buffer:nargin");
    const int n = static_cast<int>(args[1].toScalar());
    int p = 0;
    if (args.size() >= 3 && !args[2].isEmpty()) p = static_cast<int>(args[2].toScalar());
    const Value &opt = (args.size() >= 4) ? args[3] : Value::Empty;
    if (nargout >= 2 && outs.size() >= 2) {
        auto [Y, Z] = buffer2(args[0], n, p, opt, ctx.engine->resource());
        outs[0] = Y;
        outs[1] = Z;
    } else {
        outs[0] = buffer(args[0], n, p, opt, ctx.engine->resource());
    }
}

} // namespace detail

} // namespace numkit::signal
