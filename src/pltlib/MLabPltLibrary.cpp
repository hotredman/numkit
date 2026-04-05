#include "MLabPltLibrary.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

namespace mlab {

void PltLibrary::install(Engine &engine)
{
    auto &fm = engine.figureManager();

    // ================================================================
    // Helper lambdas
    // ================================================================

    auto vecToJson = [](const MValue &v) -> std::string {
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

    auto argStr = [](const MValue &v) -> std::string { return v.toString(); };

    auto parsePlotArgs = [](Span<const MValue> args, size_t startIdx, DatasetInfo &ds) {
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

    auto parsePlotXYStyle = [&vecToJson, &makeIndexJson](Span<const MValue> args,
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
    // Figure management
    // ================================================================

    engine.registerFunction("figure",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                auto &fm = ctx.engine->figureManager();
                                int id;
                                if (args.empty()) {
                                    id = fm.newFigure();
                                } else {
                                    id = static_cast<int>(args[0].toScalar());
                                    fm.setFigure(id);
                                }
                                // Emit figure so UI shows the panel (MATLAB shows window on figure())
                                fm.current().modified = true;
                                fm.emitModified();
                                {
                                    outs[0] = MValue::scalar(static_cast<double>(id), alloc);
                                    return;
                                }
                            });

    engine.registerFunction("close",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto &fm = ctx.engine->figureManager();
                                if (args.empty()) {
                                    fm.closeCurrentNotify();
                                } else if (args[0].isChar() && args[0].toString() == "all") {
                                    fm.closeAllNotify();
                                } else {
                                    int id = static_cast<int>(args[0].toScalar());
                                    fm.closeFigureNotify(id);
                                }
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("clf",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto &fm = ctx.engine->figureManager();
                                auto &fig = fm.current();
                                fig.axes.clear();
                                fig.axes.push_back(AxesState{});
                                fig.currentAxes = 0;
                                fig.subplotRows = 0;
                                fig.subplotCols = 0;
                                fig.modified = true;
                                fm.emitModified();
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("hold",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto &ax = ctx.engine->figureManager().currentAxes();
                                if (args.empty())
                                    ax.holdOn = !ax.holdOn;
                                else
                                    ax.holdOn = (args[0].toString() == "on");
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("subplot",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                if (args.size() < 3) {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                                auto &fm = ctx.engine->figureManager();
                                int m = static_cast<int>(args[0].toScalar());
                                int n = static_cast<int>(args[1].toScalar());
                                int p = static_cast<int>(args[2].toScalar());
                                fm.setSubplot(m, n, p);
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    // ================================================================
    // Plot types
    // ================================================================

    engine.registerFunction("plot",
                            [parsePlotXYStyle, parsePlotArgs](Span<const MValue> args,
                                                              size_t nargout,
                                                              Span<MValue> outs,
                                                              CallContext &ctx) {
                                if (args.empty()) {
                                    outs[0] = MValue::empty();
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
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("bar",
                            [vecToJson, makeIndexJson](Span<const MValue> args,
                                                       size_t nargout,
                                                       Span<MValue> outs,
                                                       CallContext &ctx) {
                                if (args.empty()) {
                                    outs[0] = MValue::empty();
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
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("scatter",
                            [vecToJson](Span<const MValue> args,
                                        size_t nargout,
                                        Span<MValue> outs,
                                        CallContext &ctx) {
                                if (args.size() < 2) {
                                    outs[0] = MValue::empty();
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
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("hist",
                            [vecToJson](Span<const MValue> args,
                                        size_t nargout,
                                        Span<MValue> outs,
                                        CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.empty()) {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                                auto &data = args[0];
                                int bins = (args.size() >= 2) ? static_cast<int>(args[1].toScalar())
                                                              : 10;
                                double mn = data.doubleData()[0], mx = data.doubleData()[0];
                                for (size_t i = 1; i < data.numel(); ++i) {
                                    mn = std::min(mn, data.doubleData()[i]);
                                    mx = std::max(mx, data.doubleData()[i]);
                                }
                                double bw = (mx - mn) / bins;
                                if (bw == 0)
                                    bw = 1;
                                auto centers = MValue::matrix(1, bins, MType::DOUBLE, alloc);
                                auto counts = MValue::matrix(1, bins, MType::DOUBLE, alloc);
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
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    // ================================================================
    // imagesc — display matrix as scaled color image
    // ================================================================

    engine.registerFunction("imagesc",
                            [vecToJson, doubleToJson](Span<const MValue> args,
                                                      size_t nargout,
                                                      Span<MValue> outs,
                                                      CallContext &ctx) {
                                if (args.empty()) {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                                auto &fm = ctx.engine->figureManager();
                                fm.prepareForPlot();

                                const MValue *C_arg = nullptr;
                                const MValue *x_arg = nullptr;
                                const MValue *y_arg = nullptr;

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
                                    outs[0] = MValue::empty();
                                    return;
                                }

                                size_t rows = C_arg->dims().rows();
                                size_t cols = C_arg->dims().cols();
                                if (rows == 0 || cols == 0) {
                                    outs[0] = MValue::empty();
                                    return;
                                }

                                std::ostringstream zs;
                                zs << "[";
                                for (size_t r = 0; r < rows; ++r) {
                                    if (r)
                                        zs << ",";
                                    zs << "[";
                                    for (size_t c = 0; c < cols; ++c) {
                                        if (c)
                                            zs << ",";
                                        double val;
                                        if (C_arg->isComplex()) {
                                            val = std::abs(C_arg->complexData()[r * cols + c]);
                                        } else {
                                            val = C_arg->doubleData()[r * cols + c];
                                        }
                                        doubleToJson(zs, val);
                                    }
                                    zs << "]";
                                }
                                zs << "]";

                                DatasetInfo ds;
                                ds.type = "imagesc";
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
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("polarplot",
                            [vecToJson, parsePlotArgs](Span<const MValue> args,
                                                       size_t nargout,
                                                       Span<MValue> outs,
                                                       CallContext &ctx) {
                                if (args.size() < 2) {
                                    outs[0] = MValue::empty();
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
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("stem",
                            [parsePlotXYStyle, parsePlotArgs](Span<const MValue> args,
                                                              size_t nargout,
                                                              Span<MValue> outs,
                                                              CallContext &ctx) {
                                if (args.empty()) {
                                    outs[0] = MValue::empty();
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
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("stairs",
                            [parsePlotXYStyle, parsePlotArgs](Span<const MValue> args,
                                                              size_t nargout,
                                                              Span<MValue> outs,
                                                              CallContext &ctx) {
                                if (args.empty()) {
                                    outs[0] = MValue::empty();
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
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    // ================================================================
    // Log-scale plot types
    // ================================================================

    auto registerLogPlot =
        [&](const std::string &name, const std::string &xscale, const std::string &yscale) {
            engine.registerFunction(name,
                                    [parsePlotXYStyle,
                                     parsePlotArgs,
                                     xscale,
                                     yscale](Span<const MValue> args,
                                             size_t nargout,
                                             Span<MValue> outs,
                                             CallContext &ctx) {
                                        if (args.empty()) {
                                            outs[0] = MValue::empty();
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
                                        {
                                            outs[0] = MValue::empty();
                                            return;
                                        }
                                    });
        };

    registerLogPlot("semilogx", "log", "linear");
    registerLogPlot("semilogy", "linear", "log");
    registerLogPlot("loglog", "log", "log");

    // ================================================================
    // Axes labels, limits, legend
    // ================================================================

    engine.registerFunction("title",
                            [argStr](Span<const MValue> args,
                                     size_t nargout,
                                     Span<MValue> outs,
                                     CallContext &ctx) {
                                if (!args.empty()) {
                                    auto &fm = ctx.engine->figureManager();
                                    fm.currentAxes().title = argStr(args[0]);
                                    fm.current().modified = true;
                                    fm.emitModified();
                                }
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("xlabel",
                            [argStr](Span<const MValue> args,
                                     size_t nargout,
                                     Span<MValue> outs,
                                     CallContext &ctx) {
                                if (!args.empty()) {
                                    auto &fm = ctx.engine->figureManager();
                                    fm.currentAxes().xlabel = argStr(args[0]);
                                    fm.current().modified = true;
                                    fm.emitModified();
                                }
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("ylabel",
                            [argStr](Span<const MValue> args,
                                     size_t nargout,
                                     Span<MValue> outs,
                                     CallContext &ctx) {
                                if (!args.empty()) {
                                    auto &fm = ctx.engine->figureManager();
                                    fm.currentAxes().ylabel = argStr(args[0]);
                                    fm.current().modified = true;
                                    fm.emitModified();
                                }
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("xlim",
                            [vecToJson](Span<const MValue> args,
                                        size_t nargout,
                                        Span<MValue> outs,
                                        CallContext &ctx) {
                                if (!args.empty() && args[0].numel() >= 2) {
                                    auto &fm = ctx.engine->figureManager();
                                    fm.currentAxes().xlimJson = vecToJson(args[0]);
                                    fm.current().modified = true;
                                    fm.emitModified();
                                }
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("ylim",
                            [vecToJson](Span<const MValue> args,
                                        size_t nargout,
                                        Span<MValue> outs,
                                        CallContext &ctx) {
                                if (!args.empty() && args[0].numel() >= 2) {
                                    auto &fm = ctx.engine->figureManager();
                                    fm.currentAxes().ylimJson = vecToJson(args[0]);
                                    fm.current().modified = true;
                                    fm.emitModified();
                                }
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("grid",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
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
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("legend",
                            [argStr](Span<const MValue> args,
                                     size_t nargout,
                                     Span<MValue> outs,
                                     CallContext &ctx) {
                                auto &fm = ctx.engine->figureManager();
                                auto &ax = fm.currentAxes();
                                ax.legendLabels.clear();
                                for (auto &a : args)
                                    ax.legendLabels.push_back(argStr(a));
                                fm.current().modified = true;
                                fm.emitModified();
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("axis",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                if (!args.empty() && args[0].isChar()) {
                                    auto &fm = ctx.engine->figureManager();
                                    fm.currentAxes().axisMode = args[0].toString();
                                    fm.current().modified = true;
                                    fm.emitModified();
                                }
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    // ================================================================
    // Polar-specific settings
    // ================================================================

    engine.registerFunction("rlim",
                            [vecToJson](Span<const MValue> args,
                                        size_t nargout,
                                        Span<MValue> outs,
                                        CallContext &ctx) {
                                if (!args.empty() && args[0].numel() >= 2) {
                                    auto &fm = ctx.engine->figureManager();
                                    fm.currentAxes().rlimJson = vecToJson(args[0]);
                                    fm.current().modified = true;
                                    fm.emitModified();
                                }
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("thetalim",
                            [vecToJson](Span<const MValue> args,
                                        size_t nargout,
                                        Span<MValue> outs,
                                        CallContext &ctx) {
                                if (!args.empty() && args[0].numel() >= 2) {
                                    auto &fm = ctx.engine->figureManager();
                                    fm.currentAxes().thetalimJson = vecToJson(args[0]);
                                    fm.current().modified = true;
                                    fm.emitModified();
                                }
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("thetadir",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                if (!args.empty() && args[0].isChar()) {
                                    auto &fm = ctx.engine->figureManager();
                                    fm.currentAxes().thetaDir = args[0].toString();
                                    fm.current().modified = true;
                                    fm.emitModified();
                                }
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("thetazero",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                if (!args.empty() && args[0].isChar()) {
                                    auto &fm = ctx.engine->figureManager();
                                    fm.currentAxes().thetaZeroLocation = args[0].toString();
                                    fm.current().modified = true;
                                    fm.emitModified();
                                }
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    // ================================================================
    // Color limits & colormap
    // ================================================================

    engine.registerFunction("caxis",
                            [vecToJson](Span<const MValue> args,
                                        size_t nargout,
                                        Span<MValue> outs,
                                        CallContext &ctx) {
                                if (!args.empty() && args[0].numel() >= 2) {
                                    auto &fm = ctx.engine->figureManager();
                                    fm.currentAxes().climJson = vecToJson(args[0]);
                                    fm.current().modified = true;
                                    fm.emitModified();
                                }
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("clim",
                            [vecToJson](Span<const MValue> args,
                                        size_t nargout,
                                        Span<MValue> outs,
                                        CallContext &ctx) {
                                if (!args.empty() && args[0].numel() >= 2) {
                                    auto &fm = ctx.engine->figureManager();
                                    fm.currentAxes().climJson = vecToJson(args[0]);
                                    fm.current().modified = true;
                                    fm.emitModified();
                                }
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    engine.registerFunction("colormap",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                if (!args.empty() && args[0].isChar()) {
                                    auto &fm = ctx.engine->figureManager();
                                    std::string name = args[0].toString();
                                    // normalize to lowercase
                                    for (auto &c : name)
                                        c = std::tolower(c);
                                    fm.currentAxes().colormapName = name;
                                    fm.current().modified = true;
                                    fm.emitModified();
                                }
                                {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                            });

    // ================================================================
    // GUI no-ops (not yet implemented)
    // ================================================================

    auto noop = [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
        {
            outs[0] = MValue::empty();
            return;
        }
    };
    auto noop_ret1 =
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            {
                outs[0] = MValue::scalar(1.0, &ctx.engine->allocator());
                return;
            }
        };

    engine.registerFunction("axes", noop_ret1);
    engine.registerFunction("gca", noop_ret1);
    engine.registerFunction("gcf", noop_ret1);
    engine.registerFunction("cla", noop);
    engine.registerFunction("zlabel", noop);
    engine.registerFunction("colorbar", noop);
    engine.registerFunction("zlim", noop);
    engine.registerFunction("view", noop);
    engine.registerFunction("set", noop);
    engine.registerFunction("get", noop_ret1);

    engine.registerFunction("scatter3", noop);
    engine.registerFunction("surf", noop);
    engine.registerFunction("mesh", noop);
    engine.registerFunction("contour", noop);
    engine.registerFunction("contourf", noop);
    engine.registerFunction("pcolor", noop);
    engine.registerFunction("xline", noop);
    engine.registerFunction("yline", noop);
    engine.registerFunction("camlight", noop);
    engine.registerFunction("lighting", noop);
}

} // namespace mlab