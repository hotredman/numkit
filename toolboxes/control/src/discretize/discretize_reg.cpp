// toolboxes/control/src/discretize/discretize_reg.cpp
//
// Register half of the discretisation builtins: the CallContext wrappers
// c2d / d2c that delegate to the engine-free compute in discretize.cpp.
// library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/control/discretize/discretize.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <string>

namespace numkit::control {
namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("control discretize: expected a string method",
                    0, 0, "discretize", "", "numkit:discretize:type");
    return v.toString();
}

void c2d_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("c2d: requires (sys, Ts [, method])",
                    0, 0, "c2d", "", "numkit:c2d:nargin");
    std::string method;
    if (a.size() >= 3 && !a[2].isEmpty()) method = argString(a[2]);
    o[0] = c2d(a[0], a[1].toScalar(), method, c.engine->resource());
}

void d2c_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("d2c: requires (sys [, method])",
                    0, 0, "d2c", "", "numkit:d2c:nargin");
    std::string method;
    if (a.size() >= 2 && !a[1].isEmpty()) method = argString(a[1]);
    o[0] = d2c(a[0], method, c.engine->resource());
}

} // namespace detail
} // namespace numkit::control
