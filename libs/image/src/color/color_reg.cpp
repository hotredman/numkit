// libs/image/src/color/color_reg.cpp
//
// Register half of the image colour-space builtins: the CallContext wrappers
// delegating to the engine-free compute in color.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/color/color.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace numkit::image {

namespace detail {

#define NK_COLOR_REG(name)                                                        \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                   \
                    Span<Value> outs, CallContext &ctx)                           \
    {                                                                               \
        if (args.empty())                                                           \
            throw Error(#name ": requires X", 0, 0, #name, "",                     \
                        "numkit:" #name ":nargin");                                     \
        outs[0] = name(args[0], ctx.engine->resource());                           \
    }

void imsplit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imsplit: requires (I)", 0, 0, "imsplit", "",
                    "numkit:imsplit:nargin");
    std::vector<Value> planes;
    imsplit(args[0], planes, ctx.engine->resource());
    const size_t M = std::min((size_t)outs.size(),
                               std::max(nargout, (size_t)1));
    for (size_t i = 0; i < M && i < planes.size(); ++i)
        outs[i] = std::move(planes[i]);
}

NK_COLOR_REG(rgb2hsv)
NK_COLOR_REG(hsv2rgb)
NK_COLOR_REG(rgb2ycbcr)
NK_COLOR_REG(ycbcr2rgb)
NK_COLOR_REG(rgb2ntsc)
NK_COLOR_REG(ntsc2rgb)
NK_COLOR_REG(lab2double)
NK_COLOR_REG(lab2single)
NK_COLOR_REG(lab2uint8)
NK_COLOR_REG(lab2uint16)
NK_COLOR_REG(rgb2xyz)
NK_COLOR_REG(xyz2rgb)
NK_COLOR_REG(rgb2lab)
NK_COLOR_REG(lab2rgb)
NK_COLOR_REG(xyz2lab)
NK_COLOR_REG(lab2xyz)

#undef NK_COLOR_REG

// label2rgb_reg lives in color_extras.cpp (it needs the jet/named-colormap
// helpers there); this TU keeps only the uint8 pixel-mapping core above.

void colorgradient_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("colorgradient: requires (C [, w] [, n])",
                    0, 0, "colorgradient", "", "numkit:colorgradient:nargin");
    auto *mr = ctx.engine->resource();
    Value w;
    int n = 64;
    // Octave shorthand: colorgradient(C, w_or_n) — if 2nd arg is scalar,
    // it's n; if vector, it's w (and n defaults to 64).
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].numel() == 1) {
            n = static_cast<int>(args[1].toScalar());
        } else {
            w = args[1];
        }
    }
    if (args.size() >= 3 && !args[2].isEmpty())
        n = static_cast<int>(args[2].toScalar());
    outs[0] = colorgradient(args[0], w, n, mr);
}

void wavelength2rgb_reg(Span<const Value> args, size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("wavelength2rgb: requires (wavelength [, class [, gamma]])",
                    0, 0, "wavelength2rgb", "", "numkit:wavelength2rgb:nargin");
    auto *mr = ctx.engine->resource();
    std::string cls = "double";
    double gamma = 0.8;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("wavelength2rgb: class must be a string",
                        0, 0, "wavelength2rgb", "", "numkit:wavelength2rgb:cls");
        cls = args[1].toString();
    }
    if (args.size() >= 3 && !args[2].isEmpty()) gamma = args[2].toScalar();
    outs[0] = wavelength2rgb(args[0], cls, gamma, mr);
}

void colorangle_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("colorangle: requires (rgb1, rgb2)",
                    0, 0, "colorangle", "", "numkit:colorangle:nargin");
    outs[0] = colorangle(args[0], args[1], ctx.engine->resource());
}

void cmap2gray_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cmap2gray: requires (cmap)",
                    0, 0, "cmap2gray", "", "numkit:cmap2gray:nargin");
    outs[0] = cmap2gray(args[0], ctx.engine->resource());
}

void gray_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("gray: N must be a scalar integer",
                        0, 0, "gray", "", "numkit:gray:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("gray: N must be a scalar integer",
                        0, 0, "gray", "", "numkit:gray:n");
        n = static_cast<int>(d);
    }
    outs[0] = gray_cmap(n, ctx.engine->resource());
}

void hot_reg(Span<const Value> args, size_t /*nargout*/,
             Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("hot: N must be a scalar integer",
                        0, 0, "hot", "", "numkit:hot:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("hot: N must be a scalar integer",
                        0, 0, "hot", "", "numkit:hot:n");
        n = static_cast<int>(d);
    }
    outs[0] = hot_cmap(n, ctx.engine->resource());
}

void cool_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("cool: N must be a scalar integer",
                        0, 0, "cool", "", "numkit:cool:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("cool: N must be a scalar integer",
                        0, 0, "cool", "", "numkit:cool:n");
        n = static_cast<int>(d);
    }
    outs[0] = cool_cmap(n, ctx.engine->resource());
}

void spring_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("spring: N must be a scalar integer",
                        0, 0, "spring", "", "numkit:spring:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("spring: N must be a scalar integer",
                        0, 0, "spring", "", "numkit:spring:n");
        n = static_cast<int>(d);
    }
    outs[0] = spring_cmap(n, ctx.engine->resource());
}

void summer_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("summer: N must be a scalar integer",
                        0, 0, "summer", "", "numkit:summer:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("summer: N must be a scalar integer",
                        0, 0, "summer", "", "numkit:summer:n");
        n = static_cast<int>(d);
    }
    outs[0] = summer_cmap(n, ctx.engine->resource());
}

void autumn_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("autumn: N must be a scalar integer",
                        0, 0, "autumn", "", "numkit:autumn:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("autumn: N must be a scalar integer",
                        0, 0, "autumn", "", "numkit:autumn:n");
        n = static_cast<int>(d);
    }
    outs[0] = autumn_cmap(n, ctx.engine->resource());
}

void winter_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("winter: N must be a scalar integer",
                        0, 0, "winter", "", "numkit:winter:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("winter: N must be a scalar integer",
                        0, 0, "winter", "", "numkit:winter:n");
        n = static_cast<int>(d);
    }
    outs[0] = winter_cmap(n, ctx.engine->resource());
}

void copper_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("copper: N must be a scalar integer",
                        0, 0, "copper", "", "numkit:copper:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("copper: N must be a scalar integer",
                        0, 0, "copper", "", "numkit:copper:n");
        n = static_cast<int>(d);
    }
    outs[0] = copper_cmap(n, ctx.engine->resource());
}

void pink_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("pink: N must be a scalar integer",
                        0, 0, "pink", "", "numkit:pink:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("pink: N must be a scalar integer",
                        0, 0, "pink", "", "numkit:pink:n");
        n = static_cast<int>(d);
    }
    outs[0] = pink_cmap(n, ctx.engine->resource());
}

void hsv_cmap_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("hsv: N must be a scalar integer",
                        0, 0, "hsv", "", "numkit:hsv:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("hsv: N must be a scalar integer",
                        0, 0, "hsv", "", "numkit:hsv:n");
        n = static_cast<int>(d);
    }
    outs[0] = hsv_cmap(n, ctx.engine->resource());
}

void flag_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("flag: N must be a scalar integer",
                        0, 0, "flag", "", "numkit:flag:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("flag: N must be a scalar integer",
                        0, 0, "flag", "", "numkit:flag:n");
        n = static_cast<int>(d);
    }
    outs[0] = flag_cmap(n, ctx.engine->resource());
}

void prism_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("prism: N must be a scalar integer",
                        0, 0, "prism", "", "numkit:prism:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("prism: N must be a scalar integer",
                        0, 0, "prism", "", "numkit:prism:n");
        n = static_cast<int>(d);
    }
    outs[0] = prism_cmap(n, ctx.engine->resource());
}

void lines_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("lines: N must be a scalar integer",
                        0, 0, "lines", "", "numkit:lines:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("lines: N must be a scalar integer",
                        0, 0, "lines", "", "numkit:lines:n");
        n = static_cast<int>(d);
    }
    outs[0] = lines_cmap(n, ctx.engine->resource());
}

void bone_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("bone: N must be a scalar integer",
                        0, 0, "bone", "", "numkit:bone:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("bone: N must be a scalar integer",
                        0, 0, "bone", "", "numkit:bone:n");
        n = static_cast<int>(d);
    }
    outs[0] = bone_cmap(n, ctx.engine->resource());
}

void white_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("white: N must be a scalar integer",
                        0, 0, "white", "", "numkit:white:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("white: N must be a scalar integer",
                        0, 0, "white", "", "numkit:white:n");
        n = static_cast<int>(d);
    }
    outs[0] = white_cmap(n, ctx.engine->resource());
}

void rgb2lin_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rgb2lin: requires (A)", 0, 0, "rgb2lin", "",
                    "numkit:rgb2lin:nargin");
    outs[0] = rgb2lin(args[0], ctx.engine->resource());
}

void lin2rgb_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("lin2rgb: requires (A)", 0, 0, "lin2rgb", "",
                    "numkit:lin2rgb:nargin");
    outs[0] = lin2rgb(args[0], ctx.engine->resource());
}

void xyz2double_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("xyz2double: requires (xyz)", 0, 0, "xyz2double", "",
                    "numkit:xyz2double:nargin");
    outs[0] = xyz2double(args[0], ctx.engine->resource());
}

void xyz2uint16_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("xyz2uint16: requires (xyz)", 0, 0, "xyz2uint16", "",
                    "numkit:xyz2uint16:nargin");
    outs[0] = xyz2uint16(args[0], ctx.engine->resource());
}

void brighten_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("brighten: requires (map, beta)", 0, 0, "brighten", "",
                    "numkit:brighten:nargin");
    const double beta = args[1].toScalar();
    outs[0] = brighten(args[0], beta, ctx.engine->resource());
}

void contrast_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("contrast: requires (X[, m])", 0, 0, "contrast", "",
                    "numkit:contrast:nargin");
    int m = 64;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const double d = args[1].toScalar();
        if (!std::isfinite(d) || d < 2.0)
            throw Error("contrast: M must be a finite scalar >= 2",
                        0, 0, "contrast", "", "numkit:contrast:m");
        m = static_cast<int>(d);
    }
    outs[0] = contrast(args[0], m, ctx.engine->resource());
}

void deltaE_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("deltaE: requires (I1, I2[, 'isInputLab', tf])",
                    0, 0, "deltaE", "", "numkit:deltaE:nargin");
    bool isInputLab = false;
    // Parse name-value pair: 'isInputLab', value.
    for (size_t i = 2; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("deltaE: name-value pairs require string keys",
                        0, 0, "deltaE", "", "numkit:deltaE:nv");
        std::string key = args[i].toString();
        std::string lo;
        for (char c : key) lo.push_back(static_cast<char>(std::tolower(c)));
        if (lo == "isinputlab")
            isInputLab = (args[i + 1].toScalar() != 0.0);
        else
            throw Error("deltaE: unknown name '" + key + "'",
                        0, 0, "deltaE", "", "numkit:deltaE:nv");
    }
    outs[0] = deltaE(args[0], args[1], isInputLab, ctx.engine->resource());
}

void whitepoint_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    std::string illum = "icc";  // default
    if (args.size() >= 1 && !args[0].isEmpty()) {
        if (!args[0].isChar() && !args[0].isString())
            throw Error("whitepoint: illuminant must be a string",
                        0, 0, "whitepoint", "", "numkit:whitepoint:type");
        illum = args[0].toString();
    }
    outs[0] = whitepoint(illum, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::image
