// libs/control/src/analyze/analyze_reg.cpp
//
// Register half of the analysis builtins: the CallContext wrappers dcgain /
// margin / stepinfo that delegate to the engine-free compute in analyze.cpp.
// library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/control/analyze/analyze.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <utility>

namespace numkit::control {
namespace detail {

void dcgain_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("dcgain: requires sys",
                    0, 0, "dcgain", "", "numkit:dcgain:nargin");
    o[0] = dcgain(a[0], c.engine->resource());
}

void margin_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("margin: requires sys",
                    0, 0, "margin", "", "numkit:margin:nargin");
    auto m = margin(a[0], c.engine->resource());
    if (o.size() >= 1) o[0] = std::move(m.Gm);
    if (o.size() >= 2) o[1] = std::move(m.Pm);
    if (o.size() >= 3) o[2] = std::move(m.Wcg);
    if (o.size() >= 4) o[3] = std::move(m.Wcp);
}

void stepinfo_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("stepinfo: requires sys",
                    0, 0, "stepinfo", "", "numkit:stepinfo:nargin");
    o[0] = stepinfo(a[0], c.engine->resource());
}

} // namespace detail
} // namespace numkit::control
