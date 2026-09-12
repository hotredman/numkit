// toolboxes/image/src/color/raw_planar_reg.cpp
//
// Register half of the image raw/planar builtins (raw2planar / planar2raw):
// CallContext wrappers delegating to the engine-free compute in raw_planar.cpp.
// library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <nk/image/color/color.hpp>

#include <nk/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <nk/value/value.hpp>
#include <nk/value/error.hpp>

#include <cstddef>

namespace nk::image {

namespace detail {

void raw2planar_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1)
        throw Error("raw2planar: requires (cfa)",
                    0, 0, "raw2planar", "", "nk:raw2planar:nargin");
    outs[0] = raw2planar(args[0], ctx.engine->resource());
}

void planar2raw_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1)
        throw Error("planar2raw: requires (I)",
                    0, 0, "planar2raw", "", "nk:planar2raw:nargin");
    outs[0] = planar2raw(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace nk::image
