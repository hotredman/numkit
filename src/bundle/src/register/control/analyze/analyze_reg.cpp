// toolboxes/control/src/analyze/analyze_reg.cpp
//
// Register half of the analysis builtins: the CallContext wrappers dcgain /
// margin / stepinfo that delegate to the engine-free compute in analyze.cpp.
// library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <nk/control/analyze/analyze.hpp>

#include <nk/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <nk/value/error.hpp>

#include <utility>

namespace nk::control {
namespace detail {

void dcgain_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("dcgain: requires sys",
                    0, 0, "dcgain", "", "nk:dcgain:nargin");
    o[0] = dcgain(a[0], c.engine->resource());
}

void margin_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("margin: requires sys",
                    0, 0, "margin", "", "nk:margin:nargin");
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
                    0, 0, "stepinfo", "", "nk:stepinfo:nargin");
    o[0] = stepinfo(a[0], c.engine->resource());
}

void hinfnorm_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("hinfnorm: requires sys",
                    0, 0, "hinfnorm", "", "nk:hinfnorm:nargin");
    o[0] = hinfnorm(a[0], c.engine->resource());
}

void allmargin_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("allmargin: requires sys",
                    0, 0, "allmargin", "", "nk:allmargin:nargin");
    o[0] = allmargin(a[0], c.engine->resource());
}

} // namespace detail
} // namespace nk::control
