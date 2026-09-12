// toolboxes/control/src/state/place_reg.cpp
//
// Register half of the pole-placement builtins: the CallContext wrappers
// acker / place that delegate to the engine-free compute in place.cpp.
// library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <nk/control/state/place.hpp>

#include <nk/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <nk/value/error.hpp>

namespace nk::control {
namespace detail {

void acker_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 3)
        throw Error("acker: requires (A, B, p)",
                    0, 0, "acker", "", "nk:acker:nargin");
    o[0] = acker(a[0], a[1], a[2], c.engine->resource());
}

void place_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 3)
        throw Error("place: requires (A, B, p)",
                    0, 0, "place", "", "nk:place:nargin");
    o[0] = place(a[0], a[1], a[2], c.engine->resource());
}

} // namespace detail
} // namespace nk::control
