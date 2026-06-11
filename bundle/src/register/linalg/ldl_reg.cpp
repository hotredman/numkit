// toolboxes/linalg/src/ldl_reg.cpp
//
// Register half of the ldl builtin: the CallContext wrapper that parses the
// lower/upper/matrix/vector options and delegates to the engine-free ldl
// compute in ldl.cpp. library.cpp forward-declares + registers this by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/linalg/ldl.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <string>

namespace numkit::linalg {
namespace detail {

void ldl_reg(Span<const Value> args, size_t nargout,
             Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ldl: requires (A [, opt...])",
                    0, 0, "ldl", "", "numkit:ldl:nargin");

    bool upper = false;
    bool vec_perm = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("ldl: optional args must be 'lower'/'upper'/'matrix'/'vector'",
                        0, 0, "ldl", "", "numkit:ldl:BadOpt");
        std::string s = args[i].toString();
        if      (s == "lower")  upper = false;
        else if (s == "upper")  upper = true;
        else if (s == "matrix") vec_perm = false;
        else if (s == "vector") vec_perm = true;
        else
            throw Error("ldl: unknown option '" + s + "'",
                        0, 0, "ldl", "", "numkit:ldl:BadOpt");
    }

    auto [L, D, P] = ldl(args[0], upper, vec_perm, ctx.engine->resource());
    outs[0] = L;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = D;
    if (nargout >= 3 && outs.size() >= 3) outs[2] = P;
}

} // namespace detail
} // namespace numkit::linalg
