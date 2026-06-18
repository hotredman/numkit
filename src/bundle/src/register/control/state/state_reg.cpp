// toolboxes/control/src/state/state_reg.cpp
//
// Register half of the controllability/observability builtins: the
// CallContext wrappers ctrb / obsv that delegate to the engine-free
// compute in state.cpp. library.cpp forward-declares + registers these
// by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/control/state/state.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

namespace numkit::control {
namespace detail {

void ctrb_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("ctrb: requires (A, B) or (sys)",
                    0, 0, "ctrb", "", "numkit:ctrb:nargin");
    auto *mr = c.engine->resource();
    if (a.size() == 1) {
        // Single-arg form: must be a sys struct.
        o[0] = ctrb_sys(a[0], mr);
        return;
    }
    o[0] = ctrb_AB(a[0], a[1], mr);
}

void obsv_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("obsv: requires (A, C) or (sys)",
                    0, 0, "obsv", "", "numkit:obsv:nargin");
    auto *mr = c.engine->resource();
    if (a.size() == 1) {
        o[0] = obsv_sys(a[0], mr);
        return;
    }
    o[0] = obsv_AC(a[0], a[1], mr);
}

void gram_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("gram: requires (sys, type) where type is 'c' or 'o'",
                    0, 0, "gram", "", "numkit:gram:nargin");
    o[0] = gram(a[0], a[1].toString(), c.engine->resource());
}

} // namespace detail
} // namespace numkit::control
