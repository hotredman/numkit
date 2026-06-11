// toolboxes/image/src/type_convert/type_convert_reg.cpp
//
// Register half of the image type_convert builtins: the CallContext wrappers
// delegating to the engine-free compute in type_convert.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/type_convert/type_convert.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::image {

namespace detail {

#define NK_IM_UNARY_REG(name)                                                   \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires X", 0, 0, #name, "",                  \
                        "numkit:" #name ":nargin");                                  \
        outs[0] = name(args[0], ctx.engine->resource());                        \
    }

NK_IM_UNARY_REG(im2double)
NK_IM_UNARY_REG(im2single)
NK_IM_UNARY_REG(im2uint8)
NK_IM_UNARY_REG(im2uint16)
NK_IM_UNARY_REG(im2int16)
NK_IM_UNARY_REG(im2gray)
NK_IM_UNARY_REG(rgb2gray)

#undef NK_IM_UNARY_REG

void mat2gray_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mat2gray: requires X[, [lo hi]]", 0, 0, "mat2gray", "",
                    "numkit:mat2gray:nargin");
    double lo = std::numeric_limits<double>::quiet_NaN();
    double hi = std::numeric_limits<double>::quiet_NaN();
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].numel() != 2)
            throw Error("mat2gray: range must be a 2-element vector",
                        0, 0, "mat2gray", "", "numkit:mat2gray:size");
        lo = args[1].elemAsDouble(0);
        hi = args[1].elemAsDouble(1);
    }
    outs[0] = mat2gray(args[0], lo, hi, ctx.engine->resource());
}

void iptnum2ordinal_reg(Span<const Value> args, size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("iptnum2ordinal: requires (n)",
                    0, 0, "iptnum2ordinal", "", "numkit:iptnum2ordinal:nargin");
    outs[0] = iptnum2ordinal(args[0].toScalar(), ctx.engine->resource());
}

void imcast_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imcast: requires (I, type)",
                    0, 0, "imcast", "", "numkit:imcast:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("imcast: TYPE must be a string",
                    0, 0, "imcast", "", "numkit:imcast:type");
    outs[0] = imcast(args[0], args[1].toString(), ctx.engine->resource());
}

void gray2ind_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gray2ind: requires (I [, n])",
                    0, 0, "gray2ind", "", "numkit:gray2ind:nargin");
    int n = (args[0].type() == ValueType::LOGICAL) ? 2 : 64;
    if (args.size() >= 2 && !args[1].isEmpty())
        n = static_cast<int>(args[1].toScalar());
    auto [ind, map] = gray2ind(args[0], n, ctx.engine->resource());
    outs[0] = std::move(ind);
    if (nargout > 1) outs[1] = std::move(map);
}

void ind2gray_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ind2gray: requires (idx [, map])",
                    0, 0, "ind2gray", "", "numkit:ind2gray:nargin");
    Value mp;
    if (args.size() >= 2 && !args[1].isEmpty()) mp = args[1];
    outs[0] = ind2gray(args[0], mp, ctx.engine->resource());
}

void ind2rgb_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ind2rgb: requires (idx, map)",
                    0, 0, "ind2rgb", "", "numkit:ind2rgb:nargin");
    outs[0] = ind2rgb(args[0], args[1], ctx.engine->resource());
}

void getrangefromclass_reg(Span<const Value> args, size_t /*nargout*/,
                           Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("getrangefromclass: requires (I)",
                    0, 0, "getrangefromclass", "",
                    "numkit:getrangefromclass:nargin");
    outs[0] = getrangefromclass(args[0], ctx.engine->resource());
}

void isbw_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isbw: requires (BW [, mode])",
                    0, 0, "isbw", "", "numkit:isbw:nargin");
    std::string mode = "logical";
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("isbw: MODE must be a string",
                        0, 0, "isbw", "", "numkit:isbw:mode");
        mode = args[1].toString();
    }
    outs[0] = isbw(args[0], mode, ctx.engine->resource());
}

void isgray_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isgray: requires (I)", 0, 0, "isgray", "",
                    "numkit:isgray:nargin");
    outs[0] = isgray(args[0], ctx.engine->resource());
}

void isind_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isind: requires (I)", 0, 0, "isind", "",
                    "numkit:isind:nargin");
    outs[0] = isind(args[0], ctx.engine->resource());
}

void isrgb_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isrgb: requires (I)", 0, 0, "isrgb", "",
                    "numkit:isrgb:nargin");
    outs[0] = isrgb(args[0], ctx.engine->resource());
}

void iscolormap_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("iscolormap: requires (cmap)", 0, 0, "iscolormap", "",
                    "numkit:iscolormap:nargin");
    outs[0] = iscolormap(args[0], ctx.engine->resource());
}

void intlut_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("intlut: requires (A, LUT)", 0, 0, "intlut", "",
                    "numkit:intlut:nargin");
    outs[0] = intlut(args[0], args[1], ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::image
