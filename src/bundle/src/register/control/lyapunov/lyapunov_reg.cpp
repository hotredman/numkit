// toolboxes/control/src/lyapunov/lyapunov_reg.cpp
//
// Register half of the Lyapunov solvers: the CallContext builtins lyap /
// dlyap that delegate to the engine-free compute in lyapunov.cpp.
// library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <nk/control/lyapunov/lyapunov.hpp>

#include <nk/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <nk/value/error.hpp>

namespace nk::control {
namespace detail {

void lyap_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("lyap: requires (A, Q)",
                    0, 0, "lyap", "", "nk:lyap:nargin");
    o[0] = lyap(a[0], a[1], c.engine->resource());
}

void dlyap_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("dlyap: requires (A, Q)",
                    0, 0, "dlyap", "", "nk:dlyap:nargin");
    o[0] = dlyap(a[0], a[1], c.engine->resource());
}

} // namespace detail
} // namespace nk::control
