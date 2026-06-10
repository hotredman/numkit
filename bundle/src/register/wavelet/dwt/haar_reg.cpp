// toolboxes/wavelet/src/dwt/haar_reg.cpp
//
// Register half of the Haar transform pair: the CallContext builtins
// haart / ihaart (argument parsing + nargout handling) that delegate to the
// engine-free compute in haart.cpp / ihaart.cpp. This is the only Haar TU that
// needs the engine; keeping it separate lets the compute build against
// value+fs+ops alone. library.cpp forward-declares + registers these.
//
// Phase 2b compute/register-split pilot — see project_layering_refactor memory.

#include <numkit/wavelet/dwt/haart.hpp>
#include <numkit/wavelet/dwt/ihaart.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <cctype>
#include <cmath>
#include <string>

namespace numkit::wavelet {
namespace detail {

void haart_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("haart: requires (x[, level[, integerflag]])",
                    0, 0, "haart", "", "numkit:haart:nargin");
    // Parse level (positive integer) + integerflag string here for
    // script-quality errors, then delegate to the public haart().
    int level = 0;  // 0 -> auto (max level), resolved inside haart()
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const double lvld = args[1].toScalar();
        if (!(lvld > 0.0))
            throw Error("haart: expected LEVEL to be positive",
                        0, 0, "haart", "", "numkit:haart:level");
        if (lvld != std::floor(lvld))
            throw Error("haart: expected LEVEL to be an integer",
                        0, 0, "haart", "", "numkit:haart:level");
        level = static_cast<int>(lvld);
    }

    bool integer_mode = false;
    if (args.size() >= 3) {
        if (!args[2].isChar() && !args[2].isString())
            throw Error("haart: integerflag must be 'noninteger' or 'integer'",
                        0, 0, "haart", "", "numkit:haart:flag");
        std::string flag = args[2].toString();
        for (auto &c : flag)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (flag == "integer")    integer_mode = true;
        else if (flag == "noninteger") integer_mode = false;
        else
            throw Error("haart: integerflag must be 'noninteger' or 'integer' (got '" +
                        flag + "')",
                        0, 0, "haart", "", "numkit:haart:flag");
    }

    HaartResult r = haart(args[0], level, integer_mode, ctx.engine->resource());
    outs[0] = std::move(r.a);
    if (nargout >= 2) outs[1] = std::move(r.d);
}

void ihaart_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ihaart: requires (a, d[, level[, integerflag]])",
                    0, 0, "ihaart", "", "numkit:ihaart:nargin");
    // Lax positional parse: level (number) and/or integerflag (string), in
    // any order. The level < Nlevels range check is done inside ihaart().
    int level = 0;
    bool integer_mode = false;
    for (size_t i = 2; i < args.size(); ++i) {
        const Value &arg = args[i];
        if (arg.isChar() || arg.isString()) {
            std::string flag = arg.toString();
            for (auto &c : flag)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if      (flag == "integer")    integer_mode = true;
            else if (flag == "noninteger") integer_mode = false;
            else
                throw Error("ihaart: integerflag must be 'noninteger' or "
                            "'integer' (got '" + flag + "')",
                            0, 0, "ihaart", "", "numkit:ihaart:flag");
        } else {
            const double lvld = arg.toScalar();
            if (!(lvld >= 0.0))
                throw Error("ihaart: expected input number 3, LEVEL, to be a "
                            "scalar with value >= 0",
                            0, 0, "ihaart", "", "numkit:ihaart:level");
            if (lvld != std::floor(lvld))
                throw Error("ihaart: expected LEVEL to be an integer",
                            0, 0, "ihaart", "", "numkit:ihaart:level");
            level = static_cast<int>(lvld);
        }
    }
    outs[0] = ihaart(args[0], args[1], level, integer_mode,
                     ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::wavelet
