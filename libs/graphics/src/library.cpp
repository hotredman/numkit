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
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
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

    // plot3(x, y, z) / scatter3(x, y, z) — 3-D series. The renderer
    // does a cabinet-projection to 2-D (z extends into upper-right at
    // 30°, scaled 0.5). Real 3-D camera is B3 territory; this gets
    // the data on screen so users can write the script and inspect
    // values today. Both reuse the line / scatter render modes after
    // adapter projection.
    auto plot3Impl = [vecToJson, parsePlotArgs](
                         const char *typeName,
                         Span<const Value> args, size_t nargout,
                         Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        if (args.size() < 3) {
            outs[0] = Value::empty();
            return;
        }
        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();
        DatasetInfo ds;
        ds.type = typeName;
        ds.xJson = vecToJson(args[0]);
        ds.yJson = vecToJson(args[1]);
        ds.zJson = vecToJson(args[2]);   // 1-D vector here, distinct from
                                          // imagesc's 2-D zJson — adapter
                                          // disambiguates by `type`.
        size_t nvStart = 3;
        if (args.size() >= 4 && args[3].isChar()) {
            ds.style = args[3].toString();
            nvStart = 4;
        }
        parsePlotArgs(args, nvStart, ds);
        fm.currentAxes().datasets.push_back(std::move(ds));
        fm.emitModified();
        outs[0] = Value::empty();
    };
    {
        using namespace std::placeholders;
        reg("line", "plot3",    std::bind(plot3Impl, "plot3",    _1, _2, _3, _4));
        reg("line", "scatter3", std::bind(plot3Impl, "scatter3", _1, _2, _3, _4));
    }

    // quiver(x, y, u, v) — vector field as N arrows starting at
    // (x[i], y[i]) and pointing in direction (u[i], v[i]). MATLAB
    // accepts matrices; for first cut we expect flat vectors with
    // matching length. quiver(x, y, u, v, scale) optionally scales
    // arrow lengths (default 1, packed into ds.style as "scale=N").
    reg("line", "quiver",
        [vecToJson](Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx) {
            if (args.size() < 4) {
                outs[0] = Value::empty();
                return;
            }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "quiver";
            ds.xJson = vecToJson(args[0]);
            ds.yJson = vecToJson(args[1]);
            ds.uJson = vecToJson(args[2]);
            ds.vJson = vecToJson(args[3]);
            if (args.size() >= 5 && args[4].numel() == 1 && !args[4].isChar()) {
                std::ostringstream os;
                os << "scale=" << args[4].toScalar();
                ds.style = os.str();
            }
            fm.currentAxes().datasets.push_back(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // area — filled curve. MATLAB convention:
    //   area(y)            — x = 1:N, fill from y to baseline 0
    //   area(x, y)         — fill from y to baseline 0
    //   area(x, y, base)   — fill from y to `base`
    // Stored as `type = "area"`; the optional baseline lives in
    // ds.lineWidth's slot is wrong, use a dedicated field. Reusing
    // `markerSize` is also wrong. We pack base into ds.style as
    // "base=<value>" so we don't grow the schema for one optional.
    reg("line", "area",
        [vecToJson, makeIndexJson, parsePlotArgs](Span<const Value> args, size_t nargout,
                                   Span<Value> outs, CallContext &ctx) {
            if (args.empty()) {
                outs[0] = Value::empty();
                return;
            }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "area";

            // Find optional baseline (last numeric scalar arg) and skip
            // line specs (char args).
            size_t nData = args.size();
            for (size_t i = 0; i < args.size(); ++i) {
                if (args[i].isChar()) { nData = i; break; }
            }
            double baseline = 0.0;
            bool hasBase = false;

            if (nData == 1) {
                ds.xJson = makeIndexJson(args[0].numel());
                ds.yJson = vecToJson(args[0]);
            } else if (nData >= 2) {
                ds.xJson = vecToJson(args[0]);
                ds.yJson = vecToJson(args[1]);
                if (nData >= 3 && args[2].numel() == 1) {
                    baseline = args[2].toScalar();
                    hasBase = true;
                }
            }
            // Handle optional N-V pairs (FaceColor, LineWidth, …) after
            // the data args. parsePlotArgs is forgiving about unknown
            // names; baseline gets piggybacked on ds.style as a
            // "base=N" suffix that the adapter parses out.
            parsePlotArgs(args, nData, ds);
            if (hasBase) {
                std::ostringstream os;
                if (!ds.style.empty()) os << ds.style << ";";
                os << "base=" << baseline;
                ds.style = os.str();
            }
            fm.currentAxes().datasets.push_back(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // barh — horizontal bar chart, mirror of bar(). MATLAB convention:
    //   barh(y)    — y is vector of bar lengths, vertical positions = 1:N
    //   barh(x, y) — x is vertical positions (categories), y = lengths
    // We store xJson = positions (rendered along the Y axis) and
    // yJson = lengths (along X axis); the 'barh' mode in the renderer
    // swaps the axis roles compared to 'bar'.
    reg("bar", "barh",
        [vecToJson, makeIndexJson](Span<const Value> args, size_t nargout,
                                   Span<Value> outs, CallContext &ctx) {
            if (args.empty()) {
                outs[0] = Value::empty();
                return;
            }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "barh";
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

    // Shared body for any heatmap-like builtin (imagesc / pcolor).
    // The data-emission path is identical; only the `type` field
    // differs (renderer uses it to pick cell-centre vs cell-vertex
    // alignment). Body kept as a captured lambda + std::bind to avoid
    // duplicating ~200 lines of quantization logic.
    auto heatmapImpl = [vecToJson, doubleToJson](
                           const char *typeName,
                           Span<const Value> args, size_t nargout,
                           Span<Value> outs, CallContext &ctx) {
            (void)nargout;
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

            fm.currentAxes().datasets.push_back(std::move(ds));
            if (fm.currentAxes().axisMode.empty()) {
                fm.currentAxes().axisMode = "ij";
            }
            fm.emitModified();
            outs[0] = Value::empty();
    };  // heatmapImpl

    using namespace std::placeholders;
    reg("image", "imagesc", std::bind(heatmapImpl, "imagesc", _1, _2, _3, _4));
    reg("image", "pcolor",  std::bind(heatmapImpl, "pcolor",  _1, _2, _3, _4));

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

    // errorbar(y, e)                — x = 1:N, symmetric e
    // errorbar(x, y, e)             — symmetric e
    // errorbar(x, y, neg, pos)      — asymmetric error bounds
    // errorbar(x, y, e, 'spec')     — symmetric, with line spec
    // errorbar(x, y, neg, pos, 'spec')
    //
    // The dataset's xJson/yJson hold the centre points; eJson (sym) or
    // eNegJson + ePosJson (asym) hold the magnitudes. The renderer
    // draws vertical bars from y-eNeg to y+ePos with caps at each end.
    reg("line", "errorbar",
        [vecToJson, makeIndexJson, parsePlotArgs](Span<const Value> args, size_t nargout,
                                                  Span<Value> outs, CallContext &ctx) {
            if (args.empty()) {
                outs[0] = Value::empty();
                return;
            }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "errorbar";

            // Find the first char arg (line spec) — everything before is data.
            size_t nData = args.size();
            for (size_t i = 0; i < args.size(); ++i) {
                if (args[i].isChar()) { nData = i; break; }
            }
            // Optional N/V pairs after the (optional) spec — same parser as
            // plot() handles 'LineWidth', 'MarkerSize', 'DisplayName'.
            size_t nvStart = nData;
            if (nData < args.size() && args[nData].isChar()) {
                ds.style = args[nData].toString();
                nvStart = nData + 1;
            }

            switch (nData) {
                case 1:  // errorbar(y) — degenerate but legal-ish; no error
                    ds.xJson = makeIndexJson(args[0].numel());
                    ds.yJson = vecToJson(args[0]);
                    break;
                case 2:  // errorbar(y, e) — x = 1:N, symmetric e
                    ds.xJson = makeIndexJson(args[0].numel());
                    ds.yJson = vecToJson(args[0]);
                    ds.eJson = vecToJson(args[1]);
                    break;
                case 3:  // errorbar(x, y, e) — symmetric e
                    ds.xJson = vecToJson(args[0]);
                    ds.yJson = vecToJson(args[1]);
                    ds.eJson = vecToJson(args[2]);
                    break;
                case 4:  // errorbar(x, y, neg, pos) — asymmetric
                default:
                    ds.xJson    = vecToJson(args[0]);
                    ds.yJson    = vecToJson(args[1]);
                    ds.eNegJson = vecToJson(args[2]);
                    ds.ePosJson = vecToJson(args[3]);
                    break;
            }

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

    // text(x, y, str, ...) — annotation overlay. Each call appends ONE
    // text dataset (single-point) to the current axes. For arrays, the
    // user iterates via for-loop in script. Trailing name-value pairs
    // (Color, FontSize) parsed minimally — extra pairs are ignored.
    //
    // The IDE renders text overlays after the image / line layers so
    // labels stay on top of imagesc / scatter.
    reg("layout", "text",
        [argStr](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 3) { outs[0] = Value::empty(); return; }
            const Value &xv = args[0];
            const Value &yv = args[1];
            if (!xv.numel() || !yv.numel()) { outs[0] = Value::empty(); return; }
            auto &fm = ctx.engine->figureManager();
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
            fm.currentAxes().datasets.push_back(std::move(ds));
            fm.current().modified = true;
            fm.emitModified();
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
        [argStr, normLocation](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto &fm = ctx.engine->figureManager();
            auto &ax = fm.currentAxes();
            // Walk args. Plain strings become legend labels; a `'Location'`
            // / 'location' marker captures the next arg as the location.
            // Other (Name, Value) pairs (FontSize, NumColumns, ...) are
            // not yet honoured but parsed-and-skipped so they don't
            // pollute legendLabels.
            ax.legendLabels.clear();
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
                    outs[0] = Value::empty();
                    return;
                }
                if (i == 0 && (sLower == "show" || sLower == "on")) {
                    // No labels passed — keep whatever was set previously.
                    continue;
                }
                ax.legendLabels.push_back(s);
            }
            if (sawLocation)
                ax.legendLocation = newLoc;
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });

    reg("layout", "colorbar",
        [normLocation](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto &fm = ctx.engine->figureManager();
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
                    outs[0] = Value::empty();
                    return;
                }
            }
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });

    reg("layout", "axis",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty() && args[0].isChar()) {
                auto &fm = ctx.engine->figureManager();
                std::string mode = args[0].toString();
                fm.currentAxes().axisMode = mode;
                // axis('ij') and axis('xy') are MATLAB shorthand for
                // yDir reverse / normal. Set the corresponding state
                // explicitly so the renderer doesn't need to know
                // about axisMode aliases.
                if (mode == "ij") fm.currentAxes().yDir = "reverse";
                else if (mode == "xy") fm.currentAxes().yDir = "normal";
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    // xdir / ydir — direct setters. MATLAB also accepts
    // set(gca, 'XDir', 'reverse'); we ship the direct form here,
    // and `set` is on the BACKLOG.
    reg("layout", "xdir",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty() && args[0].isChar()) {
                auto &fm = ctx.engine->figureManager();
                std::string v = args[0].toString();
                if (v == "reverse" || v == "normal") {
                    fm.currentAxes().xDir = v;
                    fm.current().modified = true;
                    fm.emitModified();
                }
            }
            outs[0] = Value::empty();
        });
    reg("layout", "ydir",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty() && args[0].isChar()) {
                auto &fm = ctx.engine->figureManager();
                std::string v = args[0].toString();
                if (v == "reverse" || v == "normal") {
                    fm.currentAxes().yDir = v;
                    fm.current().modified = true;
                    fm.emitModified();
                }
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

    // colorscale('log' | 'linear') — must be called BEFORE imagesc(M).
    // Sets the axes' colorScale state, which survives prepareForPlot so
    // the next imagesc bakes log10 into its quantization (one-shot —
    // toggling after imagesc does nothing because the data is already
    // quantized).
    reg("layout", "colorscale",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto &fm = ctx.engine->figureManager();
            if (args.empty()) {
                fm.currentAxes().colorScale = "linear";
            } else if (args[0].isChar()) {
                std::string mode = args[0].toString();
                for (auto &c : mode) c = std::tolower(c);
                if (mode == "log" || mode == "linear") {
                    fm.currentAxes().colorScale = mode;
                }
            }
            outs[0] = Value::empty();
        });

    // xscale / yscale('log' | 'linear') — set the axis scale of the current
    // axes. Mirrors `set(gca, 'XScale', 'log')` from MATLAB. Unlike colorscale
    // these do NOT survive prepareForPlot — they're meant to be set AFTER
    // the plot, like in MATLAB. The IDE-side renderer (Heatmap, Interactive-
    // Plot) reads figure.xscale / figure.yscale to drive log-axis display.
    auto regAxisScale = [&](const char *fnName, std::string AxesState::*field) {
        reg("layout", fnName,
            [field](Span<const Value> args, size_t nargout, Span<Value> outs,
                    CallContext &ctx) {
                auto &fm = ctx.engine->figureManager();
                std::string mode = "linear";
                if (!args.empty() && args[0].isChar()) {
                    std::string m = args[0].toString();
                    for (auto &c : m) c = std::tolower(c);
                    if (m == "log" || m == "linear") mode = m;
                }
                fm.currentAxes().*field = mode;
                fm.current().modified = true;
                fm.emitModified();
                outs[0] = Value::empty();
            });
    };
    regAxisScale("xscale", &AxesState::xscale);
    regAxisScale("yscale", &AxesState::yscale);

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
    reg("layout", "zlim", noop);
    reg("layout", "view", noop);
    reg("layout", "set", noop);
    reg("layout", "get", noop_ret1);

    // scatter3 — real impl registered earlier via plot3Impl shared body.
    reg("surface", "surf", noop);
    reg("surface", "mesh", noop);
    reg("contour", "contour", noop);
    reg("contour", "contourf", noop);
    // pcolor — real implementation registered earlier in install()
    // (graphics.image.pcolor + compat.pcolor). The duplicate noop
    // here used to register `compat.pcolor` a second time, which the
    // engine's registerFunction rejects, throwing during install and
    // dropping the renderer into fallback mode.
    reg("line", "xline", noop);
    reg("line", "yline", noop);
    reg("surface", "camlight", noop);
    reg("surface", "lighting", noop);
}

} // namespace numkit
