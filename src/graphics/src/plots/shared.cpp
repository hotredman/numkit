// numkit/graphics — plots/shared.cpp
//
// Plot bodies shared across more than one family, as numkit::detail free
// functions so the line / bar / image builders can each call them:
//   heatmapImpl  — imagesc / pcolor (image) + histogram2 (bar)
//   geoForward   — geoplot (line) + geoscatter / geobubble (bar)
//   delegateTo   — comet* (line) + bubblechart / swarmchart / ... (bar)
// Carved out of buildPlotTable; bodies verbatim (pure helpers via header).

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

namespace numkit::detail {

void heatmapImpl(
                       const char *typeName,
                       bool axisIj,
                       Span<const Value> args, size_t nargout,
                       Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        if (args.empty()) {
            outs[0] = Value();
            return;
        }
        auto &fm = gc.fm;
        fm.prepareForPlot();
        // axis-ij must be set AFTER prepareForPlot — that call wipes
        // the axes state. Setting it here ensures the JSON emit at
        // the end of this function sees the right yDir.
        if (axisIj) {
            if (fm.currentAxes().axisMode.empty()) {
                fm.currentAxes().axisMode = "ij";
            }
            fm.currentAxes().yDir = "reverse";
        }

        const Value *C_arg = nullptr;
        const Value *x_arg = nullptr;
        const Value *y_arg = nullptr;

        if (args.size() == 1) {
            C_arg = &args[0];
        } else if (args.size() == 2 && args[1].numel() == 2) {
            C_arg = &args[0];
            fm.currentAxes().climJson = vecToJson(args[1]);
        } else if (args.size() >= 3) {
            x_arg = &args[0];
            y_arg = &args[1];
            C_arg = &args[2];
            if (args.size() >= 4 && args[3].numel() == 2) {
                fm.currentAxes().climJson = vecToJson(args[3]);
            }
        }

        if (!C_arg) {
            outs[0] = Value();
            return;
        }

        size_t rows = C_arg->dims().rows();
        size_t cols = C_arg->dims().cols();
        if (rows == 0 || cols == 0) {
            outs[0] = Value();
            return;
        }

        DatasetInfo ds;
        ds.type = typeName;
        ds.originalRows = rows;
        ds.originalCols = cols;

        // colorScale axes-state was set by `colorscale('log')` *before*
        // this imagesc — bake log10 into the quantization. Non-positive
        // and non-finite values become NaN (rendered transparent), not
        // clamped to a fake "very negative dB" — clamping would skew
        // cmin and waste the colour range on phantom data.
        const bool logColor = (fm.currentAxes().colorScale == "log");
        ds.colorScaleBaked = logColor;

        const auto getRaw = [&](size_t idx) -> double {
            if (C_arg->isComplex())
                return std::abs(C_arg->complexData()[idx]);
            return C_arg->doubleData()[idx];
        };
        const auto getVal = [&](size_t idx) -> double {
            const double r = getRaw(idx);
            if (!std::isfinite(r)) return std::numeric_limits<double>::quiet_NaN();
            if (!logColor) return r;
            if (r <= 0.0) return std::numeric_limits<double>::quiet_NaN();
            return std::log10(r);
        };

        // Pass 1: scan for cmin/cmax (skipping NaN/Inf). When logColor
        // is on these are already in log10 space.
        double cmin = std::numeric_limits<double>::infinity();
        double cmax = -std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < rows * cols; ++i) {
            const double v = getVal(i);
            if (std::isfinite(v)) {
                if (v < cmin) cmin = v;
                if (v > cmax) cmax = v;
            }
        }
        // climJson if set has shape "[a,b]" — parse it for the user
        // override. clim is in original-domain values; in log mode we
        // need to take log10 of both limits for the quantization range.
        if (!fm.currentAxes().climJson.empty()) {
            const std::string &s = fm.currentAxes().climJson;
            size_t lb = s.find('[');
            size_t comma = s.find(',', lb);
            size_t rb = s.find(']', comma);
            if (lb != std::string::npos && comma != std::string::npos
                && rb != std::string::npos) {
                try {
                    double a = std::stod(s.substr(lb + 1, comma - lb - 1));
                    double b = std::stod(s.substr(comma + 1, rb - comma - 1));
                    if (logColor) {
                        if (a > 0 && b > 0) {
                            cmin = std::log10(a);
                            cmax = std::log10(b);
                        }
                        // else: keep scanned (log) values — user set bad clim for log
                    } else {
                        cmin = a;
                        cmax = b;
                    }
                } catch (...) { /* keep scanned values */ }
            }
        }
        if (!std::isfinite(cmin) || !std::isfinite(cmax) || cmin == cmax) {
            cmin = 0.0;
            cmax = 1.0;
        }
        ds.cminOrig = cmin;
        ds.cmaxOrig = cmax;
        // ds.colorScaleBaked already set above based on logColor flag.

        // Pass 2: quantize every cell to uint8. Index 0..254 = data range,
        // 255 = NaN/Inf sentinel. Column-major to match MATLAB.
        const double range = cmax - cmin;
        const double qScale = (range > 0) ? 254.0 / range : 0.0;
        ds.zQuantized.resize(rows * cols);
        for (size_t i = 0; i < rows * cols; ++i) {
            const double v = getVal(i);
            if (!std::isfinite(v)) {
                ds.zQuantized[i] = 255;
            } else {
                double t = (v - cmin) * qScale;
                if (t < 0) t = 0;
                else if (t > 254) t = 254;
                ds.zQuantized[i] = static_cast<uint8_t>(t + 0.5);
            }
        }

        // Inline-JSON preview: emit uint8 indices (1-3 chars per cell)
        // rather than doubles. Same ≤2M-cells cap as before — large
        // matrices get mean-pooled in index space (idx 255 NaN-skipped).
        constexpr size_t MAX_INLINE_CELLS = 2'000'000;
        const size_t totalCells = rows * cols;
        std::ostringstream zs;
        zs << "[";

        if (totalCells <= MAX_INLINE_CELLS) {
            for (size_t r = 0; r < rows; ++r) {
                if (r) zs << ",";
                zs << "[";
                for (size_t c = 0; c < cols; ++c) {
                    if (c) zs << ",";
                    zs << static_cast<int>(ds.zQuantized[c * rows + r]);
                }
                zs << "]";
            }
        } else {
            size_t step = 1;
            while ((rows + step - 1) / step * ((cols + step - 1) / step) > MAX_INLINE_CELLS) {
                ++step;
            }
            const size_t dr = step;
            const size_t dc = step;
            const size_t outRows = (rows + dr - 1) / dr;
            const size_t outCols = (cols + dc - 1) / dc;

            for (size_t orow = 0; orow < outRows; ++orow) {
                if (orow) zs << ",";
                zs << "[";
                const size_t r0 = orow * dr;
                const size_t r1 = std::min(rows, r0 + dr);
                for (size_t ocol = 0; ocol < outCols; ++ocol) {
                    if (ocol) zs << ",";
                    const size_t c0 = ocol * dc;
                    const size_t c1 = std::min(cols, c0 + dc);
                    int sum = 0;
                    int n = 0;
                    for (size_t c = c0; c < c1; ++c) {
                        for (size_t r = r0; r < r1; ++r) {
                            const uint8_t q = ds.zQuantized[c * rows + r];
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

        if (x_arg && x_arg->numel() >= 2) {
            ds.xJson = vecToJson(*x_arg);
        } else {
            std::ostringstream xs;
            xs << "[1," << cols << "]";
            ds.xJson = xs.str();
        }
        if (y_arg && y_arg->numel() >= 2) {
            ds.yJson = vecToJson(*y_arg);
        } else {
            std::ostringstream ys;
            ys << "[1," << rows << "]";
            ds.yJson = ys.str();
        }

        fm.pushDataset(std::move(ds));
        // NOTE: axis-ij setting moved OUT of heatmapImpl. Setting yDir
        // here would ride on emitModified() below, freezing it into
        // the JSON before histogram2 (which delegates here) gets a
        // chance to override. The imagesc wrapper now applies axis ij
        // BEFORE delegating; histogram2 leaves yDir alone.
        fm.emitModified();
        outs[0] = Value();
}  // heatmapImpl

void geoForward(const char *target, Span<const Value> args,
                     Span<Value> outs, GraphicsContext &gc) {
    if (args.size() < 2) { outs[0] = Value(); return; }
    // Build a new arg list with (lon, lat, ...) — i.e. swap the
    // first two so target's X = lon, Y = lat.
    std::vector<Value> proxied;
    proxied.reserve(args.size());
    proxied.push_back(args[1]);   // lon → X
    proxied.push_back(args[0]);   // lat → Y
    for (size_t i = 2; i < args.size(); ++i) proxied.push_back(args[i]);
    std::array<Value, 4> outBuf;
    if (!gc.callBuiltin(target, Span<const Value>(proxied.data(), proxied.size()), 0,
                        Span<Value>(outBuf.data(), 1))) { outs[0] = Value(); return; }
    // Add convenience axis labels so the "no basemap" output is
    // self-explanatory (lat/lon instead of generic X/Y).
    auto &ax = gc.fm.currentAxes();
    if (ax.xlabel.empty()) ax.xlabel = "lon";
    if (ax.ylabel.empty()) ax.ylabel = "lat";
    gc.fm.current().modified = true;
    gc.fm.emitModified();
    outs[0] = Value();
}

void delegateTo(const char *target, Span<const Value> args,
                     Span<Value> outs, GraphicsContext &gc) {
    std::array<Value, 1> outBuf;
    if (!gc.callBuiltin(target, args, 0, Span<Value>(outBuf.data(), 1))) { outs[0] = Value(); return; }
    outs[0] = Value();
}

}  // namespace numkit::detail
