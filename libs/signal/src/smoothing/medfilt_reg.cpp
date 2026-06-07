// libs/signal/src/smoothing/medfilt_reg.cpp
//
// Register half of the signal medfilt builtins: the CallContext wrappers
// delegating to the engine-free compute in medfilt.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/signal/smoothing/medfilt.hpp>

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

void medfilt1_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("medfilt1: requires at least 1 argument",
                     0, 0, "medfilt1", "", "numkit:medfilt1:nargin");
    size_t k = 3;
    if (args.size() >= 2 && !args[1].isEmpty()
        && !args[1].isChar() && !args[1].isString())
        k = static_cast<size_t>(args[1].toScalar());

    // Padding mode: MATLAB default 'zeropad'; a trailing 'truncate' string
    // (medfilt1(x,n,'truncate') or medfilt1(x,n,[],dim,'truncate')) clips
    // the window at the ends instead. (blksz/dim/nanflag args are accepted
    // but ignored for now — noted as a gap.)
    bool zeropad = true;
    for (size_t a = args.size(); a-- > 1;) {
        if (args[a].isChar() || args[a].isString()) {
            std::string s = args[a].toString();
            for (char &c : s) c = static_cast<char>(std::tolower((unsigned char)c));
            if (s == "truncate") zeropad = false;
            else if (s == "zeropad") zeropad = true;
            break;
        }
    }
    outs[0] = medfilt1(args[0], k, zeropad, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
