// numkit/graphics — layout.cpp
//
// graphics.layout.* builders (figure / subplot / axes / labels / limits / legend / ticks / colormap), carved out of plots.cpp. Core-free bodies (GraphicsContext); shared
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

void buildLayoutPlots(std::vector<PlotEntry> &table)
{
    using namespace detail;
    auto reg = [&](const char *sub, const char *name, GraphicsFn fn) {
        table.push_back(PlotEntry{sub, name, /*core=*/false, std::move(fn)});
    };
    auto regCore = [&](const char *sub, const char *name, GraphicsFn fn) {
        table.push_back(PlotEntry{sub, name, /*core=*/true, std::move(fn)});
    };

    regCore("layout", "figure",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto *mr = gc.mr;
            auto &fm = gc.fm;
            int id;
            if (args.empty()) {
                id = fm.newFigure();
            } else {
                id = static_cast<int>(args[0].toScalar());
                fm.setFigure(id);
            }
            fm.current().modified = true;
            fm.emitModified();
            if (nargout > 0)
                outs[0] = Value::scalar(static_cast<double>(id), mr);
        });
    regCore("layout", "close",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto &fm = gc.fm;
            if (args.empty()) {
                fm.closeCurrentNotify();
            } else if (args[0].isChar() && args[0].toString() == "all") {
                fm.closeAllNotify();
            } else {
                int id = static_cast<int>(args[0].toScalar());
                fm.closeFigureNotify(id);
            }
            outs[0] = Value();
        });
    reg("layout", "clf",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto &fm = gc.fm;
            auto &fig = fm.current();
            fig.axes.clear();
            fig.axes.push_back(AxesState{});
            fig.currentAxes = 0;
            fig.subplotRows = 0;
            fig.subplotCols = 0;
            fig.modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    regCore("layout", "hold",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto &ax = gc.fm.currentAxes();
            if (args.empty())
                ax.holdOn = !ax.holdOn;
            else
                ax.holdOn = (args[0].toString() == "on");
            outs[0] = Value();
        });
    reg("layout", "subplot",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (args.size() < 3) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
            int m = static_cast<int>(args[0].toScalar());
            int n = static_cast<int>(args[1].toScalar());
            int p = static_cast<int>(args[2].toScalar());
            fm.setSubplot(m, n, p);
            outs[0] = Value();
        });
    // tiledlayout(m, n[, ...]) — modern alternative to subplot. We
    // store the grid shape on the FigureState so subsequent nexttile
    // calls can step through cells. Trailing N-V pairs ('Padding',
    // 'TileSpacing', 'TileIndexing') are accepted but currently no-op
    // (the IDE always renders subplot cells with the same fixed gap).
    reg("layout", "tiledlayout",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            int m = 1, n = 1;
            if (args.size() >= 1 && !args[0].isChar()) m = (int)args[0].toScalar();
            if (args.size() >= 2 && !args[1].isChar()) n = (int)args[1].toScalar();
            if (m < 1) m = 1;
            if (n < 1) n = 1;
            auto &fm = gc.fm;
            // setSubplot(m, n, 1) reserves the grid + activates cell 1.
            fm.setSubplot(m, n, 1);
            outs[0] = Value();
        });
    // nexttile([span]) — bumps the active subplot cell index by 1
    // (or by `span` if given a numeric arg). When the figure has no
    // tiledlayout grid yet, the call is a no-op.
    reg("layout", "nexttile",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            auto &fm = gc.fm;
            auto &fig = fm.current();
            if (fig.subplotRows <= 0 || fig.subplotCols <= 0) {
                // No tiledlayout active; default to a 1x1 grid so
                // the first nexttile creates a single cell.
                fm.setSubplot(1, 1, 1);
                outs[0] = Value();
                return;
            }
            const int total = fig.subplotRows * fig.subplotCols;
            // Determine target cell. With a numeric arg, jump to that
            // cell (1-based); else advance by 1 from the current.
            int target = fig.currentAxes + 2;   // (currentAxes is 0-based)
            if (!args.empty() && !args[0].isChar()) {
                target = (int)args[0].toScalar();
            }
            if (target < 1) target = 1;
            if (target > total) target = total;
            fm.setSubplot(fig.subplotRows, fig.subplotCols, target);
            outs[0] = Value();
        });
    reg("layout", "title",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty()) {
                auto &fm = gc.fm;
                fm.currentAxes().title = argStr(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    reg("layout", "subtitle",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty()) {
                auto &fm = gc.fm;
                fm.currentAxes().subtitle = argStr(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    // sgtitle — figure-level "super title". Writes to
    // FigureState.superTitle (a dedicated slot, separate from per-axes
    // Title.String). For non-subplot figures the IDE still renders it
    // as the panel's top header; for subplots it spans the whole grid
    // above the cell row — matching MATLAB R2025b's sgtitle behaviour.
    reg("layout", "sgtitle",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty()) {
                auto &fm = gc.fm;
                fm.current().superTitle = argStr(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    reg("layout", "xlabel",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty()) {
                auto &fm = gc.fm;
                fm.currentAxes().xlabel = argStr(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    // text(x, y, str, ...) — annotation overlay. Each call appends ONE
    // text dataset (single-point) to the current axes. For arrays, the
    // user iterates via for-loop in script. Trailing name-value pairs
    // (Color, FontSize) parsed minimally — extra pairs are ignored.
    // The IDE renders text overlays after the image / line layers so
    // labels stay on top of imagesc / scatter.
    reg("layout", "text",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (args.size() < 3) { outs[0] = Value(); return; }
            const Value &xv = args[0];
            const Value &yv = args[1];
            if (!xv.numel() || !yv.numel()) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
            // text() does NOT call prepareForPlot — it's an annotation,
            // it should append to whatever's already there even without
            // explicit `hold on`.
            DatasetInfo ds;
            ds.type = "text";
            std::ostringstream xs, ys;
            xs << "[" << xv.doubleData()[0] << "]";
            ys << "[" << yv.doubleData()[0] << "]";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.label = argStr(args[2]);
            // Parse trailing name-value pairs for color / fontsize.
            // Pack into ds.style as a compact "color=#rrggbb;fontSize=N" string
            // — IDE side knows to split it.
            std::string extras;
            for (size_t i = 3; i + 1 < args.size(); i += 2) {
                std::string key = argStr(args[i]);
                for (auto &c : key) c = std::tolower(c);
                if (key == "color") {
                    if (!extras.empty()) extras += ";";
                    extras += "color=" + argStr(args[i + 1]);
                } else if (key == "fontsize") {
                    if (!extras.empty()) extras += ";";
                    std::ostringstream fs;
                    fs << "fontSize=" << args[i + 1].doubleData()[0];
                    extras += fs.str();
                }
            }
            ds.style = extras;
            fm.pushDataset(std::move(ds));
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    reg("layout", "ylabel",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty()) {
                auto &fm = gc.fm;
                auto &ax = fm.currentAxes();
                // yyaxis: route to the active side's label slot.
                if (ax.yyEnabled && ax.activeYside == "right")
                    ax.ylabel2 = argStr(args[0]);
                else
                    ax.ylabel = argStr(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    reg("layout", "xlim",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty() && args[0].numel() >= 2) {
                auto &fm = gc.fm;
                fm.currentAxes().xlimJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    reg("layout", "ylim",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty() && args[0].numel() >= 2) {
                auto &fm = gc.fm;
                auto &ax = fm.currentAxes();
                if (ax.yyEnabled && ax.activeYside == "right")
                    ax.ylim2Json = vecToJson(args[0]);
                else
                    ax.ylimJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    reg("layout", "grid",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto &fm = gc.fm;
            auto &ax = fm.currentAxes();
            // MATLAB semantics:
            //   grid          → toggle MAJOR (minor untouched)
            //   grid on       → major on
            //   grid off      → both off
            //   grid minor    → toggle MINOR (major untouched)
            // Major and minor are independent booleans; the script can
            // hold "minor only" (rare but valid in MATLAB).
            if (args.empty()) {
                ax.gridMajor = !ax.gridMajor;
            } else if (args[0].isChar()) {
                std::string arg = args[0].toString();
                for (auto &c : arg) c = (char)std::tolower((unsigned char)c);
                if      (arg == "on")    { ax.gridMajor = true; }
                else if (arg == "off")   { ax.gridMajor = false; ax.gridMinor = false; }
                else if (arg == "minor") { ax.gridMinor = !ax.gridMinor; }
            }
            ax.gridUserTouched = true;
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    // Normalise a Location string ("NorthWest" → "northwest"). MATLAB
    // accepts mixed case; we always lowercase. Empty / unrecognised
    // strings clear the field so the renderer falls back to its default.
    auto normLocation = [](const std::string &raw) -> std::string {
        std::string s;
        s.reserve(raw.size());
        for (char c : raw) s.push_back((char)std::tolower((unsigned char)c));
        // Allowed values, mirroring MATLAB:
        //   north, south, east, west, best, none,
        //   northeast, northwest, southeast, southwest,
        //   plus the same with `outside` suffix.
        static const std::array<const char *, 19> kAllowed{
            "north", "south", "east", "west", "best", "none",
            "northeast", "northwest", "southeast", "southwest",
            "northoutside", "southoutside", "eastoutside", "westoutside",
            "northeastoutside", "northwestoutside",
            "southeastoutside", "southwestoutside",
            "bestoutside",
        };
        for (auto p : kAllowed)
            if (s == p) return s;
        return std::string();
    };
    reg("layout", "legend",
        [normLocation](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto &fm = gc.fm;
            auto &ax = fm.currentAxes();
            // Walk args. Plain strings become legend labels; a `'Location'`
            // / 'location' marker captures the next arg as the location.
            // Other (Name, Value) pairs (FontSize, NumColumns, ...) are
            // not yet honoured but parsed-and-skipped so they don't
            // pollute legendLabels.
            // Special arg: 'boxoff'/'boxon' alone should NOT clear
            // existing labels (it's a frame-only toggle). Detect that
            // up front before zapping legendLabels.
            bool onlyBoxToggle = !args.empty();
            for (size_t i = 0; i < args.size() && onlyBoxToggle; ++i) {
                if (!args[i].isChar()) { onlyBoxToggle = false; break; }
                std::string s = args[i].toString();
                std::string sl;
                for (char c : s) sl.push_back((char)std::tolower((unsigned char)c));
                if (sl != "boxoff" && sl != "boxon") { onlyBoxToggle = false; break; }
            }
            if (!onlyBoxToggle) ax.legendLabels.clear();
            // Don't reset legendLocation if the user calls bare `legend(...)`
            // again — but DO accept an explicit override.
            bool sawLocation = false;
            std::string newLoc;
            for (size_t i = 0; i < args.size(); ++i) {
                if (!args[i].isChar()) {
                    ax.legendLabels.push_back(argStr(args[i]));
                    continue;
                }
                std::string s = args[i].toString();
                std::string sLower;
                sLower.reserve(s.size());
                for (char c : s) sLower.push_back((char)std::tolower((unsigned char)c));
                if (sLower == "location" && i + 1 < args.size() && args[i + 1].isChar()) {
                    sawLocation = true;
                    newLoc = normLocation(args[i + 1].toString());
                    ++i;  // skip the value
                    continue;
                }
                // MATLAB also accepts `'show'` / `'hide'` / `'off'` as the
                // first arg. We treat 'off'/'hide' as clear-and-stop, the
                // rest fall through as labels.
                if (i == 0 && (sLower == "off" || sLower == "hide")) {
                    ax.legendLocation.clear();
                    fm.current().modified = true;
                    fm.emitModified();
                    outs[0] = Value();
                    return;
                }
                if (i == 0 && (sLower == "show" || sLower == "on")) {
                    // No labels passed — keep whatever was set previously.
                    continue;
                }
                // 'boxoff' / 'boxon' — toggle the legend frame.
                if (sLower == "boxoff") {
                    ax.legendBoxOn = false;
                    continue;
                }
                if (sLower == "boxon") {
                    ax.legendBoxOn = true;
                    continue;
                }
                ax.legendLabels.push_back(s);
            }
            if (sawLocation)
                ax.legendLocation = newLoc;
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    reg("layout", "colorbar",
        [normLocation](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto &fm = gc.fm;
            auto &ax = fm.currentAxes();
            // Default: presence of the call enables the bar at the
            // renderer's default location ('east'). Subsequent calls
            // can move it via 'Location', '<pos>'.
            if (ax.colorbarLocation.empty())
                ax.colorbarLocation = "east";
            for (size_t i = 0; i < args.size(); ++i) {
                if (!args[i].isChar()) continue;
                std::string s = args[i].toString();
                std::string sLower;
                sLower.reserve(s.size());
                for (char c : s) sLower.push_back((char)std::tolower((unsigned char)c));
                if (sLower == "location" && i + 1 < args.size() && args[i + 1].isChar()) {
                    auto loc = normLocation(args[i + 1].toString());
                    if (!loc.empty()) ax.colorbarLocation = loc;
                    ++i;
                    continue;
                }
                if (sLower == "off" || sLower == "hide") {
                    ax.colorbarLocation.clear();
                    fm.current().modified = true;
                    fm.emitModified();
                    outs[0] = Value();
                    return;
                }
            }
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    reg("layout", "axis",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty() && args[0].isChar()) {
                auto &fm = gc.fm;
                auto &ax = fm.currentAxes();
                std::string mode = args[0].toString();
                // `axis off` / `axis on` flip axisVisible without
                // touching axisMode; MATLAB allows them to coexist
                // with `axis equal`/`image`/etc. via successive calls.
                if (mode == "off") {
                    ax.axisVisible = false;
                } else if (mode == "on") {
                    ax.axisVisible = true;
                } else {
                    ax.axisMode = mode;
                    if (mode == "ij") ax.yDir = "reverse";
                    else if (mode == "xy") ax.yDir = "normal";
                }
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    // xdir / ydir — direct setters. MATLAB also accepts
    // set(gca, 'XDir', 'reverse'); we ship the direct form here,
    // and `set` is on the BACKLOG.
    reg("layout", "xdir",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty() && args[0].isChar()) {
                auto &fm = gc.fm;
                std::string v = args[0].toString();
                if (v == "reverse" || v == "normal") {
                    fm.currentAxes().xDir = v;
                    fm.current().modified = true;
                    fm.emitModified();
                }
            }
            outs[0] = Value();
        });
    reg("layout", "ydir",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty() && args[0].isChar()) {
                auto &fm = gc.fm;
                std::string v = args[0].toString();
                if (v == "reverse" || v == "normal") {
                    fm.currentAxes().yDir = v;
                    fm.current().modified = true;
                    fm.emitModified();
                }
            }
            outs[0] = Value();
        });
    // ── Color limits & colormap — graphics.layout ─────────────────────
    reg("layout", "caxis",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty() && args[0].numel() >= 2) {
                auto &fm = gc.fm;
                fm.currentAxes().climJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    reg("layout", "clim",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty() && args[0].numel() >= 2) {
                auto &fm = gc.fm;
                fm.currentAxes().climJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    reg("layout", "colormap",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
            auto &ax = fm.currentAxes();
            if (args[0].isChar()) {
                std::string name = args[0].toString();
                for (auto &c : name) c = std::tolower(c);
                ax.colormapName = name;
                ax.customColormapJson.clear();
            } else {
                // Numeric M×3 matrix → encode as JSON array of triplets
                // and clear the named colormap.
                const auto &M = args[0];
                const std::size_t R = M.dims().rows();
                const std::size_t C = M.dims().cols();
                if (C != 3 || R == 0) {
                    outs[0] = Value();
                    return;
                }
                std::ostringstream os;
                os << "[";
                for (std::size_t r = 0; r < R; ++r) {
                    if (r) os << ",";
                    // Column-major: M(r, c) = data[c * R + r].
                    const double rv = M.doubleData()[0 * R + r];
                    const double gv = M.doubleData()[1 * R + r];
                    const double bv = M.doubleData()[2 * R + r];
                    os << "[" << rv << "," << gv << "," << bv << "]";
                }
                os << "]";
                ax.customColormapJson = os.str();
                if (ax.colormapName.empty()) ax.colormapName = "custom";
            }
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    // colorscale('log' | 'linear') — must be called BEFORE imagesc(M).
    // Sets the axes' colorScale state, which survives prepareForPlot so
    // the next imagesc bakes log10 into its quantization (one-shot —
    // toggling after imagesc does nothing because the data is already
    // quantized).
    reg("layout", "colorscale",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto &fm = gc.fm;
            if (args.empty()) {
                fm.currentAxes().colorScale = "linear";
            } else if (args[0].isChar()) {
                std::string mode = args[0].toString();
                for (auto &c : mode) c = std::tolower(c);
                if (mode == "log" || mode == "linear") {
                    fm.currentAxes().colorScale = mode;
                }
            }
            outs[0] = Value();
        });
    // xscale / yscale('log' | 'linear') — set the axis scale of the current
    // axes. Mirrors `set(gca, 'XScale', 'log')` from MATLAB. Unlike colorscale
    // these do NOT survive prepareForPlot — they're meant to be set AFTER
    // the plot, like in MATLAB. The IDE-side renderer (Heatmap, Interactive-
    // Plot) reads figure.xscale / figure.yscale to drive log-axis display.
    auto regAxisScale = [&](const char *fnName, std::string AxesState::*field) {
        reg("layout", fnName,
            [field, fnName](Span<const Value> args, size_t nargout, Span<Value> outs,
                    GraphicsContext &gc) {
                auto &fm = gc.fm;
                std::string mode = "linear";
                if (!args.empty() && args[0].isChar()) {
                    std::string m = args[0].toString();
                    for (auto &c : m) c = std::tolower(c);
                    if (m == "log" || m == "linear") mode = m;
                }
                auto &ax = fm.currentAxes();
                // yyaxis: yscale routes to yscale2 when right is active.
                if (std::string(fnName) == "yscale"
                    && ax.yyEnabled && ax.activeYside == "right")
                    ax.yscale2 = mode;
                else
                    ax.*field = mode;
                fm.current().modified = true;
                fm.emitModified();
                outs[0] = Value();
            });
    };
    regAxisScale("xscale", &AxesState::xscale);
    regAxisScale("yscale", &AxesState::yscale);
    // yyaxis(side) — switch the active Y-side. MATLAB: yyaxis left|right.
    // First call enables the dual-axis state and stays enabled for the
    // life of the current axes; toggling only changes which side new
    // plots / ylim / ylabel / yscale calls write to.
    reg("layout", "yyaxis",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto &fm = gc.fm;
            auto &ax = fm.currentAxes();
            std::string side = "left";
            if (!args.empty() && args[0].isChar()) {
                std::string s = args[0].toString();
                for (auto &c : s) c = (char)std::tolower((unsigned char)c);
                if (s == "left" || s == "right") side = s;
            }
            ax.yyEnabled = true;
            ax.activeYside = side;
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    auto noop = [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
        outs[0] = Value();
    };
    auto noop_ret1 = [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
        outs[0] = Value::scalar(1.0, gc.mr);
    };
    // linkaxes — connect all subplot cells in the current figure for
    // synchronised pan/zoom. MATLAB:
    //   linkaxes(h)            — link on x and y (alias for 'xy')
    //   linkaxes(h, 'x'|'y'|'xy'|'off')
    // numkit doesn't model graphics handles, so we ignore the first
    // argument and link every subplot cell in the current figure
    // unconditionally. The mode is stored on the FigureState and
    // applied by the SubplotGrid renderer.
    reg("layout", "linkaxes",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto &fm = gc.fm;
            std::string mode = "xy";  // MATLAB default when no mode arg
            for (auto &a : args) {
                if (a.isChar()) {
                    std::string s = a.toString();
                    std::string sl;
                    sl.reserve(s.size());
                    for (char c : s) sl.push_back((char)std::tolower((unsigned char)c));
                    if (sl == "x" || sl == "y" || sl == "xy" || sl == "off") {
                        mode = sl;
                        break;  // only the first string arg is the mode
                    }
                }
            }
            if (mode == "off")
                fm.current().linkMode.clear();
            else
                fm.current().linkMode = mode;
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    // rotate3d / pan3d / zoom3d (on|off) — toggle the OrbitControls
    // axis-of-interaction for the 3-D renderer. MATLAB also accepts
    // these as no-arg toggles but the e2e parity tests pass strings.
    auto interactionToggle = [](std::string AxesState::*field,
                                Span<const Value> args, Span<Value> outs,
                                GraphicsContext &gc) {
        std::string mode = "on";
        if (!args.empty() && args[0].isChar()) {
            std::string s = args[0].toString();
            for (auto &c : s) c = (char)std::tolower((unsigned char)c);
            if (s == "on" || s == "off") mode = s;
        }
        gc.fm.currentAxes().*field = mode;
        gc.fm.current().modified = true;
        gc.fm.emitModified();
        outs[0] = Value();
    };
    reg("layout", "rotate3d",
        [interactionToggle](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            interactionToggle(&AxesState::rotate3dMode, a, o, gc);
        });
    reg("layout", "pan3d",
        [interactionToggle](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            interactionToggle(&AxesState::pan3dMode, a, o, gc);
        });
    reg("layout", "zoom3d",
        [interactionToggle](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            interactionToggle(&AxesState::zoom3dMode, a, o, gc);
        });
    // xticks / yticks / zticks — set custom tick positions. Accept a
    // numeric vector OR the string "auto" to clear. With no args,
    // the v1 returns an empty value (true MATLAB returns the
    // auto-generated tick set; needs renderer-side query plumbing).
    auto ticksReg = [](std::string AxesState::*field) {
        return [field](Span<const Value> args, size_t nargout,
                       Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            auto &fm = gc.fm;
            auto &ax = fm.currentAxes();
            if (args.empty()) {
                outs[0] = Value();
                return;
            }
            if (args[0].isChar()) {
                std::string s = args[0].toString();
                for (auto &c : s) c = (char)std::tolower((unsigned char)c);
                if (s == "auto") {
                    (ax.*field).clear();
                    fm.current().modified = true;
                    fm.emitModified();
                }
                outs[0] = Value();
                return;
            }
            std::ostringstream os;
            os << '[';
            const size_t n = args[0].numel();
            for (size_t i = 0; i < n; ++i) {
                if (i) os << ',';
                os << args[0].elemAsDouble(i);
            }
            os << ']';
            ax.*field = os.str();
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        };
    };
    reg("layout", "xticks", ticksReg(&AxesState::xTicksJson));
    reg("layout", "yticks", ticksReg(&AxesState::yTicksJson));
    reg("layout", "zticks", ticksReg(&AxesState::zTicksJson));
    // xticklabels / yticklabels / zticklabels — set custom tick label
    // strings. Accept a cell of strings, a string array, or a single
    // multiline string. v1: cell-of-chars (most common) supported via
    // per-element toString; "auto" clears.
    auto tickLabelsReg = [](std::string AxesState::*field) {
        return [field](Span<const Value> args, size_t nargout,
                       Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            auto &fm = gc.fm;
            auto &ax = fm.currentAxes();
            if (args.empty()) { outs[0] = Value(); return; }
            if (args[0].isChar()) {
                std::string s = args[0].toString();
                std::string sl;
                for (char c : s) sl.push_back((char)std::tolower((unsigned char)c));
                if (sl == "auto") {
                    (ax.*field).clear();
                    fm.current().modified = true;
                    fm.emitModified();
                }
                outs[0] = Value();
                return;
            }
            // Build "[\"a\",\"b\",...]" from cell/string-array.
            std::ostringstream os;
            os << '[';
            const size_t n = args[0].numel();
            for (size_t i = 0; i < n; ++i) {
                if (i) os << ',';
                Value elem = args[0].elemAt(i, gc.mr);
                std::string s = elem.isChar() ? elem.toString() : "";
                os << '"';
                for (char c : s) {
                    if (c == '"' || c == '\\') os << '\\';
                    os << c;
                }
                os << '"';
            }
            os << ']';
            ax.*field = os.str();
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        };
    };
    reg("layout", "xticklabels", tickLabelsReg(&AxesState::xTickLabelsJson));
    reg("layout", "yticklabels", tickLabelsReg(&AxesState::yTickLabelsJson));
    reg("layout", "zticklabels", tickLabelsReg(&AxesState::zTickLabelsJson));
    // xtickformat / ytickformat / ztickformat — set a sprintf-style
    // format string ("%.2f", "%.0e", etc.). 'auto' clears.
    auto tickFormatReg = [](std::string AxesState::*field) {
        return [field](Span<const Value> args, size_t nargout,
                       Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            auto &fm = gc.fm;
            auto &ax = fm.currentAxes();
            if (args.empty() || !args[0].isChar()) {
                outs[0] = Value();
                return;
            }
            std::string s = args[0].toString();
            std::string sl;
            for (char c : s) sl.push_back((char)std::tolower((unsigned char)c));
            if (sl == "auto") {
                (ax.*field).clear();
            } else {
                ax.*field = s;
            }
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        };
    };
    reg("layout", "xtickformat", tickFormatReg(&AxesState::xTickFormat));
    reg("layout", "ytickformat", tickFormatReg(&AxesState::yTickFormat));
    reg("layout", "ztickformat", tickFormatReg(&AxesState::zTickFormat));
    // xtickangle / ytickangle — rotation of tick labels in degrees.
    auto tickAngleReg = [](double AxesState::*field) {
        return [field](Span<const Value> args, size_t nargout,
                       Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            auto &fm = gc.fm;
            if (args.empty()) { outs[0] = Value(); return; }
            fm.currentAxes().*field = args[0].toScalar();
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        };
    };
    reg("layout", "xtickangle", tickAngleReg(&AxesState::xTickAngle));
    reg("layout", "ytickangle", tickAngleReg(&AxesState::yTickAngle));
    // daspect([dx dy dz]) / pbaspect([px py pz]) — data + plot box
    // aspect ratios. v1 maps the canonical [1 1 1] case to
    // axisMode='equal' (equivalent visual). Other ratios accepted but
    // currently no-op — full anisotropic stretching is BACKLOG.
    auto aspectImpl = [](Span<const Value> args, size_t nargout,
                         Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        auto &fm = gc.fm;
        if (args.empty()) { outs[0] = Value(); return; }
        if (args[0].isChar()) {
            std::string s = args[0].toString();
            for (auto &c : s) c = (char)std::tolower((unsigned char)c);
            if (s == "auto") {
                fm.currentAxes().axisMode.clear();
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
            return;
        }
        if (args[0].numel() >= 3) {
            const double dx = args[0].elemAsDouble(0);
            const double dy = args[0].elemAsDouble(1);
            const double dz = args[0].elemAsDouble(2);
            if (std::abs(dx - dy) < 1e-9 && std::abs(dy - dz) < 1e-9
                && fm.currentAxes().axisMode.empty()) {
                fm.currentAxes().axisMode = "equal";
                fm.current().modified = true;
                fm.emitModified();
            }
        }
        outs[0] = Value();
    };
    reg("layout", "daspect",  aspectImpl);
    reg("layout", "pbaspect", aspectImpl);
    // box(on|off|toggle) — show/hide the axis frame rectangle (the
    // full closed box vs. just left + bottom edges).
    reg("layout", "box",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            auto &fm = gc.fm;
            auto &ax = fm.currentAxes();
            if (args.empty()) {
                ax.boxOn = !ax.boxOn;
            } else if (args[0].isChar()) {
                std::string s = args[0].toString();
                for (auto &c : s) c = (char)std::tolower((unsigned char)c);
                if      (s == "on")  ax.boxOn = true;
                else if (s == "off") ax.boxOn = false;
            }
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    reg("layout", "axes", noop_ret1);
    reg("layout", "gca", noop_ret1);
    reg("layout", "gcf", noop_ret1);
    // cla([reset]) — clear the current axes' datasets. With 'reset'
    // also clears the per-axis config (title, xlabel, etc.). MATLAB's
    // 'reset' is opt-in; default cla preserves axes properties.
    reg("layout", "cla",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            auto &fm = gc.fm;
            auto &ax = fm.currentAxes();
            ax.datasets.clear();
            ax.animatedDatasetIdx = -1;
            // 'reset' arg: also clear axis config + scales.
            if (!args.empty() && args[0].isChar()) {
                std::string s = args[0].toString();
                for (auto &c : s) c = (char)std::tolower((unsigned char)c);
                if (s == "reset") {
                    ax.title.clear(); ax.subtitle.clear();
                    ax.xlabel.clear(); ax.ylabel.clear();
                    ax.xlimJson.clear(); ax.ylimJson.clear();
                    ax.legendLabels.clear(); ax.legendLocation.clear();
                    ax.gridMajor = false; ax.gridMinor = false;
                    ax.gridUserTouched = false;
                    ax.colormapName.clear();
                    ax.axisMode.clear();
                    ax.axisVisible = true;
                    ax.xscale = "linear"; ax.yscale = "linear";
                    ax.xTicksJson.clear(); ax.yTicksJson.clear();
                    ax.xTickLabelsJson.clear(); ax.yTickLabelsJson.clear();
                    ax.xTickFormat.clear(); ax.yTickFormat.clear();
                }
            }
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    // linkprop / linkdata — handle-based property linking. We don't
    // model graphics handles, so these accept any args and return an
    // opaque scalar handle so user scripts that store the return value
    // (h = linkprop(...);) don't break. Real synchronised state is
    // BACKLOG.
    reg("layout", "linkprop", noop_ret1);
    reg("layout", "linkdata", noop_ret1);
    // ────────────────────────────────────────────────────────────────
    // animatedline — incremental line plot. MATLAB:
    //   h = animatedline;          — empty line on the current axes
    //   h = animatedline(x0, y0);  — initial points
    //   addpoints(h, x, y);
    //   clearpoints(h);
    //   [x, y] = getpoints(h);
    //   drawnow;                   — flush
    // numkit doesn't model graphics handles, so animatedline-cluster
    // calls target the most recent animated dataset on the current
    // axes (axes::animatedDatasetIdx). animatedline returns an
    // opaque scalar (1-based index) for script compat, but every
    // op walks animatedDatasetIdx.
    // Wire: the dataset uses ds.type='line' with isAnimated=true and
    // animatedX/animatedY holding the raw points. xJson/yJson are
    // rebuilt on every push.
    // ────────────────────────────────────────────────────────────────
    auto rebuildAnimatedJson = [](DatasetInfo &ds) {
        std::ostringstream xs, ys;
        xs << '[';
        for (size_t i = 0; i < ds.animatedX.size(); ++i) {
            if (i) xs << ',';
            xs << ds.animatedX[i];
        }
        xs << ']';
        ys << '[';
        for (size_t i = 0; i < ds.animatedY.size(); ++i) {
            if (i) ys << ',';
            ys << ds.animatedY[i];
        }
        ys << ']';
        ds.xJson = xs.str();
        ds.yJson = ys.str();
    };
    reg("layout", "animatedline",
        [rebuildAnimatedJson](Span<const Value> args, size_t nargout,
                              Span<Value> outs, GraphicsContext &gc) {
            auto *mr = gc.mr;
            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "line";
            ds.isAnimated = true;
            // Optional initial points.
            if (args.size() >= 2 && !args[0].isChar() && !args[1].isChar()) {
                const auto &xa = args[0];
                const auto &ya = args[1];
                const size_t n = std::min(xa.numel(), ya.numel());
                ds.animatedX.reserve(n);
                ds.animatedY.reserve(n);
                for (size_t i = 0; i < n; ++i) {
                    ds.animatedX.push_back(xa.elemAsDouble(i));
                    ds.animatedY.push_back(ya.elemAsDouble(i));
                }
            }
            rebuildAnimatedJson(ds);
            auto &ax = fm.currentAxes();
            ax.datasets.push_back(std::move(ds));
            ax.animatedDatasetIdx = (int)ax.datasets.size() - 1;
            fm.current().modified = true;
            fm.emitModified();
            // Return 1-based dataset index as a scalar handle.
            outs[0] = Value::scalar((double)(ax.animatedDatasetIdx + 1), mr);
        });
    reg("layout", "addpoints",
        [rebuildAnimatedJson](Span<const Value> args, size_t nargout,
                              Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            auto &fm = gc.fm;
            auto &ax = fm.currentAxes();
            if (ax.animatedDatasetIdx < 0
                || (size_t)ax.animatedDatasetIdx >= ax.datasets.size()) {
                outs[0] = Value();
                return;
            }
            // Skip args[0] (handle) — addpoints(h, x, y).
            if (args.size() < 3) { outs[0] = Value(); return; }
            const auto &xa = args[1];
            const auto &ya = args[2];
            const size_t n = std::min(xa.numel(), ya.numel());
            auto &ds = ax.datasets[(size_t)ax.animatedDatasetIdx];
            ds.animatedX.reserve(ds.animatedX.size() + n);
            ds.animatedY.reserve(ds.animatedY.size() + n);
            for (size_t i = 0; i < n; ++i) {
                ds.animatedX.push_back(xa.elemAsDouble(i));
                ds.animatedY.push_back(ya.elemAsDouble(i));
            }
            rebuildAnimatedJson(ds);
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    reg("layout", "clearpoints",
        [rebuildAnimatedJson](Span<const Value> args, size_t nargout,
                              Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            (void)args;
            auto &fm = gc.fm;
            auto &ax = fm.currentAxes();
            if (ax.animatedDatasetIdx < 0
                || (size_t)ax.animatedDatasetIdx >= ax.datasets.size()) {
                outs[0] = Value();
                return;
            }
            auto &ds = ax.datasets[(size_t)ax.animatedDatasetIdx];
            ds.animatedX.clear();
            ds.animatedY.clear();
            rebuildAnimatedJson(ds);
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    reg("layout", "getpoints",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)args;
            auto *mr = gc.mr;
            auto &fm = gc.fm;
            auto &ax = fm.currentAxes();
            if (ax.animatedDatasetIdx < 0
                || (size_t)ax.animatedDatasetIdx >= ax.datasets.size()) {
                outs[0] = Value();
                if (nargout >= 2) outs[1] = Value();
                return;
            }
            const auto &ds = ax.datasets[(size_t)ax.animatedDatasetIdx];
            const size_t n = ds.animatedX.size();
            auto xv = Value::matrix(1, n, ValueType::DOUBLE, mr);
            auto yv = Value::matrix(1, n, ValueType::DOUBLE, mr);
            if (n > 0) {
                std::memcpy(xv.doubleDataMut(), ds.animatedX.data(),
                            n * sizeof(double));
                std::memcpy(yv.doubleDataMut(), ds.animatedY.data(),
                            n * sizeof(double));
            }
            outs[0] = std::move(xv);
            if (nargout >= 2) outs[1] = std::move(yv);
        });
    // drawnow — flush figure state. Engine emits figure JSON on
    // every modification; drawnow just bumps modified + emits to be
    // safe (idempotent if already up-to-date).
    reg("layout", "drawnow",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)args; (void)nargout;
            auto &fm = gc.fm;
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    // zlabel(text) — 3-D Z-axis label.
    reg("layout", "zlabel",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty()) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
            fm.currentAxes().zlabel = argStr(args[0]);
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    // zlim([z0, z1]) — 3-D Z-axis limits.
    reg("layout", "zlim",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty() || args[0].numel() < 2) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
            fm.currentAxes().zlimJson = vecToJson(args[0]);
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    // view(az, el) — set the 3-D camera azimuth/elevation in degrees.
    // Both args required for the (az, el) form; 2-element vector also
    // accepted (view([az, el])). The renderer reads cfg.view on mount
    // of Composite3DPlot. Stash on AxesState so it survives prepareForPlot.
    reg("layout", "view",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty()) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
            double az = 0, el = 0;
            bool ok = false;
            // view(2) / view(3) — preset top-down / default 3-D views.
            if (args.size() == 1 && args[0].numel() == 1 && !args[0].isChar()) {
                const int n = (int)args[0].toScalar();
                if (n == 2) { az = 0;     el = 90; ok = true; }    // top-down
                else if (n == 3) { az = -37.5; el = 30; ok = true; } // default 3-D
            }
            if (!ok && args.size() >= 2 && args[0].numel() == 1 && args[1].numel() == 1) {
                az = args[0].toScalar();
                el = args[1].toScalar();
                ok = true;
            } else if (!ok && args[0].numel() >= 2) {
                az = args[0].doubleData()[0];
                el = args[0].doubleData()[1];
                ok = true;
            }
            if (!ok) { outs[0] = Value(); return; }
            std::ostringstream os;
            os << "[" << az << "," << el << "]";
            fm.currentAxes().viewJson = os.str();
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    reg("layout", "set", noop);
    reg("layout", "get", noop_ret1);
}

}  // namespace numkit
