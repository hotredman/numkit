// src/bundle/src/register/graphics/export_reg.cpp
//
// saveas / exportgraphics / print — engine-side figure export (portion
// 32). The renderer itself is core-free (figure/figure_render.hpp: a
// pixel-space display list + SVG and RGB backends); this reg glue owns
// the MATLAB argument forms, turns the RGB buffer into encoded image
// bytes via the image toolbox, and writes through the engine fs.
//
// Supported output: .svg (vector) + everything imwriteToBytes encodes
// (.png/.jpg/.bmp/.tga/.pnm/.tif). Unsupported documented MATLAB
// formats (.pdf/.eps/.fig/.emf) throw a clear "not supported" — filed
// in bugs/opened/graphics/figure-export-gaps.md, never silent.

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/figure/figure_manager.hpp>
#include <numkit/figure/figure_render.hpp>
#include <numkit/graphics/graphics_handle.hpp>
#include <numkit/image/io/io.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace numkit {

namespace {

std::string lowerExtOf(const std::string &path)
{
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return "";
    std::string e = path.substr(dot + 1);
    for (char &c : e)
        c = (char)std::tolower((unsigned char)c);
    return e;
}

// PNG + base64 data-URL embedder for SVG <image> elements. The buffer
// arrives top-down row-major from figrender::imageCmdRgb; it goes to
// imwriteToBytes as a column-major uint8 H×W×3 Value (the transpose is
// the same column-major contract the raster export path uses).
const char kBase64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
figrender::SvgImageEmbedder svgPngEmbedder(
    std::pmr::memory_resource *mr)
{
    return [mr](const std::vector<std::uint8_t> &rgb, int w, int h) {
        const size_t Hp = (size_t)h, Wp = (size_t)w;
        Value A = Value::matrix3d(Hp, Wp, 3, ValueType::UINT8, mr);
        std::uint8_t *d = A.uint8DataMut();
        for (size_t r = 0; r < Hp; ++r)
            for (size_t c = 0; c < Wp; ++c)
                for (size_t ch = 0; ch < 3; ++ch)
                    d[r + c * Hp + ch * Hp * Wp] =
                        rgb[(r * Wp + c) * 3 + ch];
        std::string png = image::imwriteToBytes(A, "png", mr);
        std::string b64 = "data:image/png;base64,";
        b64.reserve(b64.size() + (png.size() / 3 + 1) * 4);
        size_t i = 0;
        for (; i + 2 < png.size(); i += 3) {
            // Cast through uint8_t FIRST: png[i] is signed char, so
            // (char)0xFF << 16 sign-extends and corrupts every byte
            // >= 0x80 (most of any PNG).
            std::uint32_t v = ((std::uint32_t)(std::uint8_t)png[i] << 16)
                            | ((std::uint32_t)(std::uint8_t)png[i + 1] << 8)
                            | (std::uint8_t)png[i + 2];
            b64 += kBase64[(v >> 18) & 63];
            b64 += kBase64[(v >> 12) & 63];
            b64 += kBase64[(v >> 6) & 63];
            b64 += kBase64[v & 63];
        }
        size_t rem = png.size() - i;
        std::uint8_t b0 = rem >= 1 ? (std::uint8_t)png[i] : 0;
        std::uint8_t b1 = rem >= 2 ? (std::uint8_t)png[i + 1] : 0;
        if (rem == 1) {
            b64 += kBase64[b0 >> 2];
            b64 += kBase64[(b0 & 3) << 4];
            b64 += "==";
        } else if (rem == 2) {
            b64 += kBase64[b0 >> 2];
            b64 += kBase64[((b0 & 3) << 4) | (b1 >> 4)];
            b64 += kBase64[((b1 & 15) << 2)];
            b64 += '=';
        }
        return b64;
    };
}

// Resolve the TARGET FIGURE from any graphics handle / figure number.
// exportgraphics(gca)/saveas(chart) scope to the axes in MATLAB; the
// portion-32 renderer works per-figure, so axes/chart targets map to
// their containing figure (gap noted in figure-export-gaps.md).
const FigureState *targetFigure(Engine &engine, const Value &v)
{
    auto &fm = engine.figureManager();
    const int hid = graphicsHandleIdOf(v);
    if (hid >= 0) {
        auto *rec = fm.findHandle(hid);
        if (!rec)
            throw std::runtime_error("Invalid or deleted object.");
        return fm.figureOf(rec->figureId);
    }
    if (v.isScalar())
        return fm.figureOf((int)v.toScalar());
    return nullptr;
}

struct ExportArgs {
    const FigureState *fig = nullptr;
    std::string path;
    std::string ext;        // from filename or explicit format
    int dpi = 150;          // MATLAB png default; scales the 840×630 base
};

// Shared argument walk for all three entry points: a handle/number
// target, one filename, format flags ('-dpng', 'png'), '-r###'
// resolution, and (ignored, accepted) Name-Value pairs.
ExportArgs parseExportArgs(Engine &engine, Span<const Value> args,
                            const char *who, bool needTarget)
{
    ExportArgs out;
    bool havePath = false, haveTarget = false;
    for (size_t i = 0; i < args.size(); ++i) {
        const Value &a = args[i];
        if (a.isObject() || (a.isScalar() && !a.isChar())) {
            if (!haveTarget) {
                out.fig = targetFigure(engine, a);
                if (!out.fig)
                    throw std::runtime_error(std::string(who)
                        + ": first argument must be a graphics handle or figure number");
                haveTarget = true;
            }
            continue;
        }
        if (!a.isChar() && !a.isString())
            continue;   // numeric noise (e.g. ignored Name-Value values)
        std::string s = a.toString();
        std::string sl = s;
        for (char &c : sl)
            c = (char)std::tolower((unsigned char)c);
        if (!s.empty() && s[0] == '-') {
            // print-style flags: -dpng / -dsvg / -r150 / -ochr…
            if (sl.size() > 2 && sl[1] == 'd')
                out.ext = sl.substr(2);
            else if (sl.size() > 2 && sl[1] == 'r')
                out.dpi = std::max(30, std::atoi(sl.c_str() + 2));
            // other print options (-painters, -opengl, -bestfit, …)
            // accepted and ignored
            continue;
        }
        // A char with an extension that is not a known bare FORMAT is
        // the filename; a bare known format is a format override.
        static const char *kFormats[] = {"png", "svg", "jpg", "jpeg",
                                         "bmp", "tga", "pnm", "ppm",
                                         "tif", "tiff"};
        bool isBareFormat = false;
        for (const char *f : kFormats)
            if (sl == f) {
                isBareFormat = true;
                break;
            }
        if (!havePath && !isBareFormat) {
            out.path = s;
            havePath = true;
        } else if (isBareFormat) {
            out.ext = sl;
        }
    }
    if (needTarget && !out.fig)
        out.fig = &engine.figureManager().current();
    if (!havePath)
        throw std::runtime_error(std::string(who)
            + ": a filename is required");
    if (out.ext.empty())
        out.ext = lowerExtOf(out.path);
    if (out.ext.empty())
        throw std::runtime_error(std::string(who)
            + ": cannot infer the output format from '" + out.path + "'");
    return out;
}

void runExport(Engine &engine, const ExportArgs &ea, const char *who)
{
    if (!ea.fig)
        throw std::runtime_error(std::string(who) + ": no figure to export");
    // Base canvas 840×630 (MATLAB's 560×420 pt @150dpi ≈ 825×619);
    // Resolution scales it linearly.
    const int W = std::max(64, (int)(840.0 * ea.dpi / 150.0));
    const int H = std::max(64, (int)(630.0 * ea.dpi / 150.0));

    auto rp = engine.resolvePath(ea.path);
    if (!rp.fs)
        throw std::runtime_error(std::string(who) + ": cannot resolve '"
                                 + ea.path + "'");

    if (ea.ext == "svg") {
        rp.fs->writeFile(rp.path,
                         figrender::renderFigureSvg(*ea.fig, W, H,
                                                     svgPngEmbedder(
                                                         engine.resource())));
        return;
    }
    if (ea.ext == "pdf" || ea.ext == "eps" || ea.ext == "eps2"
        || ea.ext == "fig" || ea.ext == "emf" || ea.ext == "hdf") {
        throw std::runtime_error(std::string(who) + ": format '."
            + ea.ext + "' is not supported yet (supported: .png .svg .jpg "
              ".bmp .tga .pnm .tif) — bugs/opened/graphics/"
              "figure-export-gaps.md");
    }
    // Raster formats → RGB buffer → image toolbox encoder.
    std::vector<std::uint8_t> rgb =
        figrender::renderFigureRgb(*ea.fig, W, H);
    const size_t Hp = (size_t)H, Wp = (size_t)W;
    Value A = Value::matrix3d(Hp, Wp, 3, ValueType::UINT8,
                              engine.resource());
    std::uint8_t *d = A.uint8DataMut();
    // Value is COLUMN-MAJOR (MATLAB layout): d[r + c*H + ch*H*W]. The
    // raster buffer is row-major top-down RGB — transpose into place
    // (filling it linearly scrambles the image into crisscrossing
    // diagonal noise).
    for (size_t r = 0; r < Hp; ++r)
        for (size_t c = 0; c < Wp; ++c)
            for (size_t ch = 0; ch < 3; ++ch)
                d[r + c * Hp + ch * Hp * Wp] =
                    rgb[(r * Wp + c) * 3 + ch];
    std::string bytes = image::imwriteToBytes(A, ea.ext, engine.resource());
    rp.fs->writeFileBytes(rp.path, bytes);
}

} // anonymous namespace

void registerFigureExport(Engine &engine)
{
    // saveas(h, filename [, fmt])
    {
        ExternalFunc fn = [&engine](Span<const Value> args, size_t,
                                    Span<Value> outs, CallContext &ctx) {
            if (args.size() < 1)
                throw std::runtime_error("saveas: requires (h, filename)");
            // saveas(filename) targets the current figure.
            bool firstIsTarget = args[0].isObject()
                                 || (args[0].isScalar() && !args[0].isChar());
            ExportArgs ea = parseExportArgs(
                *ctx.engine, args, "saveas", /*needTarget=*/!firstIsTarget);
            runExport(*ctx.engine, ea, "saveas");
            outs[0] = Value();
        };
        engine.registerFunction("", "saveas", std::move(fn));
    }
    // exportgraphics(obj, filename [, Name,Value…])
    {
        ExternalFunc fn = [](Span<const Value> args, size_t,
                             Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw std::runtime_error(
                    "exportgraphics: requires (obj, filename)");
            ExportArgs ea = parseExportArgs(*ctx.engine, args,
                                            "exportgraphics",
                                            /*needTarget=*/false);
            runExport(*ctx.engine, ea, "exportgraphics");
            outs[0] = Value();
        };
        engine.registerFunction("", "exportgraphics", std::move(fn));
    }
    // print(fig, filename, '-dpng' [, '-r###' …]) and the command form
    // `print -dpng file.png` (glue args arrive the same way).
    {
        ExternalFunc fn = [&engine](Span<const Value> args, size_t,
                                    Span<Value> outs, CallContext &ctx) {
            (void)engine;
            ExportArgs ea = parseExportArgs(*ctx.engine, args, "print",
                                            /*needTarget=*/true);
            runExport(*ctx.engine, ea, "print");
            outs[0] = Value();
        };
        engine.registerFunction("", "print", std::move(fn));
    }
}

} // namespace numkit
