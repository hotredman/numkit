// libs/linalg/src/predicates_reg.cpp
//
// Register half of the matrix-predicate builtins: the CallContext wrappers
// issymmetric / ishermitian / isbanded / isdiag / istril / istriu /
// bandwidth that delegate to the engine-free compute in predicates.cpp.
// The register-side option parser parseSkewOpt lives here too. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/linalg/predicates.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <string>

namespace numkit::linalg {
namespace detail {

static bool parseSkewOpt(const Value &v, const char *who)
{
    if (!v.isChar() && !v.isString())
        throw Error(std::string(who) + ": option must be 'skew' or 'nonskew'",
                    0, 0, who, "", std::string("numkit:") + who + ":BadOpt");
    std::string s = v.toString();
    if (s == "skew")     return true;
    if (s == "nonskew")  return false;
    throw Error(std::string(who) + ": option must be 'skew' or 'nonskew'",
                0, 0, who, "", std::string("numkit:") + who + ":BadOpt");
}

void issymmetric_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("issymmetric: requires (A)",
                    0, 0, "issymmetric", "", "numkit:issymmetric:nargin");
    bool skew = (args.size() >= 2) && parseSkewOpt(args[1], "issymmetric");
    outs[0] = issymmetric(args[0], skew, ctx.engine->resource());
}

void ishermitian_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ishermitian: requires (A)",
                    0, 0, "ishermitian", "", "numkit:ishermitian:nargin");
    bool skew = (args.size() >= 2) && parseSkewOpt(args[1], "ishermitian");
    outs[0] = ishermitian(args[0], skew, ctx.engine->resource());
}

void isbanded_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("isbanded: requires (A, lower, upper)",
                    0, 0, "isbanded", "", "numkit:isbanded:nargin");
    long lower = static_cast<long>(args[1].toScalar());
    long upper = static_cast<long>(args[2].toScalar());
    outs[0] = isbanded(args[0], lower, upper, ctx.engine->resource());
}

void isdiag_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isdiag: requires (A)", 0, 0, "isdiag", "", "numkit:isdiag:nargin");
    outs[0] = isdiag(args[0], ctx.engine->resource());
}

void istril_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("istril: requires (A)", 0, 0, "istril", "", "numkit:istril:nargin");
    outs[0] = istril(args[0], ctx.engine->resource());
}

void istriu_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("istriu: requires (A)", 0, 0, "istriu", "", "numkit:istriu:nargin");
    outs[0] = istriu(args[0], ctx.engine->resource());
}

void bandwidth_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bandwidth: requires (A) or (A, opt)",
                    0, 0, "bandwidth", "", "numkit:bandwidth:nargin");
    if (args.size() == 1) {
        auto [lo, up] = bandwidth(args[0], ctx.engine->resource());
        outs[0] = lo;
        if (nargout >= 2 && outs.size() >= 2) outs[1] = up;
        return;
    }
    if (!args[1].isChar() && !args[1].isString())
        throw Error("bandwidth: option must be 'lower' or 'upper'",
                    0, 0, "bandwidth", "", "numkit:bandwidth:BadOpt");
    outs[0] = bandwidthOpt(args[0], args[1].toString(), ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::linalg
