// toolboxes/signal/src/math/arithmetic/rounding_reg.cpp
//
// CallContext register half of math/arithmetic/rounding.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/library.hpp>
#include <numkit/math/arithmetic/rounding.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "_unary_hint.hpp"  // 3-arg abs hint overload
#include "helpers.hpp"
#include "arithmetic/rounding.hpp"      // detail::doubleCeilLoop / FloorLoop / RoundLoop / FixLoop
#include "arithmetic/rounding_detail.hpp"
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

namespace numkit::builtin {
using namespace numkit::math;  // C4c localized (umbrella removed)

namespace detail {

#define NK_UNARY_ADAPTER(name, fn)                                              \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        outs[0] = fn(args[0], ctx.engine->resource());                          \
    }

NK_UNARY_ADAPTER(floor,   floor)
NK_UNARY_ADAPTER(ceil,    ceil)
NK_UNARY_ADAPTER(fix,     fix)
NK_UNARY_ADAPTER(sign,    sign)
NK_UNARY_ADAPTER(subplus, subplus)

#undef NK_UNARY_ADAPTER

// round(x) | round(x, N) | round(x, N, 'decimals'|'significant').
void round_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("round: requires 1 argument",
                     0, 0, "round", "", "numkit:round:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() < 2 || args[1].isEmpty()) {
        outs[0] = round(args[0], mr);
        return;
    }
    const int n = static_cast<int>(args[1].toScalar());
    bool significant = false;
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString())) {
        std::string s = args[2].toString();
        for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (s == "significant") significant = true;
        else if (s == "decimals")    significant = false;
        else
            throw Error("round: type must be 'decimals' or 'significant'",
                         0, 0, "round", "", "numkit:round:badType");
    }
    if (significant && n < 1)
        throw Error("round: N must be >= 1 for 'significant'",
                     0, 0, "round", "", "numkit:round:badN");
    outs[0] = roundN(args[0], n, significant, mr);
}

} // namespace detail

} // namespace numkit::builtin
