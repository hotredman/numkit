// libs/image/src/filter/filter_design_reg.cpp
//
// Register half of the image filter_design builtins: the CallContext wrappers
// delegating to the engine-free compute in filter_design.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/filter/filter.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace numkit::image {

namespace detail {

void fspecial3_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fspecial3: requires at least (type)",
                    0, 0, "fspecial3", "", "numkit:fspecial3:nargin");
    if (!args[0].isChar() && !args[0].isString())
        throw Error("fspecial3: type must be a string",
                    0, 0, "fspecial3", "", "numkit:fspecial3:BadType");
    const std::string type = args[0].toString();
    Value empty;
    // Positional layout per MATLAB R2025b:
    //   sobel/prewitt:  a1 = direction        (no hsize — fixed 3×3×3)
    //   ellipsoid:      a1 = semiaxes
    //   laplacian:      a1 = gamma1, a2 = gamma2
    //   average:        a1 = hsize
    //   gaussian/log:   a1 = hsize, a2 = sigma
    Value a1 = (args.size() >= 2) ? args[1] : empty;
    Value a2 = (args.size() >= 3) ? args[2] : empty;
    outs[0] = fspecial3(type, a1, a2, ctx.engine->resource());
}

// fsamp2_reg / ftrans2_reg / fwind1_reg / fwind2_reg now live in
// fir2d.cpp (cycle 65 — full implementations replacing the previous
// KNOWN-GAP stubs and the basic fwind2 stub).

void gabor_reg(Span<const Value> /*args*/, size_t /*nargout*/,
               Span<Value> /*outs*/, CallContext &)
{
    throw Error("gabor: not implemented in v1 — Gabor filter object "
                "requires class infrastructure. KNOWN GAP.",
                0, 0, "gabor", "", "numkit:gabor:NotImpl");
}

} // namespace detail

} // namespace numkit::image
