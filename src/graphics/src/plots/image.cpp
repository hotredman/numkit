// numkit/graphics — image.cpp
//
// graphics.image.* builders (imagesc / pcolor / imshow), carved out of plots.cpp. Core-free bodies (GraphicsContext); shared
// JSON/parse helpers come from plot_internal.hpp.

#include "plot_internal.hpp"
#include <numkit/graphics/graphics_context.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

namespace numkit {

void buildImagePlots(std::vector<PlotEntry> &table)
{
    using namespace detail;
    auto reg = [&](const char *sub, const char *name, GraphicsFn fn) {
        table.push_back(PlotEntry{sub, name, /*core=*/false, std::move(fn)});
    };

    // Shared body for any heatmap-like builtin (imagesc / pcolor).
    // The data-emission path is identical; only the `type` field
    // differs (renderer uses it to pick cell-centre vs cell-vertex
    // alignment). Body kept as a captured lambda + std::bind to avoid
    // duplicating ~200 lines of quantization logic.
    using namespace std::placeholders;
    // imagesc auto-applies MATLAB axis ij (yDir='reverse', matrix-row-1
    // at top). pcolor stays on axis xy. histogram2 (delegates separately)
    // also stays on axis xy. The axis-ij flag is the second positional
    // arg of heatmapImpl now — it's set AFTER prepareForPlot inside.
    reg("image", "imagesc", std::bind(heatmapImpl, "imagesc", true,  _1, _2, _3, _4));
    reg("image", "pcolor",  std::bind(heatmapImpl, "pcolor",  false, _1, _2, _3, _4));
    // ────────────────────────────────────────────────────────────────
    // imshow — display image. MATLAB:
    //   imshow(I)            — grayscale, range = type-default
    //                          (uint8 → [0,255], double/single → [0,1],
    //                           logical → [0,1])
    //   imshow(I, [lo hi])   — grayscale with explicit display range
    //   imshow(I, [])        — grayscale auto-range (== imagesc behaviour)
    //   imshow(RGB)          — truecolor, RGB is M×N×3 (uint8 or double).
    //                          double in [0,1] → cast *255 to uint8.
    // Compared to imagesc, imshow:
    //   • defaults colormap to "gray" (grayscale only; RGB ignores cmap)
    //   • forces axisMode='image' (1:1 pixels, equal aspect)
    //   • forces axisVisible=false (no ticks / labels / box)
    //   • forces yDir='reverse' (matrix-row=1 at top)
    // Existing user-set values for colormap/axisMode survive.
    // Deferred: filename input,
    // 'XData','YData','InitialMagnification','Border','Reduce',
    // 'Colormap' name-value pairs.
    auto imshowImpl = [](Span<const Value> args, size_t nargout,
                         Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        auto &fm = gc.fm;
        fm.prepareForPlot();
        auto &ax = fm.currentAxes();

        if (args.empty()) { outs[0] = Value(); return; }

        // imshow('path/to/img.png') — decode the file by calling the registered
        // `imread` builtin THROUGH A HANDLE resolved by name at call time. This
        // keeps graphics free of any image-toolbox C++ dependency (no include,
        // no numkit::image:: symbol); imread is installed by the image toolbox
        // in the same standard library and resolves at runtime.
        Value decoded;   // owns the lifetime of the decoded value
        const Value *img0 = &args[0];
        if (args[0].isChar() || args[0].isString()) {
            Value imreadFn = Value::funcHandle("imread", gc.mr);
            decoded = gc.callHandle(imreadFn,
                                                     Span<const Value>(&args[0], 1));
            img0 = &decoded;
        }
        const Value &I = *img0;
        const auto &dims = I.dims();
        const int nd = dims.ndims();
        const size_t R = dims.rows();
        const size_t C = dims.cols();

        // Detect RGB / RGBA: 3-D with 3 or 4 pages.
        const size_t pages = (nd == 3) ? dims.pages() : 0;
        const bool isRGB  = (pages == 3 || pages == 4);
        const bool hasAlpha = (pages == 4);
        if (!isRGB && (nd != 2 || R == 0 || C == 0)) {
            outs[0] = Value();
            return;
        }

        const ValueType vt = I.type();

        // ── Argument parsing ────────────────────────────────────────
        // Supported forms:
        //   imshow(I)
        //   imshow(I, [lo hi])              — explicit display range
        //   imshow(I, [])                    — auto-range (data extent)
        //   imshow(I, 'DisplayRange', [lo hi])
        //   imshow(I, 'XData', xv, 'YData', yv, ...)
        //   imshow(I, [lo hi], 'XData', xv, ...)
        // After args[0], a non-char Value is a positional range; the
        // first char Value starts N-V parsing.
        const Value *rangeArg = nullptr;       // [] / [lo hi] / null
        const Value *xDataArg = nullptr;
        const Value *yDataArg = nullptr;
        std::string colormapArg;               // 'Colormap', 'gray' / 'jet' / ...
        size_t nvStart = 1;
        if (args.size() >= 2 && !args[1].isChar()) {
            rangeArg = &args[1];
            nvStart = 2;
        }
        for (size_t i = nvStart; i + 1 < args.size(); i += 2) {
            if (!args[i].isChar()) continue;
            std::string key = args[i].toString();
            for (auto &c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (key == "displayrange")    rangeArg = &args[i + 1];
            else if (key == "xdata")      xDataArg = &args[i + 1];
            else if (key == "ydata")      yDataArg = &args[i + 1];
            else if (key == "colormap" && args[i + 1].isChar())
                colormapArg = args[i + 1].toString();
            // Border / Reduce / InitialMagnification / Parent: accepted
            // for script compatibility but currently no-op since the
            // renderer always fits the image to the panel and applies
            // the existing >2M-pixel mean-pool downsample. Calls don't
            // crash; visual effect is BACKLOG.
            else if (key == "border" || key == "reduce"
                     || key == "initialmagnification" || key == "parent") {
                // accept-and-ignore
            }
        }

        // Helper: emit "[v1,v2,...,vN]" JSON from a Value (numeric vec).
        auto vecJson = [](const Value &v) -> std::string {
            std::ostringstream os; os << "[";
            const size_t n = v.numel();
            for (size_t i = 0; i < n; ++i) {
                if (i) os << ",";
                os << v.elemAsDouble(i);
            }
            os << "]";
            return os.str();
        };
        // X/Y span. By default 1..C / 1..R; XData/YData override.
        std::string xJson, yJson;
        if (xDataArg && xDataArg->numel() >= 2) {
            xJson = vecJson(*xDataArg);
        } else {
            std::ostringstream xs; xs << "[1," << C << "]"; xJson = xs.str();
        }
        if (yDataArg && yDataArg->numel() >= 2) {
            yJson = vecJson(*yDataArg);
        } else {
            std::ostringstream ys; ys << "[1," << R << "]"; yJson = ys.str();
        }

        if (isRGB) {
            DatasetInfo ds;
            ds.type = "image-rgb";
            ds.originalRows = R;
            ds.originalCols = C;
            // Cast to uint8. uint8/uint16 etc. as-is (clamped); float
            // / logical → multiply by 255 then clamp.
            const double scale = (vt == ValueType::DOUBLE
                                  || vt == ValueType::SINGLE
                                  || vt == ValueType::LOGICAL) ? 255.0 : 1.0;
            // Wire format always carries 4 bytes per pixel (RGBA). For
            // RGB input we synthesize alpha=255. Renderer treats a
            // missing alpha as opaque, so RGB-only callers see no
            // change.
            const size_t bytesPerPixel = 4;
            ds.rgbBytes.resize(R * C * bytesPerPixel);
            const size_t numChannels = hasAlpha ? 4 : 3;
            for (size_t r = 0; r < R; ++r) {
                for (size_t c = 0; c < C; ++c) {
                    const size_t outIdx = (r * C + c) * bytesPerPixel;
                    for (size_t k = 0; k < numChannels; ++k) {
                        // column-major linear index: page * R*C + col * R + row
                        const size_t srcIdx = k * R * C + c * R + r;
                        double v = I.elemAsDouble(srcIdx) * scale;
                        if (v < 0) v = 0;
                        else if (v > 255) v = 255;
                        ds.rgbBytes[outIdx + k] = static_cast<uint8_t>(v + 0.5);
                    }
                    // Synthesize opaque alpha when input is plain RGB.
                    if (!hasAlpha) ds.rgbBytes[outIdx + 3] = 255;
                }
            }
            // Inline JSON preview — same 2M-pixel cap as imagesc; mean-pool
            // each channel independently when over the cap. Wire shape:
            // [[[r,g,b,a],...],...]  (always 4-tuple, alpha=255 for RGB).
            constexpr size_t MAX_INLINE_PIXELS = 2'000'000;
            const size_t totalPixels = R * C;
            std::ostringstream zs;
            zs << "[";
            if (totalPixels <= MAX_INLINE_PIXELS) {
                for (size_t r = 0; r < R; ++r) {
                    if (r) zs << ",";
                    zs << "[";
                    for (size_t c = 0; c < C; ++c) {
                        if (c) zs << ",";
                        const size_t off = (r * C + c) * bytesPerPixel;
                        zs << "[" << static_cast<int>(ds.rgbBytes[off])
                           << "," << static_cast<int>(ds.rgbBytes[off + 1])
                           << "," << static_cast<int>(ds.rgbBytes[off + 2])
                           << "," << static_cast<int>(ds.rgbBytes[off + 3]) << "]";
                    }
                    zs << "]";
                }
            } else {
                size_t step = 1;
                while ((R + step - 1) / step * ((C + step - 1) / step) > MAX_INLINE_PIXELS)
                    ++step;
                const size_t outR = (R + step - 1) / step;
                const size_t outC = (C + step - 1) / step;
                for (size_t orow = 0; orow < outR; ++orow) {
                    if (orow) zs << ",";
                    zs << "[";
                    const size_t r0 = orow * step;
                    const size_t r1 = std::min(R, r0 + step);
                    for (size_t ocol = 0; ocol < outC; ++ocol) {
                        if (ocol) zs << ",";
                        const size_t c0 = ocol * step;
                        const size_t c1 = std::min(C, c0 + step);
                        int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
                        int n = 0;
                        for (size_t rr = r0; rr < r1; ++rr) {
                            for (size_t cc = c0; cc < c1; ++cc) {
                                const size_t off = (rr * C + cc) * bytesPerPixel;
                                sumR += ds.rgbBytes[off];
                                sumG += ds.rgbBytes[off + 1];
                                sumB += ds.rgbBytes[off + 2];
                                sumA += ds.rgbBytes[off + 3];
                                ++n;
                            }
                        }
                        zs << "[" << (n ? sumR / n : 0)
                           << "," << (n ? sumG / n : 0)
                           << "," << (n ? sumB / n : 0)
                           << "," << (n ? sumA / n : 255) << "]";
                    }
                    zs << "]";
                }
                ds.downsampled = true;
            }
            zs << "]";
            ds.rgbJson = zs.str();
            ds.xJson = xJson;
            ds.yJson = yJson;
            fm.pushDataset(std::move(ds));
        } else {
            // Grayscale path. Pick [cmin, cmax] from class default unless
            // overridden by `imshow(I, [lo hi])` (positional or
            // 'DisplayRange') or auto-ranged via `imshow(I, [])`.
            double cmin = 0.0, cmax = 1.0;
            if (vt == ValueType::UINT8) { cmin = 0; cmax = 255; }
            // logical / double / single → already [0, 1].
            bool autoRange = false;
            if (rangeArg) {
                if (rangeArg->numel() == 0) {
                    autoRange = true;
                } else if (rangeArg->numel() >= 2) {
                    cmin = rangeArg->elemAsDouble(0);
                    cmax = rangeArg->elemAsDouble(1);
                }
            }
            if (autoRange) {
                double mn = std::numeric_limits<double>::infinity();
                double mx = -std::numeric_limits<double>::infinity();
                for (size_t i = 0; i < R * C; ++i) {
                    double v = I.elemAsDouble(i);
                    if (std::isfinite(v)) {
                        if (v < mn) mn = v;
                        if (v > mx) mx = v;
                    }
                }
                if (!std::isfinite(mn)) { mn = 0; mx = 1; }
                cmin = mn; cmax = mx;
            }
            // Quantize with FORCED [cmin, cmax]. Doesn't reuse heatmapImpl
            // because that one always derives the range from data extent.
            DatasetInfo ds;
            ds.type = "imagesc";
            ds.originalRows = R;
            ds.originalCols = C;
            ds.cminOrig = cmin;
            ds.cmaxOrig = cmax;
            const double range = cmax - cmin;
            const double qScale = (range > 0) ? 254.0 / range : 0.0;
            ds.zQuantized.resize(R * C);
            for (size_t i = 0; i < R * C; ++i) {
                const double v = I.elemAsDouble(i);
                if (!std::isfinite(v)) {
                    ds.zQuantized[i] = 255;
                } else {
                    double t = (v - cmin) * qScale;
                    if (t < 0) t = 0;
                    else if (t > 254) t = 254;
                    ds.zQuantized[i] = static_cast<uint8_t>(t + 0.5);
                }
            }
            constexpr size_t MAX_INLINE_CELLS = 2'000'000;
            const size_t totalCells = R * C;
            std::ostringstream zs;
            zs << "[";
            if (totalCells <= MAX_INLINE_CELLS) {
                for (size_t r = 0; r < R; ++r) {
                    if (r) zs << ",";
                    zs << "[";
                    for (size_t c = 0; c < C; ++c) {
                        if (c) zs << ",";
                        zs << static_cast<int>(ds.zQuantized[c * R + r]);
                    }
                    zs << "]";
                }
            } else {
                size_t step = 1;
                while ((R + step - 1) / step * ((C + step - 1) / step) > MAX_INLINE_CELLS)
                    ++step;
                const size_t outR = (R + step - 1) / step;
                const size_t outC = (C + step - 1) / step;
                for (size_t orow = 0; orow < outR; ++orow) {
                    if (orow) zs << ",";
                    zs << "[";
                    const size_t r0 = orow * step;
                    const size_t r1 = std::min(R, r0 + step);
                    for (size_t ocol = 0; ocol < outC; ++ocol) {
                        if (ocol) zs << ",";
                        const size_t c0 = ocol * step;
                        const size_t c1 = std::min(C, c0 + step);
                        int sum = 0, n = 0;
                        for (size_t cc = c0; cc < c1; ++cc) {
                            for (size_t rr = r0; rr < r1; ++rr) {
                                const uint8_t q = ds.zQuantized[cc * R + rr];
                                if (q != 255) { sum += q; ++n; }
                            }
                        }
                        zs << ((n > 0) ? (sum + n / 2) / n : 255);
                    }
                    zs << "]";
                }
                ds.downsampled = true;
            }
            zs << "]";
            ds.zJson = zs.str();
            ds.xJson = xJson;
            ds.yJson = yJson;
            fm.pushDataset(std::move(ds));
            // Grayscale-only colormap. 'Colormap' N-V wins; otherwise
            // default to 'gray' if axis hasn't seen one yet. RGB
            // ignores colormap so we don't touch it on the RGB branch.
            if (!colormapArg.empty()) ax.colormapName = colormapArg;
            else if (ax.colormapName.empty()) ax.colormapName = "gray";
        }

        // Common imshow axes config. axisMode='image' iff not already set
        // (tests want 'image' on default, but a user-set 'square' or
        // 'tight' should win). axisVisible and yDir are unconditional —
        // imshow's defining trait is "no axes, image orientation".
        if (ax.axisMode.empty()) ax.axisMode = "image";
        ax.axisVisible = false;
        ax.yDir = "reverse";

        fm.current().modified = true;
        fm.emitModified();
        outs[0] = Value();
    };
    reg("image", "imshow", imshowImpl);
}

}  // namespace numkit
