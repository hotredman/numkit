// libs/image/src/color/demosaic_reg.cpp
//
// Register half of the image demosaic builtin: the CallContext wrapper
// delegating to the engine-free compute in demosaic.cpp. library.cpp
// forward-declares + registers it by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/color/color.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <cstddef>
#include <string>

namespace numkit::image {

namespace detail {

static std::string asString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("demosaic: expected a string argument",
                    0, 0, "demosaic", "", "numkit:demosaic:type");
    return v.toString();
}

void demosaic_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("demosaic: requires (I, sensorAlignment [, NV])",
                    0, 0, "demosaic", "", "numkit:demosaic:nargin");
    auto *mr = ctx.engine->resource();
    const std::string align = asString(args[1]);
    int bps = 0;
    for (size_t i = 2; i + 1 < args.size(); i += 2) {
        const std::string name = asString(args[i]);
        if (name == "BitsPerSample" || name == "bitspersample")
            bps = static_cast<int>(args[i + 1].toScalar());
        else
            throw Error("demosaic: unknown name-value parameter '" + name + "'",
                        0, 0, "demosaic", "", "numkit:demosaic:nv");
    }
    outs[0] = demosaic(args[0], align, bps, mr);
}

} // namespace detail

} // namespace numkit::image
