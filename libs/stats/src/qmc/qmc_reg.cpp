// libs/signal/src/qmc/qmc_reg.cpp
//
// CallContext register half of qmc/qmc.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/qmc/qmc.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace detail {

void haltonset_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("haltonset: requires (dim[, 'Skip', s, 'Leap', l])",
                    0, 0, "haltonset", "", "numkit:haltonset:nargin");
    const int d = static_cast<int>(args[0].toScalar());
    // MATLAB's default Skip is 0 — the trivial origin [0…0] is the first
    // returned point. 'Skip', 1 starts at index 1 = [0.5, 1/3, …].
    long long skip = 0, leap = 0;
    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const Value &v = args[i + 1];
        if      (name == "skip") skip = static_cast<long long>(v.toScalar());
        else if (name == "leap") leap = static_cast<long long>(v.toScalar());
    }
    outs[0] = haltonset(d, skip, leap, ctx.engine->resource());
}

void net_reg(Span<const Value> args, size_t /*nargout*/,
             Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("net: requires (stream, n)",
                    0, 0, "net", "", "numkit:net:nargin");
    const long long n = static_cast<long long>(args[1].toScalar());
    outs[0] = net(args[0], n, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
