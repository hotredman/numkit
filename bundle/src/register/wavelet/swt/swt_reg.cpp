// toolboxes/wavelet/src/swt/swt_reg.cpp
//
// Register half of the stationary / maximal-overlap transforms: the
// CallContext builtins swt / iswt / modwt / imodwt (argument parsing,
// modwt default wname/level inference) that delegate to the engine-free
// compute in swt.cpp. library.cpp forward-declares + registers these.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/wavelet/swt/swt.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <cmath>
#include <string>

namespace numkit::wavelet {
namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("wavelet: expected string argument",
                    0, 0, "", "", "numkit:wavelet:type");
    return v.toString();
}

void swt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("swt: requires (x, n, wname)",
                    0, 0, "swt", "", "numkit:swt:nargin");
    outs[0] = swt(args[0], static_cast<int>(args[1].toScalar()), argString(args[2]), ctx.engine->resource());
}

void iswt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("iswt: requires (swc, wname)",
                    0, 0, "iswt", "", "numkit:iswt:nargin");
    outs[0] = iswt(args[0], argString(args[1]), ctx.engine->resource());
}

void modwt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    // MATLAB R2025b signatures:
    //   w = modwt(x)
    //   w = modwt(x, wname)
    //   w = modwt(x, wname, lev)
    //   w = modwt(x, lev)            (numeric 2nd arg → uses default wname)
    // Default wname = 'sym4'; default lev = floor(log2(N)).
    if (args.empty())
        throw Error("modwt: requires (x[, wname[, lev]])",
                    0, 0, "modwt", "", "numkit:modwt:nargin");
    auto *mr = ctx.engine->resource();
    const Value &x = args[0];
    const size_t N = x.numel();
    const int default_lev = (N >= 2)
        ? static_cast<int>(std::floor(std::log2(static_cast<double>(N))))
        : 1;

    std::string wname = "sym4";
    int lev = default_lev;
    if (args.size() >= 2) {
        if (args[1].isChar() || args[1].isString()) {
            wname = args[1].toString();
            if (args.size() >= 3)
                lev = static_cast<int>(args[2].toScalar());
        } else {
            // Numeric 2nd arg: that's the level (wname stays default).
            lev = static_cast<int>(args[1].toScalar());
        }
    }
    outs[0] = modwt(x, lev, wname, mr);
}

void imodwt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imodwt: requires (swc, wname)",
                    0, 0, "imodwt", "", "numkit:imodwt:nargin");
    outs[0] = imodwt(args[0], argString(args[1]), ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::wavelet
