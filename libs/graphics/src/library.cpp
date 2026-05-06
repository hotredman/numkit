// libs/graphics/src/library.cpp
//
// Registration hub for the graphics library. Namespace layout —
// sub-namespaces by plot family (NAMESPACE_DESIGN.md §5, §9.5):
//   layout/   — figure / subplot / hold / axes / labels / limits / legend
//   line/     — plot / stem / stairs / semilogx / semilogy / loglog / xline / yline
//   polar/    — polarplot / rlim / thetalim / thetadir / thetazero
//   bar/      — bar / scatter / hist
//   surface/  — surf / mesh / pcolor / scatter3 / camlight / lighting
//   contour/  — contour / contourf
//   image/    — imagesc
// Every function gets a dual registration:
//   1. graphics.<sub>.<name>
//   2. compat.<name>           (so `import compat.*` flattens it)

#include <numkit/graphics/library.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>

namespace numkit {

void GraphicsLibrary::install(Engine &engine)
{
    auto &fm = engine.figureManager();

    // ── Local helper: dual-register graphics.<sub>.<name> + compat.<name> ──
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("graphics.") + sub, name, fn);
        engine.registerFunction("compat", name, fn);
    };

    // ── Triple-register: graphics.<sub>.<name> + compat.<name> + core ──
    //
    // For session/workspace-style commands that conceptually live on
    // par with `clear` / `who` (not data-plotting). These are reachable
    // by short name without any `import` (the third registration into
    // core), in addition to the namespace and compat aliases. Use
    // sparingly — see NAMESPACE_DESIGN.md §7 (promotions).
    auto regCore = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("graphics.") + sub, name, fn);
        engine.registerFunction("compat", name, fn);
        engine.registerFunction("", name, fn);  // core
    };

    // ================================================================
    // Helper lambdas
    // ================================================================

    auto vecToJson = [](const Value &v) -> std::string {
        std::ostringstream os;
        os << "[";
        if (v.isComplex()) {
            for (size_t i = 0; i < v.numel(); ++i) {
                if (i)
                    os << ",";
                os << std::abs(v.complexData()[i]);
            }
        } else {
            for (size_t i = 0; i < v.numel(); ++i) {
                if (i)
                    os << ",";
                double val = v.doubleData()[i];
                if (std::isnan(val))
                    os << "null";
                else if (std::isinf(val))
                    os << (val > 0 ? "1e308" : "-1e308");
                else
                    os << val;
            }
        }
        os << "]";
        return os.str();
    };

    auto makeIndexJson = [](size_t n) -> std::string {
        std::ostringstream xs;
        xs << "[";
        for (size_t i = 0; i < n; ++i) {
            if (i)
                xs << ",";
            xs << (i + 1);
        }
        xs << "]";
        return xs.str();
    };

    auto argStr = [](const Value &v) -> std::string { return v.toString(); };

    auto parsePlotArgs = [](Span<const Value> args, size_t startIdx, DatasetInfo &ds) {
        for (size_t i = startIdx; i + 1 < args.size(); i += 2) {
            if (!args[i].isChar())
                continue;
            std::string key = args[i].toString();
            for (auto &c : key)
                c = std::tolower(c);
            if (key == "linewidth")
                ds.lineWidth = args[i + 1].toScalar();
            else if (key == "markersize")
                ds.markerSize = args[i + 1].toScalar();
        }
    };

    auto parsePlotXYStyle = [&vecToJson, &makeIndexJson](Span<const Value> args,
                                                         DatasetInfo &ds) -> size_t {
        size_t nvStart = 2;
        if (args.size() >= 2 && !args[1].isChar()) {
            ds.xJson = vecToJson(args[0]);
            ds.yJson = vecToJson(args[1]);
            if (args.size() >= 3 && args[2].isChar()) {
                ds.style = args[2].toString();
                nvStart = 3;
            }
        } else {
            ds.xJson = makeIndexJson(args[0].numel());
            ds.yJson = vecToJson(args[0]);
            if (args.size() >= 2 && args[1].isChar()) {
                ds.style = args[1].toString();
                nvStart = 2;
            } else {
                nvStart = 1;
            }
        }
        return nvStart;
    };

    auto doubleToJson = [](std::ostringstream &os, double val) {
        if (std::isnan(val))
            os << "null";
        else if (std::isinf(val))
            os << (val > 0 ? "1e308" : "-1e308");
        else
            os << val;
    };

    // ================================================================
    // Figure management — graphics.layout
    // ================================================================

    regCore("layout", "figure",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto *mr = ctx.engine->resource();
            auto &fm = ctx.engine->figureManager();
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
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto &fm = ctx.engine->figureManager();
            if (args.empty()) {
                fm.closeCurrentNotify();
            } else if (args[0].isChar() && args[0].toString() == "all") {
                fm.closeAllNotify();
            } else {
                int id = static_cast<int>(args[0].toScalar());
                fm.closeFigureNotify(id);
            }
            outs[0] = Value::empty();
        });

    reg("layout", "clf",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto &fm = ctx.engine->figureManager();
            auto &fig = fm.current();
            fig.axes.clear();
            fig.axes.push_back(AxesState{});
            fig.currentAxes = 0;
            fig.subplotRows = 0;
            fig.subplotCols = 0;
            fig.modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });

    regCore("layout", "hold",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto &ax = ctx.engine->figureManager().currentAxes();
            if (args.empty())
                ax.holdOn = !ax.holdOn;
            else
                ax.holdOn = (args[0].toString() == "on");
            outs[0] = Value::empty();
        });

    reg("layout", "subplot",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 3) {
                outs[0] = Value::empty();
                return;
            }
            auto &fm = ctx.engine->figureManager();
            int m = static_cast<int>(args[0].toScalar());
            int n = static_cast<int>(args[1].toScalar());
            int p = static_cast<int>(args[2].toScalar());
            fm.setSubplot(m, n, p);
            outs[0] = Value::empty();
        });

    // ================================================================
    // Plot types — graphics.line / graphics.bar / graphics.image
    // ================================================================

    reg("line", "plot",
        [parsePlotXYStyle, parsePlotArgs](Span<const Value> args, size_t nargout,
                                          Span<Value> outs, CallContext &ctx) {
            if (args.empty()) {
                outs[0] = Value::empty();
                return;
            }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "line";
            size_t nvStart = parsePlotXYStyle(args, ds);
            parsePlotArgs(args, nvStart, ds);
            fm.currentAxes().datasets.push_back(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    reg("bar", "bar",
        [vecToJson, makeIndexJson](Span<const Value> args, size_t nargout,
                                   Span<Value> outs, CallContext &ctx) {
            if (args.empty()) {
                outs[0] = Value::empty();
                return;
            }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "bar";
            if (args.size() >= 2 && !args[1].isChar()) {
                ds.xJson = vecToJson(args[0]);
                ds.yJson = vecToJson(args[1]);
            } else {
                ds.xJson = makeIndexJson(args[0].numel());
                ds.yJson = vecToJson(args[0]);
            }
            fm.currentAxes().datasets.push_back(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    reg("bar", "scatter",
        [vecToJson](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2) {
                outs[0] = Value::empty();
                return;
            }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "scatter";
            ds.xJson = vecToJson(args[0]);
            ds.yJson = vecToJson(args[1]);
            fm.currentAxes().datasets.push_back(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    reg("bar", "hist",
        [vecToJson](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto *mr = ctx.engine->resource();
            if (args.empty()) {
                outs[0] = Value::empty();
                return;
            }
            auto &data = args[0];
            int bins = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 10;
            double mn = data.doubleData()[0], mx = data.doubleData()[0];
            for (size_t i = 1; i < data.numel(); ++i) {
                mn = std::min(mn, data.doubleData()[i]);
                mx = std::max(mx, data.doubleData()[i]);
            }
            double bw = (mx - mn) / bins;
            if (bw == 0)
                bw = 1;
            auto centers = Value::matrix(1, bins, ValueType::DOUBLE, mr);
            auto counts = Value::matrix(1, bins, ValueType::DOUBLE, mr);
            for (int b = 0; b < bins; ++b)
                centers.doubleDataMut()[b] = mn + bw * (b + 0.5);
            for (size_t i = 0; i < data.numel(); ++i) {
                int b = static_cast<int>((data.doubleData()[i] - mn) / bw);
                if (b >= bins)
                    b = bins - 1;
                if (b < 0)
                    b = 0;
                counts.doubleDataMut()[b] += 1;
            }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "bar";
            ds.xJson = vecToJson(centers);
            ds.yJson = vecToJson(counts);
            fm.currentAxes().datasets.push_back(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    reg("image", "imagesc",
        [vecToJson, doubleToJson](Span<const Value> args, size_t nargout,
                                  Span<Value> outs, CallContext &ctx) {
            if (args.empty()) {
                outs[0] = Value::empty();
                return;
            }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();

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
                outs[0] = Value::empty();
                return;
            }

            size_t rows = C_arg->dims().rows();
            size_t cols = C_arg->dims().cols();
            if (rows == 0 || cols == 0) {
                outs[0] = Value::empty();
                return;
            }

            DatasetInfo ds;
            ds.type = "imagesc";

            // Always populate zRaw — full-resolution backing store (column-major
            // float32, MATLAB-style) the IDE will read via getFigureTile() for
            // zoom-in detail on huge matrices.
            ds.zRaw.resize(rows * cols);
            for (size_t c = 0; c < cols; ++c) {
                for (size_t r = 0; r < rows; ++r) {
                    double val;
                    if (C_arg->isComplex()) {
                        val = std::abs(C_arg->complexData()[c * rows + r]);
                    } else {
                        val = C_arg->doubleData()[c * rows + r];
                    }
                    ds.zRaw[c * rows + r] = static_cast<float>(val);
                }
            }

            // Inline-JSON path: ≤2M cells go straight, larger get mean-pooled to
            // ≤2M before serialisation. 1.2 GB JSON for 10000² is what blew up
            // the engine before; capping keeps the inline preview ~3 MB.
            constexpr size_t MAX_INLINE_CELLS = 2'000'000;
            const size_t totalCells = rows * cols;
            std::ostringstream zs;
            zs << "[";

            if (totalCells <= MAX_INLINE_CELLS) {
                // Fast path: emit full data, no downsampling.
                for (size_t r = 0; r < rows; ++r) {
                    if (r)
                        zs << ",";
                    zs << "[";
                    for (size_t c = 0; c < cols; ++c) {
                        if (c)
                            zs << ",";
                        doubleToJson(zs, static_cast<double>(ds.zRaw[c * rows + r]));
                    }
                    zs << "]";
                }
            } else {
                // Mean-pool by integer block factors. Pick smallest dr,dc such
                // that ⌈rows/dr⌉ × ⌈cols/dc⌉ ≤ MAX_INLINE_CELLS. Symmetric step
                // (rows-vs-cols ratio preserved) — start from the larger axis.
                size_t step = 1;
                while ((rows + step - 1) / step * ((cols + step - 1) / step) > MAX_INLINE_CELLS) {
                    ++step;
                }
                const size_t dr = step;
                const size_t dc = step;
                const size_t outRows = (rows + dr - 1) / dr;
                const size_t outCols = (cols + dc - 1) / dc;

                for (size_t orow = 0; orow < outRows; ++orow) {
                    if (orow)
                        zs << ",";
                    zs << "[";
                    const size_t r0 = orow * dr;
                    const size_t r1 = std::min(rows, r0 + dr);
                    for (size_t ocol = 0; ocol < outCols; ++ocol) {
                        if (ocol)
                            zs << ",";
                        const size_t c0 = ocol * dc;
                        const size_t c1 = std::min(cols, c0 + dc);
                        // Mean-pool over the (r0..r1) × (c0..c1) block. NaN
                        // values are skipped — only finite samples contribute.
                        double sum = 0.0;
                        size_t n = 0;
                        for (size_t c = c0; c < c1; ++c) {
                            for (size_t r = r0; r < r1; ++r) {
                                const float v = ds.zRaw[c * rows + r];
                                if (std::isfinite(v)) {
                                    sum += static_cast<double>(v);
                                    ++n;
                                }
                            }
                        }
                        const double mean = (n > 0) ? sum / static_cast<double>(n)
                                                    : std::numeric_limits<double>::quiet_NaN();
                        doubleToJson(zs, mean);
                    }
                    zs << "]";
                }

                ds.downsampled  = true;
                ds.originalRows = rows;
                ds.originalCols = cols;
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

            fm.currentAxes().datasets.push_back(std::move(ds));
            if (fm.currentAxes().axisMode.empty()) {
                fm.currentAxes().axisMode = "ij";
            }
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // ── Polar — graphics.polar ───────────────────────────────────────
    reg("polar", "polarplot",
        [vecToJson, parsePlotArgs](Span<const Value> args, size_t nargout,
                                   Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2) {
                outs[0] = Value::empty();
                return;
            }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            fm.currentAxes().polar = true;
            DatasetInfo ds;
            ds.type = "line";
            ds.xJson = vecToJson(args[0]);
            ds.yJson = vecToJson(args[1]);
            size_t nvStart = 2;
            if (args.size() >= 3 && args[2].isChar()) {
                ds.style = args[2].toString();
                nvStart = 3;
            }
            parsePlotArgs(args, nvStart, ds);
            fm.currentAxes().datasets.push_back(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    reg("line", "stem",
        [parsePlotXYStyle, parsePlotArgs](Span<const Value> args, size_t nargout,
                                          Span<Value> outs, CallContext &ctx) {
            if (args.empty()) {
                outs[0] = Value::empty();
                return;
            }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "stem";
            size_t nvStart = parsePlotXYStyle(args, ds);
            parsePlotArgs(args, nvStart, ds);
            fm.currentAxes().datasets.push_back(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    reg("line", "stairs",
        [parsePlotXYStyle, parsePlotArgs](Span<const Value> args, size_t nargout,
                                          Span<Value> outs, CallContext &ctx) {
            if (args.empty()) {
                outs[0] = Value::empty();
                return;
            }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "stairs";
            size_t nvStart = parsePlotXYStyle(args, ds);
            parsePlotArgs(args, nvStart, ds);
            fm.currentAxes().datasets.push_back(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // ── Log-scale plot types — graphics.line ────────────────────────
    auto registerLogPlot = [&reg, parsePlotXYStyle, parsePlotArgs](
                                const char *name, const std::string &xscale,
                                const std::string &yscale) {
        reg("line", name,
            [parsePlotXYStyle, parsePlotArgs, xscale, yscale](
                Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx) {
                if (args.empty()) {
                    outs[0] = Value::empty();
                    return;
                }
                auto &fm = ctx.engine->figureManager();
                fm.prepareForPlot();
                fm.currentAxes().xscale = xscale;
                fm.currentAxes().yscale = yscale;
                DatasetInfo ds;
                ds.type = "line";
                size_t nvStart = parsePlotXYStyle(args, ds);
                parsePlotArgs(args, nvStart, ds);
                fm.currentAxes().datasets.push_back(std::move(ds));
                fm.emitModified();
                outs[0] = Value::empty();
            });
    };
    registerLogPlot("semilogx", "log", "linear");
    registerLogPlot("semilogy", "linear", "log");
    registerLogPlot("loglog", "log", "log");

    // ================================================================
    // Axes labels, limits, legend — graphics.layout
    // ================================================================

    reg("layout", "title",
        [argStr](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty()) {
                auto &fm = ctx.engine->figureManager();
                fm.currentAxes().title = argStr(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    reg("layout", "xlabel",
        [argStr](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty()) {
                auto &fm = ctx.engine->figureManager();
                fm.currentAxes().xlabel = argStr(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    reg("layout", "ylabel",
        [argStr](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty()) {
                auto &fm = ctx.engine->figureManager();
                fm.currentAxes().ylabel = argStr(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    reg("layout", "xlim",
        [vecToJson](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty() && args[0].numel() >= 2) {
                auto &fm = ctx.engine->figureManager();
                fm.currentAxes().xlimJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    reg("layout", "ylim",
        [vecToJson](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty() && args[0].numel() >= 2) {
                auto &fm = ctx.engine->figureManager();
                fm.currentAxes().ylimJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    reg("layout", "grid",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto &fm = ctx.engine->figureManager();
            auto &ax = fm.currentAxes();
            if (args.empty())
                ax.gridMode = ax.gridMode.empty() ? "on" : "";
            else {
                std::string arg = args[0].toString();
                if (arg == "on")
                    ax.gridMode = "on";
                else if (arg == "off")
                    ax.gridMode = "";
                else if (arg == "minor")
                    ax.gridMode = (ax.gridMode == "minor") ? "on" : "minor";
            }
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });

    reg("layout", "legend",
        [argStr](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto &fm = ctx.engine->figureManager();
            auto &ax = fm.currentAxes();
            ax.legendLabels.clear();
            for (auto &a : args)
                ax.legendLabels.push_back(argStr(a));
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });

    reg("layout", "axis",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty() && args[0].isChar()) {
                auto &fm = ctx.engine->figureManager();
                fm.currentAxes().axisMode = args[0].toString();
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    // ── Polar-specific settings — graphics.polar ─────────────────────
    reg("polar", "rlim",
        [vecToJson](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty() && args[0].numel() >= 2) {
                auto &fm = ctx.engine->figureManager();
                fm.currentAxes().rlimJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    reg("polar", "thetalim",
        [vecToJson](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty() && args[0].numel() >= 2) {
                auto &fm = ctx.engine->figureManager();
                fm.currentAxes().thetalimJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    reg("polar", "thetadir",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty() && args[0].isChar()) {
                auto &fm = ctx.engine->figureManager();
                fm.currentAxes().thetaDir = args[0].toString();
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    reg("polar", "thetazero",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty() && args[0].isChar()) {
                auto &fm = ctx.engine->figureManager();
                fm.currentAxes().thetaZeroLocation = args[0].toString();
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    // ── Color limits & colormap — graphics.layout ─────────────────────
    reg("layout", "caxis",
        [vecToJson](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty() && args[0].numel() >= 2) {
                auto &fm = ctx.engine->figureManager();
                fm.currentAxes().climJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    reg("layout", "clim",
        [vecToJson](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty() && args[0].numel() >= 2) {
                auto &fm = ctx.engine->figureManager();
                fm.currentAxes().climJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    reg("layout", "colormap",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty() && args[0].isChar()) {
                auto &fm = ctx.engine->figureManager();
                std::string name = args[0].toString();
                for (auto &c : name)
                    c = std::tolower(c);
                fm.currentAxes().colormapName = name;
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    // ================================================================
    // GUI no-ops (not yet implemented)
    // ================================================================

    auto noop = [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
        outs[0] = Value::empty();
    };
    auto noop_ret1 = [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
        outs[0] = Value::scalar(1.0, ctx.engine->resource());
    };

    reg("layout", "axes", noop_ret1);
    reg("layout", "gca", noop_ret1);
    reg("layout", "gcf", noop_ret1);
    reg("layout", "cla", noop);
    reg("layout", "zlabel", noop);
    reg("layout", "colorbar", noop);
    reg("layout", "zlim", noop);
    reg("layout", "view", noop);
    reg("layout", "set", noop);
    reg("layout", "get", noop_ret1);

    reg("surface", "scatter3", noop);
    reg("surface", "surf", noop);
    reg("surface", "mesh", noop);
    reg("contour", "contour", noop);
    reg("contour", "contourf", noop);
    reg("surface", "pcolor", noop);
    reg("line", "xline", noop);
    reg("line", "yline", noop);
    reg("surface", "camlight", noop);
    reg("surface", "lighting", noop);
}

} // namespace numkit
