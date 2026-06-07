// libs/wavelet/src/filter/families_reg.cpp
//
// Register half of the family-scaling filters: the CallContext builtins
// dbwavf / coifwavf / symwavf / orthfilt that delegate to the engine-free
// compute in families.cpp. argName lives here because it is a register-side
// Value→string parser (the compute entries take std::string / Value
// directly). library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/wavelet/filter/families.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <string>

namespace numkit::wavelet {

namespace {

std::string argName(const Value &v, const char *fn)
{
    if (!v.isChar() && !v.isString())
        throw Error(std::string(fn) + ": wavelet name must be a character vector",
                    0, 0, fn, "", "numkit:wavelet:type");
    return v.toString();
}

} // anonymous

namespace detail {

void dbwavf_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dbwavf: requires the wavelet name (e.g. 'db4')",
                    0, 0, "dbwavf", "", "numkit:dbwavf:nargin");
    outs[0] = dbwavf(argName(args[0], "dbwavf"), ctx.engine->resource());
}

void coifwavf_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("coifwavf: requires the wavelet name (e.g. 'coif1')",
                    0, 0, "coifwavf", "", "numkit:coifwavf:nargin");
    outs[0] = coifwavf(argName(args[0], "coifwavf"), ctx.engine->resource());
}

void symwavf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("symwavf: requires the wavelet name (e.g. 'sym4')",
                    0, 0, "symwavf", "", "numkit:symwavf:nargin");
    outs[0] = symwavf(argName(args[0], "symwavf"), ctx.engine->resource());
}

// [Lo_D, Hi_D, Lo_R, Hi_R] = orthfilt(W)
void orthfilt_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("orthfilt: requires a scaling filter W",
                    0, 0, "orthfilt", "", "numkit:orthfilt:nargin");
    OrthfiltResult r = orthfilt(args[0], ctx.engine->resource());
    if (outs.size() >= 1) outs[0] = r.Lo_D;
    if (outs.size() >= 2) outs[1] = r.Hi_D;
    if (outs.size() >= 3) outs[2] = r.Lo_R;
    if (outs.size() >= 4) outs[3] = r.Hi_R;
}

} // namespace detail
} // namespace numkit::wavelet
