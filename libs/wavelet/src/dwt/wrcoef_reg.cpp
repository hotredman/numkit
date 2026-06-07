// libs/wavelet/src/dwt/wrcoef_reg.cpp
//
// Register half of wrcoef: the CallContext builtin (type/level parsing,
// wname-form guard) that delegates to the engine-free compute in
// wrcoef.cpp. The compute wrcoef() is declared in multilevel.hpp, so this
// TU includes that header. library.cpp forward-declares + registers this.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/wavelet/dwt/multilevel.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <cctype>
#include <cmath>
#include <string>

namespace numkit::wavelet {
namespace detail {

void wrcoef_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("wrcoef: requires (type, c, l, wname[, n])",
                    0, 0, "wrcoef", "", "numkit:wrcoef:nargin");

    if (!args[0].isChar() && !args[0].isString())
        throw Error("wrcoef: type must be a character vector ('a' or 'd')",
                    0, 0, "wrcoef", "", "numkit:wrcoef:type");
    std::string type = args[0].toString();
    for (auto &ch : type)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    // MATLAB also accepts (Lo_R, Hi_R) in place of wname — guard
    // against that misuse with a clear message.
    if (!args[3].isChar() && !args[3].isString())
        throw Error("wrcoef: numkit only supports the wname form "
                    "wrcoef(type, c, l, wname[, n]). The (Lo_R, Hi_R) "
                    "two-filter form is not implemented.",
                    0, 0, "wrcoef", "", "numkit:wrcoef:wname");
    const std::string wname = args[3].toString();

    int n = -1;     // sentinel meaning "default"
    if (args.size() >= 5) {
        if (args[4].isEmpty()) {
            // empty → keep default
        } else {
            const double nd = args[4].toScalar();
            if (nd < 0.0 || nd != std::floor(nd))
                throw Error("wrcoef: level n must be a non-negative integer",
                            0, 0, "wrcoef", "", "numkit:wrcoef:level");
            n = static_cast<int>(nd);
        }
    }

    outs[0] = wrcoef(type, args[1], args[2], wname, n,
                     ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::wavelet
