// toolboxes/signal/src/digital_filtering/quantizers_reg.cpp
//
// Register half of the signal quantizers builtins: the CallContext wrappers
// delegating to the engine-free compute in quantizers.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/signal/digital_filtering/quantizers.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

namespace detail {

void uencode_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("uencode: requires (u, N [, V [, 'signed'/'unsigned']])",
                    0, 0, "uencode", "", "numkit:uencode:nargin");
    const int N = static_cast<int>(args[1].toScalar());
    double V = 1.0;
    if (args.size() >= 3 && !args[2].isEmpty()) V = args[2].toScalar();
    bool signedOut = false;
    if (args.size() >= 4 && !args[3].isEmpty()) {
        std::string s = args[3].toString();
        std::transform(s.begin(), s.end(), s.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        if (s == "signed")        signedOut = true;
        else if (s == "unsigned") signedOut = false;
        else throw Error("uencode: 4th arg must be 'signed' or 'unsigned'",
                          0, 0, "uencode", "", "numkit:uencode:Polarity");
    }
    outs[0] = uencode(args[0], N, V, signedOut, ctx.engine->resource());
}

void udecode_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("udecode: requires (u, N [, V [, 'saturate'/'wrap']])",
                    0, 0, "udecode", "", "numkit:udecode:nargin");
    const int N = static_cast<int>(args[1].toScalar());
    double V = 1.0;
    if (args.size() >= 3 && !args[2].isEmpty()) V = args[2].toScalar();
    bool wrap = false;
    if (args.size() >= 4 && !args[3].isEmpty()) {
        std::string s = args[3].toString();
        std::transform(s.begin(), s.end(), s.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        if (s == "wrap")          wrap = true;
        else if (s == "saturate") wrap = false;
        else throw Error("udecode: 4th arg must be 'saturate' or 'wrap'",
                          0, 0, "udecode", "", "numkit:udecode:BadOpt");
    }
    outs[0] = udecode(args[0], N, V, wrap, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
