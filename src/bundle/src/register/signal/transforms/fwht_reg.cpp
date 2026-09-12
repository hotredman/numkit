// toolboxes/signal/src/transforms/fwht_reg.cpp
//
// CallContext register half of transforms/fwht.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/signal/transforms/fwht.hpp>
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/value.hpp>
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace nk::signal {

namespace detail {

static void parseFwhtArgs(Span<const Value> args,
                           std::size_t &n, std::string &ordering)
{
    n = 0;
    ordering = "sequency";
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].isChar() || args[1].isString()) {
            // (x, ordering) — n omitted.
            ordering = args[1].toString();
            return;
        }
        n = static_cast<std::size_t>(args[1].toScalar());
    }
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (!(args[2].isChar() || args[2].isString()))
            throw Error("fwht: ordering must be a string",
                         0, 0, "fwht", "", "nk:fwht:badOrdering");
        ordering = args[2].toString();
    }
}

void fwht_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fwht: requires (x [, n [, ordering]])",
                     0, 0, "fwht", "", "nk:fwht:nargin");
    std::size_t n; std::string ordering;
    parseFwhtArgs(args, n, ordering);
    outs[0] = fwht(args[0], n, ordering, ctx.engine->resource());
}

void ifwht_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ifwht: requires (y [, n [, ordering]])",
                     0, 0, "ifwht", "", "nk:ifwht:nargin");
    std::size_t n; std::string ordering;
    parseFwhtArgs(args, n, ordering);
    outs[0] = ifwht(args[0], n, ordering, ctx.engine->resource());
}

} // namespace detail

} // namespace nk::signal
