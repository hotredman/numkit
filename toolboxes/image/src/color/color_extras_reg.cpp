// toolboxes/image/src/color/color_extras_reg.cpp
//
// Register half of the image color-extras builtins (cmunique / lab2double /
// imnoise-adjacent colour ops etc.): the CallContext wrappers delegating to
// the engine-free compute in color_extras.cpp. library.cpp forward-declares +
// registers these by name. (Four detail blocks, mirroring the interleaved
// layout of the compute TU.)
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/color/color.hpp>
#include <numkit/image/type_convert/type_convert.hpp>
#include <numkit/image/geom/geom.hpp>
#include <numkit/image/contrast/contrast.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include "color_extras_detail.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace numkit::image {

namespace detail {

void rgb2lightness_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rgb2lightness: requires (RGB)",
                    0, 0, "rgb2lightness", "", "numkit:rgb2lightness:nargin");
    outs[0] = rgb2lightness(args[0], ctx.engine->resource());
}

void rgb2ind_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rgb2ind: requires (RGB, inmap [, dithering]) — "
                    "Q/tol forms deferred",
                    0, 0, "rgb2ind", "", "numkit:rgb2ind:nargin");
    // Optional 3rd arg: 'dither' (default in MATLAB) | 'nodither'.
    // Numkit always behaves as 'nodither'; if 'dither' is requested we
    // throw to keep parity honest (KNOWN GAP).
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString())) {
        const std::string s = args[2].toString();
        if (s == "dither")
            throw Error("rgb2ind: 'dither' option not implemented in v1 "
                        "(KNOWN GAP); pass 'nodither' instead",
                        0, 0, "rgb2ind", "", "numkit:rgb2ind:NoDither");
        if (s != "nodither")
            throw Error("rgb2ind: dithering arg must be 'dither' or 'nodither'",
                        0, 0, "rgb2ind", "", "numkit:rgb2ind:BadOpt");
    }
    // Q (positive integer scalar) and tol (real in [0,1]) forms throw.
    if (args[1].numel() == 1) {
        throw Error("rgb2ind: scalar Q (min-variance quant) and tol "
                    "(uniform quant) forms not implemented in v1; pass "
                    "an explicit K×3 colormap instead (KNOWN GAP)",
                    0, 0, "rgb2ind", "", "numkit:rgb2ind:NotImpl");
    }
    auto [X, cmap] = rgb2ind_inmap(args[0], args[1], ctx.engine->resource());
    outs[0] = X;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = cmap;
}

} // namespace detail

namespace detail {

void rgbwide2ycbcr_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rgbwide2ycbcr: requires (RGB, BPS)",
                    0, 0, "rgbwide2ycbcr", "", "numkit:rgbwide2ycbcr:nargin");
    const int bps = static_cast<int>(args[1].toScalar());
    outs[0] = rgbwide2ycbcr(args[0], bps, ctx.engine->resource());
}

void ycbcr2rgbwide_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ycbcr2rgbwide: requires (YCBCR, BPS)",
                    0, 0, "ycbcr2rgbwide", "", "numkit:ycbcr2rgbwide:nargin");
    const int bps = static_cast<int>(args[1].toScalar());
    outs[0] = ycbcr2rgbwide(args[0], bps, ctx.engine->resource());
}

void rgbwide2xyz_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rgbwide2xyz: requires (RGB, BPS [, NV...])",
                    0, 0, "rgbwide2xyz", "",
                    "numkit:rgbwide2xyz:nargin");
    auto *mr = ctx.engine->resource();
    const int bps = static_cast<int>(args[1].toScalar());
    std::string cs = "BT.2020";
    std::string lin = "PQ";
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };
    std::size_t i = 2;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("rgbwide2xyz: expected NV-pair name string",
                        0, 0, "rgbwide2xyz", "",
                        "numkit:rgbwide2xyz:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name) nlo += static_cast<char>(std::tolower(
            static_cast<unsigned char>(ch)));
        if (nlo == "colorspace") cs = args[i + 1].toString();
        else if (nlo == "linearizationfcn") lin = args[i + 1].toString();
        else if (nlo == "whitepoint") {
            // Accepted-but-ignored — only D65 path is implemented this cycle.
        } else {
            throw Error("rgbwide2xyz: unknown option '" + name + "'",
                        0, 0, "rgbwide2xyz", "",
                        "numkit:rgbwide2xyz:unknownNv");
        }
        i += 2;
    }
    outs[0] = rgbwide2xyz(args[0], bps, cs, lin, mr);
}

void xyz2rgbwide_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("xyz2rgbwide: requires (XYZ, BPS [, NV...])",
                    0, 0, "xyz2rgbwide", "",
                    "numkit:xyz2rgbwide:nargin");
    auto *mr = ctx.engine->resource();
    const int bps = static_cast<int>(args[1].toScalar());
    std::string cs = "BT.2020";
    std::string lin = "PQ";
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };
    std::size_t i = 2;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("xyz2rgbwide: expected NV-pair name string",
                        0, 0, "xyz2rgbwide", "",
                        "numkit:xyz2rgbwide:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name) nlo += static_cast<char>(std::tolower(
            static_cast<unsigned char>(ch)));
        if (nlo == "colorspace") cs = args[i + 1].toString();
        else if (nlo == "linearizationfcn") lin = args[i + 1].toString();
        else if (nlo == "whitepoint") {
            // Accepted-but-ignored — only D65 path is implemented this cycle.
        } else {
            throw Error("xyz2rgbwide: unknown option '" + name + "'",
                        0, 0, "xyz2rgbwide", "",
                        "numkit:xyz2rgbwide:unknownNv");
        }
        i += 2;
    }
    outs[0] = xyz2rgbwide(args[0], bps, cs, lin, mr);
}

void cmunique_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cmunique: requires (X, MAP), (RGB), or (I)",
                    0, 0, "cmunique", "", "numkit:cmunique:nargin");
    auto *mr = ctx.engine->resource();
    std::pair<Value, Value> result;
    if (args.size() == 1) {
        const auto &d = args[0].dims();
        if (d.is3D() && d.pages() == 3)
            result = cmunique_rgb(args[0], mr);
        else
            result = cmunique_i(args[0], mr);
    } else {
        result = cmunique_xm(args[0], args[1], mr);
    }
    outs[0] = std::move(result.first);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(result.second);
}

} // namespace detail (cmunique adapter)

namespace detail {

void imfuse_reg(Span<const Value> args, std::size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imfuse: requires (A, B [, METHOD] [, NV...])",
                    0, 0, "imfuse", "", "numkit:imfuse:nargin");
    auto *mr = ctx.engine->resource();

    const Value &A = args[0];
    const Value &B = args[1];

    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };
    std::string method = "falsecolor";
    std::string scaling = "independent";
    Value channels;  // empty → default (green-magenta)
    std::size_t i = 2;
    if (i < args.size() && is_string(args[i])) {
        std::string m = args[i].toString();
        static const std::array<const char *, 5> mset{
            "falsecolor", "blend", "diff", "checkerboard", "montage"};
        static const std::array<const char *, 2> nvset{
            "Scaling", "ColorChannels"};
        bool is_method = false;
        for (auto mm : mset) if (m == mm) { is_method = true; break; }
        bool is_nv = false;
        // Case-insensitive NV-name check.
        std::string mlo;
        for (char ch : m)
            mlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (mlo == "scaling" || mlo == "colorchannels") is_nv = true;
        if (is_method) { method = m; ++i; }
        else if (!is_nv)
            throw Error("imfuse: unknown METHOD '" + m + "' (allowed: "
                        "falsecolor / blend / diff / checkerboard / "
                        "montage)",
                        0, 0, "imfuse", "", "numkit:imfuse:method");
        // else: leave i=2 so it gets parsed as NV pair below.
    }
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("imfuse: expected NV-pair name string",
                        0, 0, "imfuse", "", "numkit:imfuse:badNvArg");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "scaling") {
            scaling = args[i + 1].toString();
            // MATLAB lowercases for switch — replicate.
            std::string slo;
            for (char ch : scaling)
                slo += static_cast<char>(std::tolower(
                    static_cast<unsigned char>(ch)));
            scaling = slo;
        } else if (nlo == "colorchannels") {
            const Value &v = args[i + 1];
            if (is_string(v)) {
                std::string s = v.toString();
                std::string slo;
                for (char ch : s)
                    slo += static_cast<char>(std::tolower(
                        static_cast<unsigned char>(ch)));
                channels = Value::matrix(1, 3, ValueType::DOUBLE, mr);
                if (slo == "red-cyan") {
                    channels.doubleDataMut()[0] = 1;
                    channels.doubleDataMut()[1] = 2;
                    channels.doubleDataMut()[2] = 2;
                } else if (slo == "green-magenta") {
                    channels.doubleDataMut()[0] = 2;
                    channels.doubleDataMut()[1] = 1;
                    channels.doubleDataMut()[2] = 2;
                } else {
                    throw Error("imfuse: unknown ColorChannels '" + s + "'",
                                0, 0, "imfuse", "", "numkit:imfuse:channels");
                }
            } else {
                channels = v;
            }
        } else {
            throw Error("imfuse: unknown option '" + name + "'",
                        0, 0, "imfuse", "", "numkit:imfuse:unknownNv");
        }
        i += 2;
    }
    outs[0] = imfuse(A, B, method, scaling, channels, mr);
}

void tonemap_reg(Span<const Value> args, std::size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("tonemap: requires (HDR [, NV...])",
                    0, 0, "tonemap", "", "numkit:tonemap:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    double lo = 0.0, hi = 1.0;
    double saturation = 1.0;
    int ntilesR = 4, ntilesC = 4;

    std::size_t i = 1;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("tonemap: expected NV-pair name",
                        0, 0, "tonemap", "", "numkit:tonemap:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "adjustlightness") {
            const Value &v = args[i + 1];
            if (v.numel() != 2)
                throw Error("tonemap: AdjustLightness must be [lo hi]",
                            0, 0, "tonemap", "", "numkit:tonemap:adjustLen");
            lo = v.elemAsDouble(0);
            hi = v.elemAsDouble(1);
        } else if (nlo == "adjustsaturation") {
            saturation = args[i + 1].toScalar();
        } else if (nlo == "numberoftiles") {
            const Value &v = args[i + 1];
            if (v.numel() == 1) {
                ntilesR = ntilesC = static_cast<int>(v.toScalar());
            } else if (v.numel() == 2) {
                ntilesR = static_cast<int>(v.elemAsDouble(0));
                ntilesC = static_cast<int>(v.elemAsDouble(1));
            } else {
                throw Error("tonemap: NumberOfTiles must be a scalar or "
                            "2-element vector",
                            0, 0, "tonemap", "", "numkit:tonemap:tilesLen");
            }
        } else {
            throw Error("tonemap: unknown option '" + name + "'",
                        0, 0, "tonemap", "", "numkit:tonemap:unknownNv");
        }
        i += 2;
    }
    outs[0] = tonemap(args[0], lo, hi, saturation, ntilesR, ntilesC, mr);
}

} // namespace detail

namespace detail {

void labeloverlay_reg(Span<const Value> args, std::size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("labeloverlay: requires (A, L [, NV...])",
                    0, 0, "labeloverlay", "",
                    "numkit:labeloverlay:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    Value cmap;             // empty → default 'jet'
    std::string ca = "auto";
    Value included;         // empty → default 1:maxLabel
    double transparency = 0.5;

    std::size_t i = 2;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("labeloverlay: expected NV-pair name string",
                        0, 0, "labeloverlay", "",
                        "numkit:labeloverlay:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "colormap") {
            cmap = args[i + 1];
        } else if (nlo == "colorassignment") {
            ca = args[i + 1].toString();
        } else if (nlo == "includedlabels") {
            included = args[i + 1];
        } else if (nlo == "transparency") {
            transparency = args[i + 1].toScalar();
        } else {
            throw Error("labeloverlay: unknown option '" + name + "'",
                        0, 0, "labeloverlay", "",
                        "numkit:labeloverlay:unknownNv");
        }
        i += 2;
    }

    outs[0] = labeloverlay(args[0], args[1], cmap, ca, included,
                           transparency, mr);
}

// label2rgb(L [, map [, zerocolor [, order]]]) — MATLAB R2025b parity.
//   map       : omitted/[] → jet(numregion); colormap NAME string; or Nx3.
//   zerocolor : omitted → white; RGB triplet; or a ColorSpec string
//               ('y/m/c/r/g/b/w/k' or full color names).
//   order     : 'noshuffle' (default). 'shuffle' deferred (needs the
//               swb2712 RNG stream).
// numregion = max(L(:)); the colormap is generated with that many rows, so
// label k maps to row k and label 0 maps to zerocolor. Delegates the pixel
// mapping to the existing uint8 label2rgb(L, cmap, zerocolor) core.
void label2rgb_reg(Span<const Value> args, std::size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("label2rgb: requires (L [, map [, zerocolor [, order]]])",
                    0, 0, "label2rgb", "", "numkit:label2rgb:nargin");
    auto *mr = ctx.engine->resource();
    const Value &L = args[0];

    // numregion = max(L(:)); force ≥1 so the generated colormap always has
    // 3 columns (a 1-row map is never indexed when L is all background).
    const std::size_t plane = L.numel();
    int maxLabel = 0;
    for (std::size_t i = 0; i < plane; ++i) {
        const double v = L.elemAsDouble(i);
        if (std::isfinite(v) && v > maxLabel) maxLabel = static_cast<int>(v);
    }
    const int numregion = maxLabel < 1 ? 1 : maxLabel;

    // ── Resolve colormap: default jet / named string / Nx3 matrix ──
    Value cmap;
    if (args.size() < 2 || args[1].isEmpty())
        cmap = jet_colormap(numregion, mr);
    else if (args[1].isChar() || args[1].isString())
        cmap = resolve_named_colormap(args[1].toString(), numregion, mr);
    else
        cmap = args[1];                       // explicit Nx3 — core validates.

    // ── order (4th positional): only 'noshuffle' supported ─────────
    if (args.size() >= 4 && (args[3].isChar() || args[3].isString())) {
        const std::string ord = lower(args[3].toString());
        if (ord == "shuffle")
            throw Error("label2rgb: order 'shuffle' is not yet supported "
                        "(requires MATLAB's swb2712 random stream)",
                        0, 0, "label2rgb", "",
                        "numkit:label2rgb:shuffleUnsupported");
        if (ord != "noshuffle")
            throw Error("label2rgb: order must be 'noshuffle' or 'shuffle'",
                        0, 0, "label2rgb", "", "numkit:label2rgb:order");
    }

    // ── Resolve zerocolor: RGB triplet or ColorSpec string ─────────
    Value bg;                                 // empty → core defaults white.
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (args[2].isChar() || args[2].isString()) {
            double rgb[3];
            if (!parseColorSpec(args[2].toString(), rgb))
                throw Error("label2rgb: invalid zerocolor string '" +
                            args[2].toString() + "'",
                            0, 0, "label2rgb", "",
                            "numkit:label2rgb:zerocolor");
            bg = Value::matrix(1, 3, ValueType::DOUBLE, mr);
            double *bd = bg.doubleDataMut();
            bd[0] = rgb[0]; bd[1] = rgb[1]; bd[2] = rgb[2];
        } else {
            bg = args[2];                     // numeric triplet — core checks.
        }
    }

    outs[0] = label2rgb(L, cmap, bg, mr);
}

} // namespace detail

} // namespace numkit::image
