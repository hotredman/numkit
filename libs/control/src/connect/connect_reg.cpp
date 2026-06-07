// libs/control/src/connect/connect_reg.cpp
//
// Register half of the interconnection builtins: the CallContext wrappers
// series / parallel / feedback that delegate to the engine-free compute in
// connect.cpp. library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/control/connect/connect.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

namespace numkit::control {
namespace detail {

void series_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
    if (a.size() < 2)
        throw Error("series: requires (sys1, sys2)",
                    0, 0, "series", "", "numkit:series:nargin");
    o[0] = series(a[0], a[1], c.engine->resource());
}

void parallel_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
    if (a.size() < 2)
        throw Error("parallel: requires (sys1, sys2)",
                    0, 0, "parallel", "", "numkit:parallel:nargin");
    o[0] = parallel(a[0], a[1], c.engine->resource());
}

void feedback_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
    if (a.size() < 2)
        throw Error("feedback: requires (G, H [, sign])",
                    0, 0, "feedback", "", "numkit:feedback:nargin");
    int sign = -1;
    if (a.size() >= 3 && !a[2].isEmpty())
        sign = static_cast<int>(a[2].toScalar());
    o[0] = feedback(a[0], a[1], sign, c.engine->resource());
}

} // namespace detail
} // namespace numkit::control
