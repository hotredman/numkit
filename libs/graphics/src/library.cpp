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
#include <numkit/image/io/io.hpp>

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

    // tiledlayout(m, n[, ...]) — modern alternative to subplot. We
    // store the grid shape on the FigureState so subsequent nexttile
    // calls can step through cells. Trailing N-V pairs ('Padding',
    // 'TileSpacing', 'TileIndexing') are accepted but currently no-op
    // (the IDE always renders subplot cells with the same fixed gap).
    reg("layout", "tiledlayout",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            int m = 1, n = 1;
            if (args.size() >= 1 && !args[0].isChar()) m = (int)args[0].toScalar();
            if (args.size() >= 2 && !args[1].isChar()) n = (int)args[1].toScalar();
            if (m < 1) m = 1;
            if (n < 1) n = 1;
            auto &fm = ctx.engine->figureManager();
            // setSubplot(m, n, 1) reserves the grid + activates cell 1.
            fm.setSubplot(m, n, 1);
            outs[0] = Value::empty();
        });
    // nexttile([span]) — bumps the active subplot cell index by 1
    // (or by `span` if given a numeric arg). When the figure has no
    // tiledlayout grid yet, the call is a no-op.
    reg("layout", "nexttile",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            auto &fm = ctx.engine->figureManager();
            auto &fig = fm.current();
            if (fig.subplotRows <= 0 || fig.subplotCols <= 0) {
                // No tiledlayout active; default to a 1x1 grid so
                // the first nexttile creates a single cell.
                fm.setSubplot(1, 1, 1);
                outs[0] = Value::empty();
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
            fm.pushDataset(std::move(ds));
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

            // Identify the X (optional) and Y arguments.
            const Value *xArg = nullptr;
            const Value *yArg = nullptr;
            size_t nvStart = 1;
            if (args.size() >= 2 && !args[1].isChar()) {
                xArg = &args[0];
                yArg = &args[1];
                nvStart = 2;
            } else {
                yArg = &args[0];
            }
            // 'stacked' / 'grouped' / 'histc' specifier (string) after Y.
            std::string mode = "grouped";
            for (size_t i = nvStart; i < args.size(); ++i) {
                if (args[i].isChar()) {
                    std::string s = args[i].toString();
                    for (auto &c : s) c = (char)std::tolower((unsigned char)c);
                    if (s == "stacked" || s == "grouped"
                        || s == "histc"  || s == "hist") {
                        mode = s;
                    }
                }
            }

            const size_t Yr = yArg->dims().rows();
            const size_t Yc = yArg->dims().cols();
            const bool matrix = (Yr > 1 && Yc > 1);
            // Vector path (back-compat): single dataset.
            if (!matrix) {
                DatasetInfo ds;
                ds.type = "bar";
                if (xArg) {
                    ds.xJson = vecToJson(*xArg);
                    ds.yJson = vecToJson(*yArg);
                } else {
                    ds.xJson = makeIndexJson(yArg->numel());
                    ds.yJson = vecToJson(*yArg);
                }
                fm.pushDataset(std::move(ds));
                fm.emitModified();
                outs[0] = Value::empty();
                return;
            }
            // Matrix path: one dataset per column. Stacked uses
            // cumulative sums (same trick as area-stacked); grouped
            // uses raw values + a fan-out X offset per series. Default
            // is 'grouped'.
            static const char *kBarPalette[8] = {
                "#7fd99a", "#5fb3d4", "#e9b870", "#9b8cf2",
                "#e26a6a", "#d4a5e6", "#f2a37e", "#6fcfbf",
            };
            // Build base X vector — explicit when given, else 1..Yr.
            std::vector<double> xb(Yr);
            if (xArg) {
                for (size_t r = 0; r < Yr; ++r) xb[r] = xArg->doubleData()[r];
            } else {
                for (size_t r = 0; r < Yr; ++r) xb[r] = (double)(r + 1);
            }
            const double *Yp = yArg->doubleData();
            if (mode == "stacked") {
                // Cumulative sums; emit column Yc-1 first so lower
                // bands overdraw the bottom of higher ones (same idea
                // as area-stacked).
                std::vector<std::vector<double>> S(Yr, std::vector<double>(Yc, 0.0));
                for (size_t r = 0; r < Yr; ++r) {
                    double acc = 0;
                    for (size_t c = 0; c < Yc; ++c) {
                        acc += Yp[c * Yr + r];
                        S[r][c] = acc;
                    }
                }
                std::ostringstream xs;
                xs << '[';
                for (size_t r = 0; r < Yr; ++r) {
                    if (r) xs << ',';
                    xs << xb[r];
                }
                xs << ']';
                const std::string xJsonStr = xs.str();
                for (long long cc = (long long)Yc - 1; cc >= 0; --cc) {
                    DatasetInfo ds;
                    ds.type = "bar";
                    ds.xJson = xJsonStr;
                    std::ostringstream ys;
                    ys << '[';
                    for (size_t r = 0; r < Yr; ++r) {
                        if (r) ys << ',';
                        ys << S[r][(size_t)cc];
                    }
                    ys << ']';
                    ds.yJson = ys.str();
                    std::ostringstream sty;
                    sty << "color=" << kBarPalette[(size_t)cc % 8];
                    ds.style = sty.str();
                    fm.pushDataset(std::move(ds));
                }
            } else {
                // Grouped: each column gets a small X offset so bars
                // don't overlap. The IDE renderer renders each dataset
                // with the same default width, so sub-pixel
                // positioning is the visible separator. With Yc bars
                // per group, offsets span ±0.4 around the base.
                const double groupHalf = 0.4;
                const double slot = (Yc > 1) ? (2 * groupHalf) / (Yc - 1) : 0.0;
                for (size_t cc = 0; cc < Yc; ++cc) {
                    DatasetInfo ds;
                    ds.type = "bar";
                    std::ostringstream xs, ys;
                    xs << '['; ys << '[';
                    const double off = -groupHalf + slot * cc;
                    for (size_t r = 0; r < Yr; ++r) {
                        if (r) { xs << ','; ys << ','; }
                        xs << (xb[r] + off);
                        ys << Yp[cc * Yr + r];
                    }
                    xs << ']'; ys << ']';
                    ds.xJson = xs.str();
                    ds.yJson = ys.str();
                    std::ostringstream sty;
                    sty << "color=" << kBarPalette[cc % 8];
                    ds.style = sty.str();
                    fm.pushDataset(std::move(ds));
                }
            }
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
        fm.pushDataset(std::move(ds));
        fm.emitModified();
        outs[0] = Value::empty();
    };
    {
        using namespace std::placeholders;
        reg("line", "plot3",    std::bind(plot3Impl, "plot3",    _1, _2, _3, _4));
        reg("line", "scatter3", std::bind(plot3Impl, "scatter3", _1, _2, _3, _4));
    }

    // stem3(x, y, z) — vertical 3D stems from the z=0 plane up to
    // each (x, y, z) point + marker at the tip. Shares the cabinet
    // projection with plot3/scatter3. Emits two datasets:
    //   1. type='plot3' line with NaN-separated 2-point segments
    //      (one segment per stem, going (x,y,0) → (x,y,z))
    //   2. type='scatter3' markers at the tips.
    reg("line", "stem3",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 3) { outs[0] = Value::empty(); return; }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            const auto &X = args[0];
            const auto &Y = args[1];
            const auto &Z = args[2];
            const size_t N = std::min({X.numel(), Y.numel(), Z.numel()});
            if (N == 0) { outs[0] = Value::empty(); return; }

            // Stems: 2 points per stem with null between stems.
            std::ostringstream sx, sy, sz;
            sx << '['; sy << '['; sz << '[';
            for (size_t i = 0; i < N; ++i) {
                const double x = X.doubleData()[i];
                const double y = Y.doubleData()[i];
                const double z = Z.doubleData()[i];
                if (i > 0) { sx << ",null,"; sy << ",null,"; sz << ",null,"; }
                sx << x << ',' << x;
                sy << y << ',' << y;
                sz << "0," << z;
            }
            sx << ']'; sy << ']'; sz << ']';
            DatasetInfo dsLine;
            dsLine.type = "plot3";
            dsLine.xJson = sx.str();
            dsLine.yJson = sy.str();
            dsLine.zJson = sz.str();
            dsLine.style = "color=#1f77b4";
            fm.pushDataset(std::move(dsLine));

            // Tip markers via scatter3.
            std::ostringstream mx, my, mz;
            mx << '['; my << '['; mz << '[';
            for (size_t i = 0; i < N; ++i) {
                if (i) { mx << ','; my << ','; mz << ','; }
                mx << X.doubleData()[i];
                my << Y.doubleData()[i];
                mz << Z.doubleData()[i];
            }
            mx << ']'; my << ']'; mz << ']';
            DatasetInfo dsDot;
            dsDot.type = "scatter3";
            dsDot.xJson = mx.str();
            dsDot.yJson = my.str();
            dsDot.zJson = mz.str();
            dsDot.style = "color=#1f77b4";
            dsDot.markerSize = 4;
            fm.pushDataset(std::move(dsDot));
            fm.emitModified();
            outs[0] = Value::empty();
        });

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
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // compass(U, V) — vector arrows from origin. Equivalent to
    // quiver(zeros, zeros, U, V) plus scale=1 (no auto-scaling so the
    // arrow tips land exactly on (U_i, V_i)). Single-arg form
    // compass(Z) takes complex Z and unpacks real/imag parts.
    reg("line", "compass",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (args.empty()) { outs[0] = Value::empty(); return; }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();

            // Build U / V from args. Two forms:
            //   compass(Z)       — Z complex, U=real(Z), V=imag(Z)
            //   compass(U, V)    — explicit pair
            std::ostringstream us, vs;
            us << '['; vs << '[';
            const auto emit = [&](double u, double v, bool first) {
                if (!first) { us << ','; vs << ','; }
                us << u; vs << v;
            };
            const Value &A = args[0];
            const size_t N = A.numel();
            if (args.size() == 1 && A.isComplex()) {
                for (size_t i = 0; i < N; ++i) {
                    const auto z = A.complexData()[i];
                    emit(z.real(), z.imag(), i == 0);
                }
            } else if (args.size() >= 2 && args[1].numel() >= N) {
                for (size_t i = 0; i < N; ++i)
                    emit(A.doubleData()[i], args[1].doubleData()[i], i == 0);
            } else {
                outs[0] = Value::empty();
                return;
            }
            us << ']'; vs << ']';

            std::ostringstream zs;
            zs << '[';
            for (size_t i = 0; i < N; ++i) { if (i) zs << ','; zs << '0'; }
            zs << ']';

            DatasetInfo ds;
            ds.type = "quiver";
            ds.xJson = zs.str();   // origin x = 0 for every arrow
            ds.yJson = zs.str();   // origin y = 0 for every arrow
            ds.uJson = us.str();
            ds.vJson = vs.str();
            ds.style = "scale=1";  // tips land exactly at (U, V)
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // feather(U, V) — arrows on the x-axis. Each arrow starts at
    // (i, 0) and points to (i + U_i, V_i). Same scale=1 contract as
    // compass.
    reg("line", "feather",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2) { outs[0] = Value::empty(); return; }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            const auto &U = args[0];
            const auto &V = args[1];
            const size_t N = std::min(U.numel(), V.numel());
            if (N == 0) { outs[0] = Value::empty(); return; }
            std::ostringstream xs, ys, us, vs;
            xs << '['; ys << '['; us << '['; vs << '[';
            for (size_t i = 0; i < N; ++i) {
                if (i) { xs << ','; ys << ','; us << ','; vs << ','; }
                xs << (i + 1);                      // origin: (1..N, 0)
                ys << '0';
                us << U.doubleData()[i];
                vs << V.doubleData()[i];
            }
            xs << ']'; ys << ']'; us << ']'; vs << ']';
            DatasetInfo ds;
            ds.type = "quiver";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.uJson = us.str();
            ds.vJson = vs.str();
            ds.style = "scale=1";
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // ────────── Statistical chart wrappers ───────────────────────────
    // These all reduce to 1-2 existing dataset types (scatter / line /
    // bar / stairs) so the renderer doesn't need any new code.

    // cdfplot(x) / ecdf(x) — empirical cumulative distribution. Sorts
    // x ascending and renders a right-continuous step function from
    // 0 to 1. NaN inputs are dropped.
    auto cdfImpl = [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        if (args.empty() || args[0].numel() == 0) { outs[0] = Value::empty(); return; }
        const auto &X = args[0];
        std::vector<double> xs;
        xs.reserve(X.numel());
        for (size_t i = 0; i < X.numel(); ++i) {
            const double v = X.doubleData()[i];
            if (std::isfinite(v)) xs.push_back(v);
        }
        if (xs.empty()) { outs[0] = Value::empty(); return; }
        std::sort(xs.begin(), xs.end());
        const size_t N = xs.size();

        std::ostringstream sx, sy;
        sx << '['; sy << '[';
        for (size_t i = 0; i < N; ++i) {
            if (i) { sx << ','; sy << ','; }
            sx << xs[i];
            sy << ((double)(i + 1) / (double)N);
        }
        sx << ']'; sy << ']';

        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();
        DatasetInfo ds;
        ds.type = "stairs";
        ds.xJson = sx.str();
        ds.yJson = sy.str();
        ds.style = "color=#1f77b4";
        fm.pushDataset(std::move(ds));
        fm.emitModified();
        outs[0] = Value::empty();
    };
    reg("bar", "cdfplot", cdfImpl);
    // `ecdf` is already provided by libs/stats as a computational
    // routine that returns (F, x) — registering a graphics version
    // here would duplicate the compat.<ecdf> alias and brick WASM
    // init. Users plot the empirical CDF via `cdfplot(x)`.

    // qqplot(x) — quantile-quantile plot vs the standard normal. The
    // observed sample quantile at rank i = (i - 0.5) / N is plotted
    // against the theoretical normal quantile Φ⁻¹((i - 0.5) / N).
    // Reference line passes through the 25th and 75th sample quantiles
    // — a robust IQR-based fit that beats a naive μ ± σ line on
    // skewed data. Two datasets: scatter (points) + line (reference).
    reg("bar", "qqplot",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) { outs[0] = Value::empty(); return; }
            std::vector<double> xs;
            for (size_t i = 0; i < args[0].numel(); ++i) {
                const double v = args[0].doubleData()[i];
                if (std::isfinite(v)) xs.push_back(v);
            }
            if (xs.size() < 2) { outs[0] = Value::empty(); return; }
            std::sort(xs.begin(), xs.end());
            const size_t N = xs.size();

            // Probit (inverse normal CDF) — Abramowitz & Stegun 26.2.23
            // rational approx, good to ~4.5e-4 absolute error. Mirrors
            // the upper tail; the lower tail is its negative.
            const auto probit = [](double p) {
                p = std::max(1e-15, std::min(1.0 - 1e-15, p));
                const bool lower = (p < 0.5);
                const double q = lower ? p : (1.0 - p);
                const double t = std::sqrt(-2.0 * std::log(q));
                const double c0 = 2.515517, c1 = 0.802853, c2 = 0.010328;
                const double d1 = 1.432788, d2 = 0.189269, d3 = 0.001308;
                const double r = t - (c0 + c1*t + c2*t*t)
                                   / (1 + d1*t + d2*t*t + d3*t*t*t);
                return lower ? -r : r;
            };

            std::ostringstream tx, ty;
            tx << '['; ty << '[';
            for (size_t i = 0; i < N; ++i) {
                if (i) { tx << ','; ty << ','; }
                tx << probit((i + 0.5) / (double)N);
                ty << xs[i];
            }
            tx << ']'; ty << ']';

            // Reference line: passes through (probit(0.25), q1) and
            // (probit(0.75), q3) where q1, q3 are the 25th / 75th
            // percentiles of the sample.
            const double q1 = xs[(size_t)((N - 1) * 0.25)];
            const double q3 = xs[(size_t)((N - 1) * 0.75)];
            const double t1 = probit(0.25), t3 = probit(0.75);
            const double slope = (q3 - q1) / (t3 - t1);
            const double intercept = q1 - slope * t1;
            // Extend slightly past the data range so the ref line
            // visibly bisects the cloud.
            const double tlo = probit(0.5 / N) - 0.1;
            const double thi = probit(1.0 - 0.5 / N) + 0.1;
            std::ostringstream lx, ly;
            lx << '[' << tlo << ',' << thi << ']';
            ly << '[' << (intercept + slope * tlo) << ',' << (intercept + slope * thi) << ']';

            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo dsLine;
            dsLine.type = "line";
            dsLine.xJson = lx.str();
            dsLine.yJson = ly.str();
            dsLine.style = "color=#d62728";
            fm.pushDataset(std::move(dsLine));
            DatasetInfo dsDot;
            dsDot.type = "scatter";
            dsDot.xJson = tx.str();
            dsDot.yJson = ty.str();
            dsDot.style = "color=#1f77b4";
            dsDot.markerSize = 3;
            fm.pushDataset(std::move(dsDot));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // pareto(Y) — bars in descending Y order + a cumulative-percent
    // line on the same axes. Useful for "80/20" / quality-control
    // visualisations. We emit two datasets:
    //   1. type=bar, X = 1..N (rank), Y sorted descending
    //   2. type=line, same X, Y = 100 * cumsum(Y) / sum(Y)
    reg("bar", "pareto",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) { outs[0] = Value::empty(); return; }
            std::vector<double> ys;
            for (size_t i = 0; i < args[0].numel(); ++i) {
                const double v = args[0].doubleData()[i];
                if (std::isfinite(v)) ys.push_back(v);
            }
            if (ys.empty()) { outs[0] = Value::empty(); return; }
            std::sort(ys.begin(), ys.end(), std::greater<double>());
            double sum = 0;
            for (double v : ys) sum += v;
            if (sum == 0) sum = 1;

            std::ostringstream bx, by, lx, ly;
            bx << '['; by << '['; lx << '['; ly << '[';
            double cum = 0;
            for (size_t i = 0; i < ys.size(); ++i) {
                if (i) { bx << ','; by << ','; lx << ','; ly << ','; }
                cum += ys[i];
                bx << (i + 1); by << ys[i];
                lx << (i + 1); ly << (100.0 * cum / sum);
            }
            bx << ']'; by << ']'; lx << ']'; ly << ']';

            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo dsBar;
            dsBar.type = "bar";
            dsBar.xJson = bx.str();
            dsBar.yJson = by.str();
            dsBar.style = "color=#1f77b4";
            fm.pushDataset(std::move(dsBar));
            DatasetInfo dsLine;
            dsLine.type = "line";
            dsLine.xJson = lx.str();
            dsLine.yJson = ly.str();
            dsLine.style = "color=#d62728";
            fm.pushDataset(std::move(dsLine));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // histfit(x[, nbins]) — histogram of x with a Gaussian fit overlay.
    // Default 10 bins. The fit uses sample mean/std; PDF is scaled by
    // (N * binWidth) so the curve is visually comparable to the bar
    // counts.
    reg("bar", "histfit",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) { outs[0] = Value::empty(); return; }
            std::vector<double> xs;
            for (size_t i = 0; i < args[0].numel(); ++i) {
                const double v = args[0].doubleData()[i];
                if (std::isfinite(v)) xs.push_back(v);
            }
            if (xs.size() < 2) { outs[0] = Value::empty(); return; }
            int nbins = 10;
            if (args.size() >= 2 && args[1].numel() == 1)
                nbins = std::max(1, (int)args[1].toScalar());

            double mn = xs[0], mx = xs[0];
            for (double v : xs) {
                if (v < mn) mn = v;
                if (v > mx) mx = v;
            }
            const double bw = (mx - mn) / nbins;
            const double bwSafe = (bw == 0) ? 1.0 : bw;
            std::vector<double> counts(nbins, 0.0), centers(nbins);
            for (int i = 0; i < nbins; ++i)
                centers[i] = mn + bwSafe * (i + 0.5);
            for (double v : xs) {
                int b = (int)((v - mn) / bwSafe);
                if (b >= nbins) b = nbins - 1;
                if (b < 0) b = 0;
                counts[b] += 1;
            }
            double mean = 0;
            for (double v : xs) mean += v;
            mean /= xs.size();
            double var = 0;
            for (double v : xs) var += (v - mean) * (v - mean);
            var /= (xs.size() > 1 ? xs.size() - 1 : 1);
            const double sd = std::sqrt(var);
            const double sdSafe = (sd > 0) ? sd : 1.0;

            // Sample the Gaussian on a fine grid for the fit curve.
            const int gridN = 100;
            std::ostringstream bx, by, fx, fy;
            bx << '['; by << '[';
            for (int i = 0; i < nbins; ++i) {
                if (i) { bx << ','; by << ','; }
                bx << centers[i]; by << counts[i];
            }
            bx << ']'; by << ']';
            fx << '['; fy << '[';
            const double scale = xs.size() * bwSafe;
            for (int g = 0; g < gridN; ++g) {
                if (g) { fx << ','; fy << ','; }
                const double xg = mn + (mx - mn) * g / (gridN - 1);
                const double pdf = std::exp(-0.5 * std::pow((xg - mean) / sdSafe, 2))
                                 / (sdSafe * std::sqrt(2 * 3.14159265358979323846));
                fx << xg; fy << (pdf * scale);
            }
            fx << ']'; fy << ']';

            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo dsBar;
            dsBar.type = "bar";
            dsBar.xJson = bx.str();
            dsBar.yJson = by.str();
            dsBar.style = "color=#aec7e8";
            fm.pushDataset(std::move(dsBar));
            DatasetInfo dsFit;
            dsFit.type = "line";
            dsFit.xJson = fx.str();
            dsFit.yJson = fy.str();
            dsFit.style = "color=#d62728";
            dsFit.lineWidth = 2;
            fm.pushDataset(std::move(dsFit));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // gscatter(x, y, g) — scatter coloured by group label. Each unique
    // value of g becomes its own scatter dataset (so it picks up a
    // distinct color from the renderer's palette).
    reg("bar", "gscatter",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.size() < 3) { outs[0] = Value::empty(); return; }
            const auto &X = args[0];
            const auto &Y = args[1];
            const auto &G = args[2];
            const size_t N = std::min({X.numel(), Y.numel(), G.numel()});
            if (N == 0) { outs[0] = Value::empty(); return; }

            // Group indices: distinct values in G, preserving first-seen
            // order. G can be numeric or char (treat both via toScalar
            // when numeric; char-array support deferred — most users
            // pass numeric labels).
            std::vector<double> groupKeys;
            std::vector<int> groupIdx(N);
            for (size_t i = 0; i < N; ++i) {
                const double k = G.doubleData()[i];
                int found = -1;
                for (size_t j = 0; j < groupKeys.size(); ++j) {
                    if (groupKeys[j] == k) { found = (int)j; break; }
                }
                if (found < 0) {
                    found = (int)groupKeys.size();
                    groupKeys.push_back(k);
                }
                groupIdx[i] = found;
            }

            // Categorical palette (ColorBrewer Set1, 8 colors).
            static const char *kPalette[] = {
                "#1f77b4", "#d62728", "#2ca02c", "#9467bd",
                "#ff7f0e", "#17becf", "#e377c2", "#7f7f7f",
            };
            const size_t nGroups = groupKeys.size();

            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            for (size_t gi = 0; gi < nGroups; ++gi) {
                std::ostringstream sx, sy;
                sx << '['; sy << '[';
                bool first = true;
                for (size_t i = 0; i < N; ++i) {
                    if (groupIdx[i] != (int)gi) continue;
                    if (!first) { sx << ','; sy << ','; }
                    first = false;
                    sx << X.doubleData()[i];
                    sy << Y.doubleData()[i];
                }
                sx << ']'; sy << ']';
                if (first) continue;
                DatasetInfo ds;
                ds.type = "scatter";
                ds.xJson = sx.str();
                ds.yJson = sy.str();
                std::ostringstream st;
                st << "color=" << kPalette[gi % 8];
                ds.style = st.str();
                ds.label = "group " + std::to_string((long long)groupKeys[gi]);
                fm.pushDataset(std::move(ds));
            }
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // spy(M) — sparsity pattern. Renders a marker at every (col, row)
    // where M is non-zero (and finite). Mirrors MATLAB convention of
    // axis ij so the matrix sits like the printed form (row 1 at top).
    reg("bar", "spy",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (args.empty()) { outs[0] = Value::empty(); return; }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            const auto &M = args[0];
            const size_t R = M.dims().rows();
            const size_t C = M.dims().cols();
            std::ostringstream xs, ys;
            xs << '['; ys << '[';
            bool first = true;
            for (size_t c = 0; c < C; ++c) {
                for (size_t r = 0; r < R; ++r) {
                    const double v = M.doubleData()[c * R + r];
                    if (v == 0.0 || !std::isfinite(v)) continue;
                    if (!first) { xs << ','; ys << ','; }
                    first = false;
                    xs << (c + 1);
                    ys << (r + 1);
                }
            }
            xs << ']'; ys << ']';
            DatasetInfo ds;
            ds.type = "scatter";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.style = "color=#1f77b4";
            ds.markerSize = 3;
            fm.pushDataset(std::move(ds));
            // axis ij so row 1 is at the top, matching MATLAB's spy.
            fm.currentAxes().yDir = "reverse";
            fm.currentAxes().axisMode = "ij";
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

            // Find optional baseline (last numeric scalar arg) and skip
            // line specs (char args).
            size_t nData = args.size();
            for (size_t i = 0; i < args.size(); ++i) {
                if (args[i].isChar()) { nData = i; break; }
            }

            // Identify the Y data Value and its layout. MATLAB
            // distinguishes vector vs matrix:
            //   area(Y)         — Y vector → 1 series; Y matrix → cols stacked
            //   area(x, Y)      — same with explicit x
            //   area(x, Y, base)— optional baseline scalar
            const Value *xData = nullptr;
            const Value *yData = nullptr;
            double baseline = 0.0;
            bool hasBase = false;
            if (nData == 1) {
                yData = &args[0];
            } else if (nData >= 2) {
                xData = &args[0];
                yData = &args[1];
                if (nData >= 3 && args[2].numel() == 1) {
                    baseline = args[2].toScalar();
                    hasBase = true;
                }
            }
            if (!yData) { outs[0] = Value::empty(); return; }

            const size_t Yr = yData->dims().rows();
            const size_t Yc = yData->dims().cols();
            // Stacked path: matrix Y with rows>1 AND cols>1.
            const bool stacked = (Yr > 1 && Yc > 1);
            // Palette colours for stacked series — sampled from a
            // distinct ramp so users don't see all-blue.
            static const char *kStackPalette[8] = {
                "#7fd99a", "#5fb3d4", "#e9b870", "#9b8cf2",
                "#e26a6a", "#d4a5e6", "#f2a37e", "#6fcfbf",
            };

            if (!stacked) {
                // Single-series path — backwards-compatible with the
                // previous wire format.
                DatasetInfo ds;
                ds.type = "area";
                if (nData == 1) {
                    ds.xJson = makeIndexJson(yData->numel());
                    ds.yJson = vecToJson(*yData);
                } else {
                    ds.xJson = vecToJson(*xData);
                    ds.yJson = vecToJson(*yData);
                }
                parsePlotArgs(args, nData, ds);
                if (hasBase) {
                    std::ostringstream os;
                    if (!ds.style.empty()) os << ds.style << ";";
                    os << "base=" << baseline;
                    ds.style = os.str();
                }
                fm.pushDataset(std::move(ds));
                fm.emitModified();
                outs[0] = Value::empty();
                return;
            }

            // ── Stacked path ──────────────────────────────────────
            // For each row r, compute cumulative sums S[r][c] =
            // sum_{c'=0..c} Y[r][c']. Emit one dataset per column of
            // Y, each with y = S[r][c]; render order is REVERSE so
            // the topmost band (largest cumulative) is drawn first
            // and gets overdrawn from below — yielding visible bands
            // for c = Yc-1, Yc-2, ..., 0.
            std::vector<std::vector<double>> S(Yr, std::vector<double>(Yc, 0.0));
            const double *Yp = yData->doubleData();
            for (size_t r = 0; r < Yr; ++r) {
                double acc = 0;
                for (size_t c = 0; c < Yc; ++c) {
                    acc += Yp[c * Yr + r];   // column-major
                    S[r][c] = acc;
                }
            }
            // X vector — explicit when given, else 1..Yr.
            std::ostringstream xs;
            xs << '[';
            if (xData) {
                for (size_t r = 0; r < Yr; ++r) {
                    if (r) xs << ',';
                    xs << xData->doubleData()[r];
                }
            } else {
                for (size_t r = 0; r < Yr; ++r) {
                    if (r) xs << ',';
                    xs << (r + 1);
                }
            }
            xs << ']';
            const std::string xJsonStr = xs.str();

            // Emit datasets c = Yc-1 down to 0 (topmost band first).
            for (long long cc = (long long)Yc - 1; cc >= 0; --cc) {
                DatasetInfo ds;
                ds.type = "area";
                ds.xJson = xJsonStr;
                std::ostringstream ys;
                ys << '[';
                for (size_t r = 0; r < Yr; ++r) {
                    if (r) ys << ',';
                    ys << S[r][(size_t)cc];
                }
                ys << ']';
                ds.yJson = ys.str();
                std::ostringstream sty;
                sty << "color=" << kStackPalette[(size_t)cc % 8];
                if (hasBase) sty << ";base=" << baseline;
                ds.style = sty.str();
                fm.pushDataset(std::move(ds));
            }
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
            fm.pushDataset(std::move(ds));
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
            fm.pushDataset(std::move(ds));
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
            fm.pushDataset(std::move(ds));
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

            fm.pushDataset(std::move(ds));
            // NOTE: axis-ij setting moved OUT of heatmapImpl. Setting yDir
            // here would ride on emitModified() below, freezing it into
            // the JSON before histogram2 (which delegates here) gets a
            // chance to override. The imagesc wrapper now applies axis ij
            // BEFORE delegating; histogram2 leaves yDir alone.
            fm.emitModified();
            outs[0] = Value::empty();
    };  // heatmapImpl

    using namespace std::placeholders;
    // imagesc wrapper: apply MATLAB axis ij (yDir='reverse', matrix-row-1
    // at top) BEFORE delegating. heatmapImpl will then emit the JSON with
    // the new yDir baked in. pcolor and histogram2 (which both delegate
    // here too) stay on axis xy because they don't go through this wrapper.
    reg("image", "imagesc",
        [heatmapImpl](Span<const Value> args, size_t nargout,
                      Span<Value> outs, CallContext &ctx) {
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            if (fm.currentAxes().axisMode.empty()) {
                fm.currentAxes().axisMode = "ij";
            }
            fm.currentAxes().yDir = "reverse";
            heatmapImpl("imagesc", args, nargout, outs, ctx);
        });
    reg("image", "pcolor",  std::bind(heatmapImpl, "pcolor",  _1, _2, _3, _4));

    // ────────────────────────────────────────────────────────────────
    // imshow — display image. MATLAB:
    //   imshow(I)            — grayscale, range = type-default
    //                          (uint8 → [0,255], double/single → [0,1],
    //                           logical → [0,1])
    //   imshow(I, [lo hi])   — grayscale with explicit display range
    //   imshow(I, [])        — grayscale auto-range (== imagesc behaviour)
    //   imshow(RGB)          — truecolor, RGB is M×N×3 (uint8 or double).
    //                          double in [0,1] → cast *255 to uint8.
    //
    // Compared to imagesc, imshow:
    //   • defaults colormap to "gray" (grayscale only; RGB ignores cmap)
    //   • forces axisMode='image' (1:1 pixels, equal aspect)
    //   • forces axisVisible=false (no ticks / labels / box)
    //   • forces yDir='reverse' (matrix-row=1 at top)
    // Existing user-set values for colormap/axisMode survive.
    //
    // Deferred (audit/findings/graphics/imshow.md): filename input,
    // 'XData','YData','InitialMagnification','Border','Reduce',
    // 'Colormap' name-value pairs.
    auto imshowImpl = [](Span<const Value> args, size_t nargout,
                         Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();
        auto &ax = fm.currentAxes();

        if (args.empty()) { outs[0] = Value::empty(); return; }

        // imshow('path/to/img.png') — decode via stb_image and feed
        // the resulting H×W or H×W×{3,4} uint8 array through the rest
        // of the pipeline. imread errors propagate up as Engine
        // exceptions; we don't try/catch here.
        Value decoded;   // owns lifetime of the decoded value
        const Value *img0 = &args[0];
        if (args[0].isChar()) {
            decoded = numkit::image::imread(ctx.engine->resource(),
                                            args[0].toString());
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
            outs[0] = Value::empty();
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
        //
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
            // crash; visual effect is BACKLOG. See audit/findings/
            // graphics/imshow.md.
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
        outs[0] = Value::empty();
    };
    reg("image", "imshow", imshowImpl);

    // histogram2(X, Y) — 2-D histogram. Bins (X, Y) into an nx×ny grid
    // and renders the count matrix as an imagesc-style heatmap. Param
    // forms supported (positional, like MATLAB's classic call):
    //   histogram2(X, Y)              — 10×10 bins over data extent
    //   histogram2(X, Y, n)           — n×n
    //   histogram2(X, Y, [nx ny])     — explicit grid
    //   histogram2(X, Y, nx, ny)      — explicit grid (separate args)
    // Name-Value form (NumBins, BinEdges, …) is on the BACKLOG.
    reg("bar", "histogram2",
        [heatmapImpl](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto *mr = ctx.engine->resource();
            if (args.size() < 2 || args[0].numel() == 0 || args[1].numel() == 0) {
                outs[0] = Value::empty();
                return;
            }
            const auto &X = args[0];
            const auto &Y = args[1];
            const size_t N = std::min(X.numel(), Y.numel());

            int nx = 10, ny = 10;
            if (args.size() >= 3) {
                if (args[2].numel() >= 2) {
                    nx = (int)args[2].doubleData()[0];
                    ny = (int)args[2].doubleData()[1];
                } else if (args[2].numel() == 1) {
                    nx = ny = (int)args[2].toScalar();
                    if (args.size() >= 4 && args[3].numel() == 1)
                        ny = (int)args[3].toScalar();
                }
            }
            if (nx < 1) nx = 1;
            if (ny < 1) ny = 1;

            double xmn = X.doubleData()[0], xmx = xmn;
            double ymn = Y.doubleData()[0], ymx = ymn;
            for (size_t k = 1; k < N; ++k) {
                const double xv = X.doubleData()[k];
                const double yv = Y.doubleData()[k];
                if (std::isfinite(xv)) {
                    if (xv < xmn) xmn = xv;
                    if (xv > xmx) xmx = xv;
                }
                if (std::isfinite(yv)) {
                    if (yv < ymn) ymn = yv;
                    if (yv > ymx) ymx = yv;
                }
            }
            double bwx = (xmx - xmn) / nx;
            double bwy = (ymx - ymn) / ny;
            if (bwx == 0) bwx = 1;
            if (bwy == 0) bwy = 1;

            auto centers_x = Value::matrix(1, (size_t)nx, ValueType::DOUBLE, mr);
            auto centers_y = Value::matrix(1, (size_t)ny, ValueType::DOUBLE, mr);
            // counts is row-major-as-Y, col-major-as-X (i.e. ny rows, nx
            // cols). MATLAB stores column-major, so element (j, i) is at
            // linear index i * ny + j.
            auto counts = Value::matrix((size_t)ny, (size_t)nx, ValueType::DOUBLE, mr);
            for (int i = 0; i < nx; ++i)
                centers_x.doubleDataMut()[i] = xmn + bwx * (i + 0.5);
            for (int j = 0; j < ny; ++j)
                centers_y.doubleDataMut()[j] = ymn + bwy * (j + 0.5);
            for (size_t k = 0; k < N; ++k) {
                const double xv = X.doubleData()[k];
                const double yv = Y.doubleData()[k];
                if (!std::isfinite(xv) || !std::isfinite(yv))
                    continue;
                int i = (int)((xv - xmn) / bwx);
                int j = (int)((yv - ymn) / bwy);
                if (i >= nx) i = nx - 1;
                if (j >= ny) j = ny - 1;
                if (i < 0) i = 0;
                if (j < 0) j = 0;
                counts.doubleDataMut()[(size_t)i * (size_t)ny + (size_t)j] += 1.0;
            }
            // Delegate to the heatmap pipeline. typeName='imagesc' so the
            // adapter routes through the existing heatmap renderer with
            // its full quantization + LUT path.
            std::vector<Value> proxied = { centers_x, centers_y, counts };
            // Delegate directly to heatmapImpl. histogram2 stays on axis
            // xy by default — heatmapImpl no longer forces axis ij; only
            // the imagesc wrapper does that.
            heatmapImpl("imagesc", Span<const Value>(proxied.data(), proxied.size()),
                        nargout, outs, ctx);
        });

    // ── Contour — marching squares over Z(R, C) ────────────────────────
    // contour(Z) / contour(Z, n) / contour(Z, levels)
    // contour(X, Y, Z[, n|levels])
    // We don't reuse the imagesc heatmap path — contour produces 1-D
    // line layers instead of a 2-D raster. Each level becomes its own
    // DatasetInfo with type='line' and an inline color (HSL→RGB ramp
    // through the z-extent), with NaN separators between segments so
    // the existing line renderer can draw them as one path.
    auto contourImpl = [](Span<const Value> args, size_t nargout,
                          Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();

        const Value *Z_arg = nullptr;
        const Value *X_arg = nullptr;
        const Value *Y_arg = nullptr;
        const Value *levels_arg = nullptr;
        if (args.size() == 1) {
            Z_arg = &args[0];
        } else if (args.size() == 2) {
            Z_arg = &args[0];
            levels_arg = &args[1];
        } else if (args.size() == 3) {
            X_arg = &args[0]; Y_arg = &args[1]; Z_arg = &args[2];
        } else if (args.size() >= 4) {
            X_arg = &args[0]; Y_arg = &args[1]; Z_arg = &args[2];
            levels_arg = &args[3];
        }
        if (!Z_arg) { outs[0] = Value::empty(); return; }

        const size_t R = Z_arg->dims().rows();
        const size_t C = Z_arg->dims().cols();
        if (R < 2 || C < 2) { outs[0] = Value::empty(); return; }

        const auto Zat = [&](size_t r, size_t c) {
            return Z_arg->doubleData()[c * R + r];   // column-major
        };

        std::vector<double> Xs(C), Ys(R);
        if (X_arg && Y_arg && X_arg->numel() >= C && Y_arg->numel() >= R) {
            for (size_t c = 0; c < C; ++c) Xs[c] = X_arg->doubleData()[c];
            for (size_t r = 0; r < R; ++r) Ys[r] = Y_arg->doubleData()[r];
        } else {
            for (size_t c = 0; c < C; ++c) Xs[c] = (double)(c + 1);
            for (size_t r = 0; r < R; ++r) Ys[r] = (double)(r + 1);
        }

        double zmn = std::numeric_limits<double>::infinity();
        double zmx = -std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < R * C; ++i) {
            const double v = Z_arg->doubleData()[i];
            if (std::isfinite(v)) {
                if (v < zmn) zmn = v;
                if (v > zmx) zmx = v;
            }
        }

        std::vector<double> levels;
        int n = 10;
        if (levels_arg) {
            if (levels_arg->numel() == 1) {
                n = (int)levels_arg->toScalar();
                if (n < 1) n = 1;
            } else {
                const double *p = levels_arg->doubleData();
                for (size_t i = 0; i < levels_arg->numel(); ++i)
                    levels.push_back(p[i]);
            }
        }
        if (levels.empty()) {
            // Equally-spaced levels strictly inside (zmn, zmx) so the
            // boundary cases (level == data) don't degenerate.
            if (n == 1) {
                levels.push_back((zmn + zmx) / 2.0);
            } else {
                const double step = (zmx - zmn) / (n + 1);
                for (int i = 1; i <= n; ++i)
                    levels.push_back(zmn + step * i);
            }
        }

        const auto interp = [](double a, double b, double va, double vb, double L) {
            if (std::abs(vb - va) < 1e-15) return a;
            return a + (L - va) / (vb - va) * (b - a);
        };

        // HSL → RGB ramp helper: blue (240°) at zmn → red (0°) at zmx.
        const auto colorForLevel = [&](double L) {
            const double t = (zmx == zmn) ? 0.5 : (L - zmn) / (zmx - zmn);
            const double Hd = (1.0 - std::clamp(t, 0.0, 1.0)) * 240.0;
            const double Cr = 0.6;
            const double H = Hd / 60.0;
            const double Xc = Cr * (1.0 - std::abs(std::fmod(H, 2.0) - 1.0));
            double r1 = 0, g1 = 0, b1 = 0;
            if      (H < 1) { r1 = Cr; g1 = Xc; b1 = 0; }
            else if (H < 2) { r1 = Xc; g1 = Cr; b1 = 0; }
            else if (H < 3) { r1 = 0;  g1 = Cr; b1 = Xc; }
            else if (H < 4) { r1 = 0;  g1 = Xc; b1 = Cr; }
            else if (H < 5) { r1 = Xc; g1 = 0;  b1 = Cr; }
            else            { r1 = Cr; g1 = 0;  b1 = Xc; }
            const double m = 0.5 - Cr / 2.0;
            const int R8 = (int)((r1 + m) * 255);
            const int G8 = (int)((g1 + m) * 255);
            const int B8 = (int)((b1 + m) * 255);
            char buf[16];
            std::snprintf(buf, sizeof buf, "color=#%02x%02x%02x", R8, G8, B8);
            return std::string(buf);
        };

        // Marching squares per level. Each cell's 4-bit code (TL, TR, BR, BL)
        // maps to 0/1/2 segments on the cell's edges.
        struct Pt { double x, y; };
        for (double L : levels) {
            std::ostringstream xs, ys;
            xs << '['; ys << '[';
            bool first = true;
            for (size_t r = 0; r + 1 < R; ++r) {
                for (size_t c = 0; c + 1 < C; ++c) {
                    const double v_tl = Zat(r,     c);
                    const double v_tr = Zat(r,     c + 1);
                    const double v_bl = Zat(r + 1, c);
                    const double v_br = Zat(r + 1, c + 1);
                    if (!std::isfinite(v_tl) || !std::isfinite(v_tr)
                     || !std::isfinite(v_bl) || !std::isfinite(v_br)) continue;
                    int code = 0;
                    if (v_tl > L) code |= 1;
                    if (v_tr > L) code |= 2;
                    if (v_br > L) code |= 4;
                    if (v_bl > L) code |= 8;
                    if (code == 0 || code == 15) continue;

                    const double xL = Xs[c], xR = Xs[c + 1];
                    const double yT = Ys[r], yB = Ys[r + 1];
                    const Pt T   = { interp(xL, xR, v_tl, v_tr, L), yT };
                    const Pt Re  = { xR, interp(yT, yB, v_tr, v_br, L) };
                    const Pt B   = { interp(xL, xR, v_bl, v_br, L), yB };
                    const Pt Le  = { xL, interp(yT, yB, v_tl, v_bl, L) };

                    std::array<std::pair<Pt, Pt>, 2> segs;
                    int nseg = 0;
                    switch (code) {
                        case 1:  case 14: segs[nseg++] = { Le, T }; break;
                        case 2:  case 13: segs[nseg++] = { T, Re }; break;
                        case 3:  case 12: segs[nseg++] = { Le, Re }; break;
                        case 4:  case 11: segs[nseg++] = { Re, B }; break;
                        case 6:  case 9:  segs[nseg++] = { T, B }; break;
                        case 7:  case 8:  segs[nseg++] = { Le, B }; break;
                        case 5:  segs[nseg++] = { Le, T }; segs[nseg++] = { Re, B }; break;
                        case 10: segs[nseg++] = { Le, B }; segs[nseg++] = { T, Re }; break;
                    }
                    for (int s = 0; s < nseg; ++s) {
                        if (!first) { xs << ",null,"; ys << ",null,"; }
                        first = false;
                        xs << segs[s].first.x  << ',' << segs[s].second.x;
                        ys << segs[s].first.y  << ',' << segs[s].second.y;
                    }
                }
            }
            xs << ']'; ys << ']';
            if (first) continue;   // no segments at this level

            DatasetInfo ds;
            ds.type = "line";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.style = colorForLevel(L);
            fm.pushDataset(std::move(ds));
        }
        fm.emitModified();
        outs[0] = Value::empty();
    };
    reg("contour", "contour", contourImpl);

    // ────────────────────────────────────────────────────────────────
    // contourf — filled bands between consecutive levels.
    //
    // Strategy: for each level L from highest to lowest, draw the
    // closed polygon "Z >= L" with a colour from the colormap at that
    // level. Because levels are drawn in descending order each layer
    // overdraws a smaller region of the previous one, producing the
    // classic MATLAB filled-contour banded effect.
    //
    // Per cell with values (v_TL, v_TR, v_BR, v_BL) and the four
    // corner-points (TL, TR, BR, BL), we compute a 4-bit bitmask of
    // "corner is inside (z >= L)" and look up the polygon vertices
    // from a 16-entry table. Saddle codes (5, 10) are split into two
    // disjoint triangles — visually equivalent to MATLAB's
    // disambiguation in our usage (the renderer fills the union).
    // ────────────────────────────────────────────────────────────────
    auto contourfImpl = [](Span<const Value> args, size_t nargout,
                           Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();

        const Value *Z_arg = nullptr;
        const Value *X_arg = nullptr;
        const Value *Y_arg = nullptr;
        const Value *levels_arg = nullptr;
        if (args.size() == 1) { Z_arg = &args[0]; }
        else if (args.size() == 2) { Z_arg = &args[0]; levels_arg = &args[1]; }
        else if (args.size() == 3) { X_arg = &args[0]; Y_arg = &args[1]; Z_arg = &args[2]; }
        else if (args.size() >= 4) {
            X_arg = &args[0]; Y_arg = &args[1]; Z_arg = &args[2];
            levels_arg = &args[3];
        }
        if (!Z_arg) { outs[0] = Value::empty(); return; }

        const size_t R = Z_arg->dims().rows();
        const size_t C = Z_arg->dims().cols();
        if (R < 2 || C < 2) { outs[0] = Value::empty(); return; }

        const auto Zat = [&](size_t r, size_t c) {
            return Z_arg->doubleData()[c * R + r];
        };
        std::vector<double> Xs(C), Ys(R);
        if (X_arg && Y_arg && X_arg->numel() >= C && Y_arg->numel() >= R) {
            for (size_t c = 0; c < C; ++c) Xs[c] = X_arg->doubleData()[c];
            for (size_t r = 0; r < R; ++r) Ys[r] = Y_arg->doubleData()[r];
        } else {
            for (size_t c = 0; c < C; ++c) Xs[c] = (double)(c + 1);
            for (size_t r = 0; r < R; ++r) Ys[r] = (double)(r + 1);
        }

        double zmn = std::numeric_limits<double>::infinity();
        double zmx = -std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < R * C; ++i) {
            const double v = Z_arg->doubleData()[i];
            if (std::isfinite(v)) {
                if (v < zmn) zmn = v;
                if (v > zmx) zmx = v;
            }
        }

        std::vector<double> levels;
        int n = 10;
        if (levels_arg) {
            if (levels_arg->numel() == 1) {
                n = (int)levels_arg->toScalar();
                if (n < 1) n = 1;
            } else {
                const double *p = levels_arg->doubleData();
                for (size_t i = 0; i < levels_arg->numel(); ++i)
                    levels.push_back(p[i]);
            }
        }
        if (levels.empty()) {
            if (n == 1) {
                levels.push_back((zmn + zmx) / 2.0);
            } else {
                const double step = (zmx - zmn) / (n + 1);
                for (int i = 1; i <= n; ++i)
                    levels.push_back(zmn + step * i);
            }
        }
        // Bottom band — covers the entire data extent. Drawn first
        // (lightest colour) so descending layers sit on top.
        std::sort(levels.begin(), levels.end());

        const auto interp = [](double a, double b, double va, double vb, double L) {
            if (std::abs(vb - va) < 1e-15) return a;
            return a + (L - va) / (vb - va) * (b - a);
        };

        // HSL→RGB ramp same as contour (blue→red across [zmn, zmx]).
        const auto colorForLevel = [&](double L) {
            const double t = (zmx == zmn) ? 0.5 : (L - zmn) / (zmx - zmn);
            const double Hd = (1.0 - std::clamp(t, 0.0, 1.0)) * 240.0;
            const double Cr = 0.6;
            const double H = Hd / 60.0;
            const double Xc = Cr * (1.0 - std::abs(std::fmod(H, 2.0) - 1.0));
            double r1 = 0, g1 = 0, b1 = 0;
            if      (H < 1) { r1 = Cr; g1 = Xc; b1 = 0; }
            else if (H < 2) { r1 = Xc; g1 = Cr; b1 = 0; }
            else if (H < 3) { r1 = 0;  g1 = Cr; b1 = Xc; }
            else if (H < 4) { r1 = 0;  g1 = Xc; b1 = Cr; }
            else if (H < 5) { r1 = Xc; g1 = 0;  b1 = Cr; }
            else            { r1 = Cr; g1 = 0;  b1 = Xc; }
            const double m = 0.5 - Cr / 2.0;
            const int R8 = (int)((r1 + m) * 255);
            const int G8 = (int)((g1 + m) * 255);
            const int B8 = (int)((b1 + m) * 255);
            char buf[40];
            std::snprintf(buf, sizeof buf, "color=#%02x%02x%02x;fillOpacity=1.0", R8, G8, B8);
            return std::string(buf);
        };

        // Bottom layer — full extent — coloured at the lowest band so
        // any cell whose minimum corner is below the smallest level
        // gets a colour. Drawn first (lowest stack position).
        {
            std::ostringstream xs, ys;
            xs << '[' << Xs[0] << ',' << Xs[C - 1] << ',' << Xs[C - 1]
               << ',' << Xs[0] << ']';
            ys << '[' << Ys[0] << ',' << Ys[0] << ',' << Ys[R - 1]
               << ',' << Ys[R - 1] << ']';
            DatasetInfo ds;
            ds.type = "polygon";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.style = colorForLevel(zmn);
            fm.pushDataset(std::move(ds));
        }

        // For each level (ascending), draw the closed region "Z >= L"
        // as one combined polygon dataset with null separators. Each
        // cell contributes 0..6 vertices depending on its 4-bit code.
        struct Pt { double x, y; };
        for (double L : levels) {
            std::ostringstream xs, ys;
            xs << '['; ys << '[';
            bool first = true;
            const auto pushPt = [&](const Pt &p) {
                if (!first) { xs << ','; ys << ','; }
                first = false;
                xs << p.x; ys << p.y;
            };
            const auto closeRing = [&] {
                xs << ",null"; ys << ",null";
                first = true;   // next cell starts a fresh sub-path
            };

            for (size_t r = 0; r + 1 < R; ++r) {
                for (size_t c = 0; c + 1 < C; ++c) {
                    const double v_tl = Zat(r,     c);
                    const double v_tr = Zat(r,     c + 1);
                    const double v_br = Zat(r + 1, c + 1);
                    const double v_bl = Zat(r + 1, c);
                    if (!std::isfinite(v_tl) || !std::isfinite(v_tr)
                     || !std::isfinite(v_bl) || !std::isfinite(v_br)) continue;
                    int code = 0;
                    if (v_tl >= L) code |= 1;
                    if (v_tr >= L) code |= 2;
                    if (v_br >= L) code |= 4;
                    if (v_bl >= L) code |= 8;
                    if (code == 0) continue;
                    const double xL = Xs[c], xR = Xs[c + 1];
                    const double yT = Ys[r], yB = Ys[r + 1];
                    const Pt TL = { xL, yT }, TR = { xR, yT };
                    const Pt BR = { xR, yB }, BL = { xL, yB };
                    const Pt T   = { interp(xL, xR, v_tl, v_tr, L), yT };
                    const Pt Re_ = { xR, interp(yT, yB, v_tr, v_br, L) };
                    const Pt B   = { interp(xL, xR, v_bl, v_br, L), yB };
                    const Pt Le  = { xL, interp(yT, yB, v_tl, v_bl, L) };
                    auto emitRing = [&](std::initializer_list<Pt> ring) {
                        bool firstInRing = true;
                        for (const auto &p : ring) {
                            if (!firstInRing) { xs << ','; ys << ','; }
                            else if (!first) { xs << ",null,"; ys << ",null,"; }
                            firstInRing = false;
                            first = false;
                            xs << p.x; ys << p.y;
                        }
                    };
                    switch (code) {
                        case 15: emitRing({ TL, TR, BR, BL }); break;
                        case 1:  emitRing({ TL, T, Le }); break;
                        case 2:  emitRing({ TR, Re_, T }); break;
                        case 4:  emitRing({ BR, B, Re_ }); break;
                        case 8:  emitRing({ BL, Le, B }); break;
                        case 3:  emitRing({ TL, TR, Re_, Le }); break;
                        case 6:  emitRing({ TR, BR, B, T }); break;
                        case 12: emitRing({ BR, BL, Le, Re_ }); break;
                        case 9:  emitRing({ TL, T, B, BL }); break;
                        case 7:  emitRing({ TL, TR, BR, B, Le }); break;
                        case 11: emitRing({ TL, TR, Re_, B, BL }); break;
                        case 13: emitRing({ TL, T, Re_, BR, BL }); break;
                        case 14: emitRing({ TR, BR, BL, Le, T }); break;
                        // Saddles — draw as two disjoint triangles.
                        case 5:  emitRing({ TL, T, Le });
                                 emitRing({ BR, B, Re_ }); break;
                        case 10: emitRing({ TR, Re_, T });
                                 emitRing({ BL, Le, B }); break;
                        default: break;
                    }
                    (void)pushPt; (void)closeRing;
                }
            }
            xs << ']'; ys << ']';
            if (first) continue;
            DatasetInfo ds;
            ds.type = "polygon";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.style = colorForLevel(L);
            fm.pushDataset(std::move(ds));
        }
        fm.emitModified();
        outs[0] = Value::empty();
    };
    reg("contour", "contourf", contourfImpl);

    // ── Surf / mesh — wireframe quad mesh under cabinet projection ──
    // surf(Z) / surf(X, Y, Z); mesh(...) shares the same body. Real
    // face shading + lighting is deferred; for now we emit the wire
    // skeleton (rows + cols) as plot3-style line segments and let the
    // existing cabinet projection in the JS adapter render it.
    auto surfImpl = [](const char *typeName,
                       Span<const Value> args, size_t nargout,
                       Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();

        const Value *Z_arg = nullptr;
        const Value *X_arg = nullptr;
        const Value *Y_arg = nullptr;
        if (args.size() == 1) {
            Z_arg = &args[0];
        } else if (args.size() >= 3) {
            X_arg = &args[0]; Y_arg = &args[1]; Z_arg = &args[2];
        }
        if (!Z_arg) { outs[0] = Value::empty(); return; }

        const size_t R = Z_arg->dims().rows();
        const size_t C = Z_arg->dims().cols();
        if (R < 2 || C < 2) { outs[0] = Value::empty(); return; }

        const auto Zat = [&](size_t r, size_t c) {
            return Z_arg->doubleData()[c * R + r];
        };
        std::vector<double> Xs(C), Ys(R);
        if (X_arg && Y_arg && X_arg->numel() >= C && Y_arg->numel() >= R) {
            for (size_t c = 0; c < C; ++c) Xs[c] = X_arg->doubleData()[c];
            for (size_t r = 0; r < R; ++r) Ys[r] = Y_arg->doubleData()[r];
        } else {
            for (size_t c = 0; c < C; ++c) Xs[c] = (double)(c + 1);
            for (size_t r = 0; r < R; ++r) Ys[r] = (double)(r + 1);
        }

        const std::string kind(typeName);
        if (kind == "surf") {
            // Face-shaded surface: emit a single dataset carrying the
            // X / Y vectors and the Z-matrix as nested rows. The WebGL
            // renderer builds an indexed triangle mesh from this with
            // per-vertex colors sampled from a colormap by Z.
            std::ostringstream xs, ys, zs;
            xs << '[';
            for (size_t c = 0; c < C; ++c) {
                if (c) xs << ',';
                xs << Xs[c];
            }
            xs << ']';
            ys << '[';
            for (size_t r = 0; r < R; ++r) {
                if (r) ys << ',';
                ys << Ys[r];
            }
            ys << ']';
            zs << '[';
            for (size_t r = 0; r < R; ++r) {
                if (r) zs << ',';
                zs << '[';
                for (size_t c = 0; c < C; ++c) {
                    if (c) zs << ',';
                    const double v = Zat(r, c);
                    if (std::isfinite(v)) zs << v;
                    else                  zs << "null";
                }
                zs << ']';
            }
            zs << ']';
            DatasetInfo ds;
            ds.type = "surf";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.zJson = zs.str();
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
            return;
        }

        // mesh — wireframe: emit two big plot3 polylines (horizontal
        // sweep + vertical sweep) so the existing 3-D line renderer
        // draws the lattice.
        std::ostringstream xH, yH, zH;
        xH << '['; yH << '['; zH << '[';
        size_t hCount = 0;
        for (size_t r = 0; r < R; ++r) {
            for (size_t c = 0; c < C; ++c) {
                if (hCount > 0) {
                    const char *sep = (c == 0) ? ",null," : ",";
                    xH << sep; yH << sep; zH << sep;
                }
                xH << Xs[c]; yH << Ys[r]; zH << Zat(r, c);
                ++hCount;
            }
        }
        xH << ']'; yH << ']'; zH << ']';

        std::ostringstream xV, yV, zV;
        xV << '['; yV << '['; zV << '[';
        size_t vCount = 0;
        for (size_t c = 0; c < C; ++c) {
            for (size_t r = 0; r < R; ++r) {
                if (vCount > 0) {
                    const char *sep = (r == 0) ? ",null," : ",";
                    xV << sep; yV << sep; zV << sep;
                }
                xV << Xs[c]; yV << Ys[r]; zV << Zat(r, c);
                ++vCount;
            }
        }
        xV << ']'; yV << ']'; zV << ']';

        // Two datasets so adapter applies cabinet projection via the
        // plot3 path. Color uses a simple steel-blue stroke (mesh look).
        DatasetInfo dsH;
        dsH.type = "plot3";
        dsH.xJson = xH.str();
        dsH.yJson = yH.str();
        dsH.zJson = zH.str();
        dsH.style = "color=#4a90b8";
        fm.pushDataset(std::move(dsH));

        DatasetInfo dsV;
        dsV.type = "plot3";
        dsV.xJson = xV.str();
        dsV.yJson = yV.str();
        dsV.zJson = zV.str();
        dsV.style = "color=#4a90b8";
        fm.pushDataset(std::move(dsV));

        fm.emitModified();
        outs[0] = Value::empty();
    };
    {
        using namespace std::placeholders;
        reg("surface", "surf", std::bind(surfImpl, "surf", _1, _2, _3, _4));
        reg("surface", "mesh", std::bind(surfImpl, "mesh", _1, _2, _3, _4));
    }

    // ────────────────────────────────────────────────────────────────
    // slice — axis-aligned cross sections of a 3-D scalar volume.
    //
    // Forms supported:
    //   slice(V, sx, sy, sz)           — V is M×N×P
    //   slice(X, Y, Z, V, sx, sy, sz)  — explicit grid coords
    //
    // sx / sy / sz are vectors (or empty) of slice coordinates along
    // each axis. Each entry produces one 2-D plane of quads at that
    // axis position, picking volume values via nearest-neighbour
    // sampling (linear interpolation along the slice axis is a
    // follow-up). Each plane is emitted as a single polygon3d
    // dataset; colour is the volume's mean value mapped through the
    // contour HSL ramp — per-cell colormap on the WebGL side is in
    // BACKLOG (needs vertexColors plumbing in buildPolygon3D).
    // ────────────────────────────────────────────────────────────────
    auto sliceImpl = [](Span<const Value> args, size_t nargout,
                        Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();

        const Value *V = nullptr;
        const Value *X = nullptr;
        const Value *Y = nullptr;
        const Value *Z = nullptr;
        const Value *sx = nullptr;
        const Value *sy = nullptr;
        const Value *sz = nullptr;
        if (args.size() == 4) {
            V = &args[0]; sx = &args[1]; sy = &args[2]; sz = &args[3];
        } else if (args.size() >= 7) {
            X = &args[0]; Y = &args[1]; Z = &args[2]; V = &args[3];
            sx = &args[4]; sy = &args[5]; sz = &args[6];
        } else {
            outs[0] = Value::empty();
            return;
        }
        if (!V || V->dims().ndims() != 3) {
            outs[0] = Value::empty();
            return;
        }

        const size_t M = V->dims().rows();
        const size_t N = V->dims().cols();
        const size_t P = V->dims().pages();
        if (M < 2 || N < 2 || P < 2) {
            outs[0] = Value::empty();
            return;
        }

        // Default coordinate vectors when X/Y/Z aren't supplied.
        std::vector<double> Xs(N), Ys(M), Zs(P);
        if (X && Y && Z && X->numel() >= N && Y->numel() >= M && Z->numel() >= P) {
            for (size_t i = 0; i < N; ++i) Xs[i] = X->doubleData()[i];
            for (size_t i = 0; i < M; ++i) Ys[i] = Y->doubleData()[i];
            for (size_t i = 0; i < P; ++i) Zs[i] = Z->doubleData()[i];
        } else {
            for (size_t i = 0; i < N; ++i) Xs[i] = (double)(i + 1);
            for (size_t i = 0; i < M; ++i) Ys[i] = (double)(i + 1);
            for (size_t i = 0; i < P; ++i) Zs[i] = (double)(i + 1);
        }

        // Compute the volume value range for the single representative
        // colour per slice. (Per-cell colormap is BACKLOG.)
        double vmn = std::numeric_limits<double>::infinity();
        double vmx = -std::numeric_limits<double>::infinity();
        const size_t Vn = M * N * P;
        for (size_t i = 0; i < Vn; ++i) {
            const double v = V->elemAsDouble(i);
            if (std::isfinite(v)) {
                if (v < vmn) vmn = v;
                if (v > vmx) vmx = v;
            }
        }
        if (!std::isfinite(vmn)) { vmn = 0; vmx = 1; }
        const double vmid = (vmn + vmx) * 0.5;

        const auto rgbForValue = [&](double L) -> std::array<int, 3> {
            const double t = (vmx == vmn) ? 0.5 : (L - vmn) / (vmx - vmn);
            const double Hd = (1.0 - std::clamp(t, 0.0, 1.0)) * 240.0;
            const double Cr = 0.6;
            const double H = Hd / 60.0;
            const double Xc = Cr * (1.0 - std::abs(std::fmod(H, 2.0) - 1.0));
            double r1 = 0, g1 = 0, b1 = 0;
            if      (H < 1) { r1 = Cr; g1 = Xc; b1 = 0; }
            else if (H < 2) { r1 = Xc; g1 = Cr; b1 = 0; }
            else if (H < 3) { r1 = 0;  g1 = Cr; b1 = Xc; }
            else if (H < 4) { r1 = 0;  g1 = Xc; b1 = Cr; }
            else if (H < 5) { r1 = Xc; g1 = 0;  b1 = Cr; }
            else            { r1 = Cr; g1 = 0;  b1 = Xc; }
            const double m = 0.5 - Cr / 2.0;
            return { (int)((r1 + m) * 255),
                     (int)((g1 + m) * 255),
                     (int)((b1 + m) * 255) };
        };
        const auto colorForValue = [&](double L) {
            const auto rgb = rgbForValue(L);
            char buf[40];
            std::snprintf(buf, sizeof buf,
                          "color=#%02x%02x%02x;fillOpacity=0.85",
                          rgb[0], rgb[1], rgb[2]);
            return std::string(buf);
        };

        // Index of the volume in column-major + page-major order: the
        // value at logical (i_row, j_col, k_page) is at
        // V[k * M * N + j * M + i].
        const auto Vat = [&](size_t i, size_t j, size_t k) {
            return V->elemAsDouble(k * M * N + j * M + i);
        };

        // Helper — emit a 4-vertex CCW quad-loop, terminated by null.
        // Also pushes one RGB triplet per vertex into the parallel
        // vertexColors stream so the renderer can colour each quad
        // by the local volume value.
        const auto emitQuad = [&rgbForValue](
                std::ostringstream &xs,
                std::ostringstream &ys,
                std::ostringstream &zs,
                std::ostringstream &cs,
                bool &first,
                double X0, double Y0, double Z0, double v0,
                double X1, double Y1, double Z1, double v1,
                double X2, double Y2, double Z2, double v2,
                double X3, double Y3, double Z3, double v3) {
            if (!first) {
                xs << ",null,"; ys << ",null,"; zs << ",null,";
                cs << ',';
            }
            first = false;
            xs << X0 << ',' << X1 << ',' << X2 << ',' << X3;
            ys << Y0 << ',' << Y1 << ',' << Y2 << ',' << Y3;
            zs << Z0 << ',' << Z1 << ',' << Z2 << ',' << Z3;
            const auto c0 = rgbForValue(v0);
            const auto c1 = rgbForValue(v1);
            const auto c2 = rgbForValue(v2);
            const auto c3 = rgbForValue(v3);
            cs << c0[0] << ',' << c0[1] << ',' << c0[2] << ','
               << c1[0] << ',' << c1[1] << ',' << c1[2] << ','
               << c2[0] << ',' << c2[1] << ',' << c2[2] << ','
               << c3[0] << ',' << c3[1] << ',' << c3[2];
        };

        const auto pushSlice = [&](char axis, size_t fixedIdx,
                                   double meanVal) {
            (void)meanVal;
            std::ostringstream xs, ys, zs, cs;
            xs << '['; ys << '['; zs << '['; cs << '[';
            bool first = true;
            if (axis == 'x') {
                // Plane x = Xs[fixedIdx]. Loop over (Y, Z) cells.
                const double xVal = Xs[fixedIdx];
                for (size_t i = 0; i + 1 < M; ++i) {
                    for (size_t k = 0; k + 1 < P; ++k) {
                        emitQuad(xs, ys, zs, cs, first,
                                 xVal, Ys[i],     Zs[k],     Vat(i,     fixedIdx, k),
                                 xVal, Ys[i + 1], Zs[k],     Vat(i + 1, fixedIdx, k),
                                 xVal, Ys[i + 1], Zs[k + 1], Vat(i + 1, fixedIdx, k + 1),
                                 xVal, Ys[i],     Zs[k + 1], Vat(i,     fixedIdx, k + 1));
                    }
                }
            } else if (axis == 'y') {
                const double yVal = Ys[fixedIdx];
                for (size_t j = 0; j + 1 < N; ++j) {
                    for (size_t k = 0; k + 1 < P; ++k) {
                        emitQuad(xs, ys, zs, cs, first,
                                 Xs[j],     yVal, Zs[k],     Vat(fixedIdx, j,     k),
                                 Xs[j + 1], yVal, Zs[k],     Vat(fixedIdx, j + 1, k),
                                 Xs[j + 1], yVal, Zs[k + 1], Vat(fixedIdx, j + 1, k + 1),
                                 Xs[j],     yVal, Zs[k + 1], Vat(fixedIdx, j,     k + 1));
                    }
                }
            } else {
                const double zVal = Zs[fixedIdx];
                for (size_t i = 0; i + 1 < M; ++i) {
                    for (size_t j = 0; j + 1 < N; ++j) {
                        emitQuad(xs, ys, zs, cs, first,
                                 Xs[j],     Ys[i],     zVal, Vat(i,     j,     fixedIdx),
                                 Xs[j + 1], Ys[i],     zVal, Vat(i,     j + 1, fixedIdx),
                                 Xs[j + 1], Ys[i + 1], zVal, Vat(i + 1, j + 1, fixedIdx),
                                 Xs[j],     Ys[i + 1], zVal, Vat(i + 1, j,     fixedIdx));
                    }
                }
            }
            xs << ']'; ys << ']'; zs << ']'; cs << ']';
            DatasetInfo ds;
            // Reuse the existing fill3 wire shape — adapter routes
            // type='fill3' to polygon3d mode in the 3-D renderer.
            ds.type = "fill3";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.zJson = zs.str();
            ds.vertexColorsJson = cs.str();
            ds.style = "color=#888888;fillOpacity=0.85";   // fallback
            fm.pushDataset(std::move(ds));
        };

        // Snap each requested slice value to the nearest grid index.
        const auto nearest = [](const std::vector<double> &v, double q) {
            size_t best = 0;
            double bestDist = std::abs(v[0] - q);
            for (size_t i = 1; i < v.size(); ++i) {
                const double d = std::abs(v[i] - q);
                if (d < bestDist) { bestDist = d; best = i; }
            }
            return best;
        };
        // Mean-value-per-slice — just average finite voxels in the
        // selected plane.
        const auto sliceMean = [&](char axis, size_t idx) {
            double sum = 0; size_t n = 0;
            if (axis == 'x') {
                for (size_t i = 0; i < M; ++i)
                    for (size_t k = 0; k < P; ++k) {
                        const double v = Vat(i, idx, k);
                        if (std::isfinite(v)) { sum += v; ++n; }
                    }
            } else if (axis == 'y') {
                for (size_t j = 0; j < N; ++j)
                    for (size_t k = 0; k < P; ++k) {
                        const double v = Vat(idx, j, k);
                        if (std::isfinite(v)) { sum += v; ++n; }
                    }
            } else {
                for (size_t i = 0; i < M; ++i)
                    for (size_t j = 0; j < N; ++j) {
                        const double v = Vat(i, j, idx);
                        if (std::isfinite(v)) { sum += v; ++n; }
                    }
            }
            return n > 0 ? sum / n : 0.0;
        };

        if (sx && sx->numel() > 0) {
            for (size_t s = 0; s < sx->numel(); ++s) {
                const size_t idx = nearest(Xs, sx->elemAsDouble(s));
                pushSlice('x', idx, sliceMean('x', idx));
            }
        }
        if (sy && sy->numel() > 0) {
            for (size_t s = 0; s < sy->numel(); ++s) {
                const size_t idx = nearest(Ys, sy->elemAsDouble(s));
                pushSlice('y', idx, sliceMean('y', idx));
            }
        }
        if (sz && sz->numel() > 0) {
            for (size_t s = 0; s < sz->numel(); ++s) {
                const size_t idx = nearest(Zs, sz->elemAsDouble(s));
                pushSlice('z', idx, sliceMean('z', idx));
            }
        }
        // No slices specified — degenerate: pick mid-plane on each axis.
        if ((!sx || sx->numel() == 0) && (!sy || sy->numel() == 0)
         && (!sz || sz->numel() == 0)) {
            pushSlice('x', N / 2, vmid);
            pushSlice('y', M / 2, vmid);
            pushSlice('z', P / 2, vmid);
        }
        fm.emitModified();
        outs[0] = Value::empty();
    };
    reg("surface", "slice", sliceImpl);

    // ────────────────────────────────────────────────────────────────
    // isosurface(V, isovalue) — marching cubes.
    //
    // Forms supported:
    //   isosurface(V, iso)
    //   isosurface(X, Y, Z, V, iso)
    //
    // The volume V is M×N×P (rows = Y, cols = X, pages = Z). For each
    // cube cell we compute an 8-bit code from "corner < iso", look up
    // the edge bitmask + triangle list from the standard Bourke
    // marching-cubes tables, and interpolate edge crossings linearly.
    // Output: a single fill3 dataset whose x/y/z arrays carry every
    // triangle's three vertices, separated by null markers between
    // triangles so the existing 3-D polygon3d renderer fans them
    // correctly. Per-vertex normals + per-vertex colour from V are a
    // BACKLOG item.
    // ────────────────────────────────────────────────────────────────

    // Standard Paul-Bourke marching-cubes tables (public domain, see
    // http://paulbourke.net/geometry/polygonise/). edgeTable[cubeIndex]
    // is a 12-bit mask of edges crossed by the surface; triTable[i][k]
    // gives -1-terminated runs of triangle vertex indices in [0, 11]
    // referring to the cube's edges.
    static const int kMcEdgeTable[256] = {
        0x0,0x109,0x203,0x30a,0x406,0x50f,0x605,0x70c,0x80c,0x905,0xa0f,0xb06,0xc0a,0xd03,0xe09,0xf00,
        0x190,0x99,0x393,0x29a,0x596,0x49f,0x795,0x69c,0x99c,0x895,0xb9f,0xa96,0xd9a,0xc93,0xf99,0xe90,
        0x230,0x339,0x33,0x13a,0x636,0x73f,0x435,0x53c,0xa3c,0xb35,0x83f,0x936,0xe3a,0xf33,0xc39,0xd30,
        0x3a0,0x2a9,0x1a3,0xaa,0x7a6,0x6af,0x5a5,0x4ac,0xbac,0xaa5,0x9af,0x8a6,0xfaa,0xea3,0xda9,0xca0,
        0x460,0x569,0x663,0x76a,0x66,0x16f,0x265,0x36c,0xc6c,0xd65,0xe6f,0xf66,0x86a,0x963,0xa69,0xb60,
        0x5f0,0x4f9,0x7f3,0x6fa,0x1f6,0xff,0x3f5,0x2fc,0xdfc,0xcf5,0xfff,0xef6,0x9fa,0x8f3,0xbf9,0xaf0,
        0x650,0x759,0x453,0x55a,0x256,0x35f,0x55,0x15c,0xe5c,0xf55,0xc5f,0xd56,0xa5a,0xb53,0x859,0x950,
        0x7c0,0x6c9,0x5c3,0x4ca,0x3c6,0x2cf,0x1c5,0xcc,0xfcc,0xec5,0xdcf,0xcc6,0xbca,0xac3,0x9c9,0x8c0,
        0x8c0,0x9c9,0xac3,0xbca,0xcc6,0xdcf,0xec5,0xfcc,0xcc,0x1c5,0x2cf,0x3c6,0x4ca,0x5c3,0x6c9,0x7c0,
        0x950,0x859,0xb53,0xa5a,0xd56,0xc5f,0xf55,0xe5c,0x15c,0x55,0x35f,0x256,0x55a,0x453,0x759,0x650,
        0xaf0,0xbf9,0x8f3,0x9fa,0xef6,0xfff,0xcf5,0xdfc,0x2fc,0x3f5,0xff,0x1f6,0x6fa,0x7f3,0x4f9,0x5f0,
        0xb60,0xa69,0x963,0x86a,0xf66,0xe6f,0xd65,0xc6c,0x36c,0x265,0x16f,0x66,0x76a,0x663,0x569,0x460,
        0xca0,0xda9,0xea3,0xfaa,0x8a6,0x9af,0xaa5,0xbac,0x4ac,0x5a5,0x6af,0x7a6,0xaa,0x1a3,0x2a9,0x3a0,
        0xd30,0xc39,0xf33,0xe3a,0x936,0x83f,0xb35,0xa3c,0x53c,0x435,0x73f,0x636,0x13a,0x33,0x339,0x230,
        0xe90,0xf99,0xc93,0xd9a,0xa96,0xb9f,0x895,0x99c,0x69c,0x795,0x49f,0x596,0x29a,0x393,0x99,0x190,
        0xf00,0xe09,0xd03,0xc0a,0xb06,0xa0f,0x905,0x80c,0x70c,0x605,0x50f,0x406,0x30a,0x203,0x109,0x0
    };

    static const int kMcTriTable[256][16] = {
        {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {0,8,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {0,1,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {1,8,3,9,8,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {1,2,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {0,8,3,1,2,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {9,2,10,0,2,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {2,8,3,2,10,8,10,9,8,-1,-1,-1,-1,-1,-1,-1},
        {3,11,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {0,11,2,8,11,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {1,9,0,2,3,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {1,11,2,1,9,11,9,8,11,-1,-1,-1,-1,-1,-1,-1},
        {3,10,1,11,10,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {0,10,1,0,8,10,8,11,10,-1,-1,-1,-1,-1,-1,-1},
        {3,9,0,3,11,9,11,10,9,-1,-1,-1,-1,-1,-1,-1},
        {9,8,10,10,8,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {4,7,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {4,3,0,7,3,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {0,1,9,8,4,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {4,1,9,4,7,1,7,3,1,-1,-1,-1,-1,-1,-1,-1},
        {1,2,10,8,4,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {3,4,7,3,0,4,1,2,10,-1,-1,-1,-1,-1,-1,-1},
        {9,2,10,9,0,2,8,4,7,-1,-1,-1,-1,-1,-1,-1},
        {2,10,9,2,9,7,2,7,3,7,9,4,-1,-1,-1,-1},
        {8,4,7,3,11,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {11,4,7,11,2,4,2,0,4,-1,-1,-1,-1,-1,-1,-1},
        {9,0,1,8,4,7,2,3,11,-1,-1,-1,-1,-1,-1,-1},
        {4,7,11,9,4,11,9,11,2,9,2,1,-1,-1,-1,-1},
        {3,10,1,3,11,10,7,8,4,-1,-1,-1,-1,-1,-1,-1},
        {1,11,10,1,4,11,1,0,4,7,11,4,-1,-1,-1,-1},
        {4,7,8,9,0,11,9,11,10,11,0,3,-1,-1,-1,-1},
        {4,7,11,4,11,9,9,11,10,-1,-1,-1,-1,-1,-1,-1},
        {9,5,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {9,5,4,0,8,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {0,5,4,1,5,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {8,5,4,8,3,5,3,1,5,-1,-1,-1,-1,-1,-1,-1},
        {1,2,10,9,5,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {3,0,8,1,2,10,4,9,5,-1,-1,-1,-1,-1,-1,-1},
        {5,2,10,5,4,2,4,0,2,-1,-1,-1,-1,-1,-1,-1},
        {2,10,5,3,2,5,3,5,4,3,4,8,-1,-1,-1,-1},
        {9,5,4,2,3,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {0,11,2,0,8,11,4,9,5,-1,-1,-1,-1,-1,-1,-1},
        {0,5,4,0,1,5,2,3,11,-1,-1,-1,-1,-1,-1,-1},
        {2,1,5,2,5,8,2,8,11,4,8,5,-1,-1,-1,-1},
        {10,3,11,10,1,3,9,5,4,-1,-1,-1,-1,-1,-1,-1},
        {4,9,5,0,8,1,8,10,1,8,11,10,-1,-1,-1,-1},
        {5,4,0,5,0,11,5,11,10,11,0,3,-1,-1,-1,-1},
        {5,4,8,5,8,10,10,8,11,-1,-1,-1,-1,-1,-1,-1},
        {9,7,8,5,7,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {9,3,0,9,5,3,5,7,3,-1,-1,-1,-1,-1,-1,-1},
        {0,7,8,0,1,7,1,5,7,-1,-1,-1,-1,-1,-1,-1},
        {1,5,3,3,5,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {9,7,8,9,5,7,10,1,2,-1,-1,-1,-1,-1,-1,-1},
        {10,1,2,9,5,0,5,3,0,5,7,3,-1,-1,-1,-1},
        {8,0,2,8,2,5,8,5,7,10,5,2,-1,-1,-1,-1},
        {2,10,5,2,5,3,3,5,7,-1,-1,-1,-1,-1,-1,-1},
        {7,9,5,7,8,9,3,11,2,-1,-1,-1,-1,-1,-1,-1},
        {9,5,7,9,7,2,9,2,0,2,7,11,-1,-1,-1,-1},
        {2,3,11,0,1,8,1,7,8,1,5,7,-1,-1,-1,-1},
        {11,2,1,11,1,7,7,1,5,-1,-1,-1,-1,-1,-1,-1},
        {9,5,8,8,5,7,10,1,3,10,3,11,-1,-1,-1,-1},
        {5,7,0,5,0,9,7,11,0,1,0,10,11,10,0,-1},
        {11,10,0,11,0,3,10,5,0,8,0,7,5,7,0,-1},
        {11,10,5,7,11,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {10,6,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {0,8,3,5,10,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {9,0,1,5,10,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {1,8,3,1,9,8,5,10,6,-1,-1,-1,-1,-1,-1,-1},
        {1,6,5,2,6,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {1,6,5,1,2,6,3,0,8,-1,-1,-1,-1,-1,-1,-1},
        {9,6,5,9,0,6,0,2,6,-1,-1,-1,-1,-1,-1,-1},
        {5,9,8,5,8,2,5,2,6,3,2,8,-1,-1,-1,-1},
        {2,3,11,10,6,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {11,0,8,11,2,0,10,6,5,-1,-1,-1,-1,-1,-1,-1},
        {0,1,9,2,3,11,5,10,6,-1,-1,-1,-1,-1,-1,-1},
        {5,10,6,1,9,2,9,11,2,9,8,11,-1,-1,-1,-1},
        {6,3,11,6,5,3,5,1,3,-1,-1,-1,-1,-1,-1,-1},
        {0,8,11,0,11,5,0,5,1,5,11,6,-1,-1,-1,-1},
        {3,11,6,0,3,6,0,6,5,0,5,9,-1,-1,-1,-1},
        {6,5,9,6,9,11,11,9,8,-1,-1,-1,-1,-1,-1,-1},
        {5,10,6,4,7,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {4,3,0,4,7,3,6,5,10,-1,-1,-1,-1,-1,-1,-1},
        {1,9,0,5,10,6,8,4,7,-1,-1,-1,-1,-1,-1,-1},
        {10,6,5,1,9,7,1,7,3,7,9,4,-1,-1,-1,-1},
        {6,1,2,6,5,1,4,7,8,-1,-1,-1,-1,-1,-1,-1},
        {1,2,5,5,2,6,3,0,4,3,4,7,-1,-1,-1,-1},
        {8,4,7,9,0,5,0,6,5,0,2,6,-1,-1,-1,-1},
        {7,3,9,7,9,4,3,2,9,5,9,6,2,6,9,-1},
        {3,11,2,7,8,4,10,6,5,-1,-1,-1,-1,-1,-1,-1},
        {5,10,6,4,7,2,4,2,0,2,7,11,-1,-1,-1,-1},
        {0,1,9,4,7,8,2,3,11,5,10,6,-1,-1,-1,-1},
        {9,2,1,9,11,2,9,4,11,7,11,4,5,10,6,-1},
        {8,4,7,3,11,5,3,5,1,5,11,6,-1,-1,-1,-1},
        {5,1,11,5,11,6,1,0,11,7,11,4,0,4,11,-1},
        {0,5,9,0,6,5,0,3,6,11,6,3,8,4,7,-1},
        {6,5,9,6,9,11,4,7,9,7,11,9,-1,-1,-1,-1},
        {10,4,9,6,4,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {4,10,6,4,9,10,0,8,3,-1,-1,-1,-1,-1,-1,-1},
        {10,0,1,10,6,0,6,4,0,-1,-1,-1,-1,-1,-1,-1},
        {8,3,1,8,1,6,8,6,4,6,1,10,-1,-1,-1,-1},
        {1,4,9,1,2,4,2,6,4,-1,-1,-1,-1,-1,-1,-1},
        {3,0,8,1,2,9,2,4,9,2,6,4,-1,-1,-1,-1},
        {0,2,4,4,2,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {8,3,2,8,2,4,4,2,6,-1,-1,-1,-1,-1,-1,-1},
        {10,4,9,10,6,4,11,2,3,-1,-1,-1,-1,-1,-1,-1},
        {0,8,2,2,8,11,4,9,10,4,10,6,-1,-1,-1,-1},
        {3,11,2,0,1,6,0,6,4,6,1,10,-1,-1,-1,-1},
        {6,4,1,6,1,10,4,8,1,2,1,11,8,11,1,-1},
        {9,6,4,9,3,6,9,1,3,11,6,3,-1,-1,-1,-1},
        {8,11,1,8,1,0,11,6,1,9,1,4,6,4,1,-1},
        {3,11,6,3,6,0,0,6,4,-1,-1,-1,-1,-1,-1,-1},
        {6,4,8,11,6,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {7,10,6,7,8,10,8,9,10,-1,-1,-1,-1,-1,-1,-1},
        {0,7,3,0,10,7,0,9,10,6,7,10,-1,-1,-1,-1},
        {10,6,7,1,10,7,1,7,8,1,8,0,-1,-1,-1,-1},
        {10,6,7,10,7,1,1,7,3,-1,-1,-1,-1,-1,-1,-1},
        {1,2,6,1,6,8,1,8,9,8,6,7,-1,-1,-1,-1},
        {2,6,9,2,9,1,6,7,9,0,9,3,7,3,9,-1},
        {7,8,0,7,0,6,6,0,2,-1,-1,-1,-1,-1,-1,-1},
        {7,3,2,6,7,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {2,3,11,10,6,8,10,8,9,8,6,7,-1,-1,-1,-1},
        {2,0,7,2,7,11,0,9,7,6,7,10,9,10,7,-1},
        {1,8,0,1,7,8,1,10,7,6,7,10,2,3,11,-1},
        {11,2,1,11,1,7,10,6,1,6,7,1,-1,-1,-1,-1},
        {8,9,6,8,6,7,9,1,6,11,6,3,1,3,6,-1},
        {0,9,1,11,6,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {7,8,0,7,0,6,3,11,0,11,6,0,-1,-1,-1,-1},
        {7,11,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {7,6,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {3,0,8,11,7,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {0,1,9,11,7,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {8,1,9,8,3,1,11,7,6,-1,-1,-1,-1,-1,-1,-1},
        {10,1,2,6,11,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {1,2,10,3,0,8,6,11,7,-1,-1,-1,-1,-1,-1,-1},
        {2,9,0,2,10,9,6,11,7,-1,-1,-1,-1,-1,-1,-1},
        {6,11,7,2,10,3,10,8,3,10,9,8,-1,-1,-1,-1},
        {7,2,3,6,2,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {7,0,8,7,6,0,6,2,0,-1,-1,-1,-1,-1,-1,-1},
        {2,7,6,2,3,7,0,1,9,-1,-1,-1,-1,-1,-1,-1},
        {1,6,2,1,8,6,1,9,8,8,7,6,-1,-1,-1,-1},
        {10,7,6,10,1,7,1,3,7,-1,-1,-1,-1,-1,-1,-1},
        {10,7,6,1,7,10,1,8,7,1,0,8,-1,-1,-1,-1},
        {0,3,7,0,7,10,0,10,9,6,10,7,-1,-1,-1,-1},
        {7,6,10,7,10,8,8,10,9,-1,-1,-1,-1,-1,-1,-1},
        {6,8,4,11,8,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {3,6,11,3,0,6,0,4,6,-1,-1,-1,-1,-1,-1,-1},
        {8,6,11,8,4,6,9,0,1,-1,-1,-1,-1,-1,-1,-1},
        {9,4,6,9,6,3,9,3,1,11,3,6,-1,-1,-1,-1},
        {6,8,4,6,11,8,2,10,1,-1,-1,-1,-1,-1,-1,-1},
        {1,2,10,3,0,11,0,6,11,0,4,6,-1,-1,-1,-1},
        {4,11,8,4,6,11,0,2,9,2,10,9,-1,-1,-1,-1},
        {10,9,3,10,3,2,9,4,3,11,3,6,4,6,3,-1},
        {8,2,3,8,4,2,4,6,2,-1,-1,-1,-1,-1,-1,-1},
        {0,4,2,4,6,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {1,9,0,2,3,4,2,4,6,4,3,8,-1,-1,-1,-1},
        {1,9,4,1,4,2,2,4,6,-1,-1,-1,-1,-1,-1,-1},
        {8,1,3,8,6,1,8,4,6,6,10,1,-1,-1,-1,-1},
        {10,1,0,10,0,6,6,0,4,-1,-1,-1,-1,-1,-1,-1},
        {4,6,3,4,3,8,6,10,3,0,3,9,10,9,3,-1},
        {10,9,4,6,10,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {4,9,5,7,6,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {0,8,3,4,9,5,11,7,6,-1,-1,-1,-1,-1,-1,-1},
        {5,0,1,5,4,0,7,6,11,-1,-1,-1,-1,-1,-1,-1},
        {11,7,6,8,3,4,3,5,4,3,1,5,-1,-1,-1,-1},
        {9,5,4,10,1,2,7,6,11,-1,-1,-1,-1,-1,-1,-1},
        {6,11,7,1,2,10,0,8,3,4,9,5,-1,-1,-1,-1},
        {7,6,11,5,4,10,4,2,10,4,0,2,-1,-1,-1,-1},
        {3,4,8,3,5,4,3,2,5,10,5,2,11,7,6,-1},
        {7,2,3,7,6,2,5,4,9,-1,-1,-1,-1,-1,-1,-1},
        {9,5,4,0,8,6,0,6,2,6,8,7,-1,-1,-1,-1},
        {3,6,2,3,7,6,1,5,0,5,4,0,-1,-1,-1,-1},
        {6,2,8,6,8,7,2,1,8,4,8,5,1,5,8,-1},
        {9,5,4,10,1,6,1,7,6,1,3,7,-1,-1,-1,-1},
        {1,6,10,1,7,6,1,0,7,8,7,0,9,5,4,-1},
        {4,0,10,4,10,5,0,3,10,6,10,7,3,7,10,-1},
        {7,6,10,7,10,8,5,4,10,4,8,10,-1,-1,-1,-1},
        {6,9,5,6,11,9,11,8,9,-1,-1,-1,-1,-1,-1,-1},
        {3,6,11,0,6,3,0,5,6,0,9,5,-1,-1,-1,-1},
        {0,11,8,0,5,11,0,1,5,5,6,11,-1,-1,-1,-1},
        {6,11,3,6,3,5,5,3,1,-1,-1,-1,-1,-1,-1,-1},
        {1,2,10,9,5,11,9,11,8,11,5,6,-1,-1,-1,-1},
        {0,11,3,0,6,11,0,9,6,5,6,9,1,2,10,-1},
        {11,8,5,11,5,6,8,0,5,10,5,2,0,2,5,-1},
        {6,11,3,6,3,5,2,10,3,10,5,3,-1,-1,-1,-1},
        {5,8,9,5,2,8,5,6,2,3,8,2,-1,-1,-1,-1},
        {9,5,6,9,6,0,0,6,2,-1,-1,-1,-1,-1,-1,-1},
        {1,5,8,1,8,0,5,6,8,3,8,2,6,2,8,-1},
        {1,5,6,2,1,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {1,3,6,1,6,10,3,8,6,5,6,9,8,9,6,-1},
        {10,1,0,10,0,6,9,5,0,5,6,0,-1,-1,-1,-1},
        {0,3,8,5,6,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {10,5,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {11,5,10,7,5,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {11,5,10,11,7,5,8,3,0,-1,-1,-1,-1,-1,-1,-1},
        {5,11,7,5,10,11,1,9,0,-1,-1,-1,-1,-1,-1,-1},
        {10,7,5,10,11,7,9,8,1,8,3,1,-1,-1,-1,-1},
        {11,1,2,11,7,1,7,5,1,-1,-1,-1,-1,-1,-1,-1},
        {0,8,3,1,2,7,1,7,5,7,2,11,-1,-1,-1,-1},
        {9,7,5,9,2,7,9,0,2,2,11,7,-1,-1,-1,-1},
        {7,5,2,7,2,11,5,9,2,3,2,8,9,8,2,-1},
        {2,5,10,2,3,5,3,7,5,-1,-1,-1,-1,-1,-1,-1},
        {8,2,0,8,5,2,8,7,5,10,2,5,-1,-1,-1,-1},
        {9,0,1,5,10,3,5,3,7,3,10,2,-1,-1,-1,-1},
        {9,8,2,9,2,1,8,7,2,10,2,5,7,5,2,-1},
        {1,3,5,3,7,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {0,8,7,0,7,1,1,7,5,-1,-1,-1,-1,-1,-1,-1},
        {9,0,3,9,3,5,5,3,7,-1,-1,-1,-1,-1,-1,-1},
        {9,8,7,5,9,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {5,8,4,5,10,8,10,11,8,-1,-1,-1,-1,-1,-1,-1},
        {5,0,4,5,11,0,5,10,11,11,3,0,-1,-1,-1,-1},
        {0,1,9,8,4,10,8,10,11,10,4,5,-1,-1,-1,-1},
        {10,11,4,10,4,5,11,3,4,9,4,1,3,1,4,-1},
        {2,5,1,2,8,5,2,11,8,4,5,8,-1,-1,-1,-1},
        {0,4,11,0,11,3,4,5,11,2,11,1,5,1,11,-1},
        {0,2,5,0,5,9,2,11,5,4,5,8,11,8,5,-1},
        {9,4,5,2,11,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {2,5,10,3,5,2,3,4,5,3,8,4,-1,-1,-1,-1},
        {5,10,2,5,2,4,4,2,0,-1,-1,-1,-1,-1,-1,-1},
        {3,10,2,3,5,10,3,8,5,4,5,8,0,1,9,-1},
        {5,10,2,5,2,4,1,9,2,9,4,2,-1,-1,-1,-1},
        {8,4,5,8,5,3,3,5,1,-1,-1,-1,-1,-1,-1,-1},
        {0,4,5,1,0,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {8,4,5,8,5,3,9,0,5,0,3,5,-1,-1,-1,-1},
        {9,4,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {4,11,7,4,9,11,9,10,11,-1,-1,-1,-1,-1,-1,-1},
        {0,8,3,4,9,7,9,11,7,9,10,11,-1,-1,-1,-1},
        {1,10,11,1,11,4,1,4,0,7,4,11,-1,-1,-1,-1},
        {3,1,4,3,4,8,1,10,4,7,4,11,10,11,4,-1},
        {4,11,7,9,11,4,9,2,11,9,1,2,-1,-1,-1,-1},
        {9,7,4,9,11,7,9,1,11,2,11,1,0,8,3,-1},
        {11,7,4,11,4,2,2,4,0,-1,-1,-1,-1,-1,-1,-1},
        {11,7,4,11,4,2,8,3,4,3,2,4,-1,-1,-1,-1},
        {2,9,10,2,7,9,2,3,7,7,4,9,-1,-1,-1,-1},
        {9,10,7,9,7,4,10,2,7,8,7,0,2,0,7,-1},
        {3,7,10,3,10,2,7,4,10,1,10,0,4,0,10,-1},
        {1,10,2,8,7,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {4,9,1,4,1,7,7,1,3,-1,-1,-1,-1,-1,-1,-1},
        {4,9,1,4,1,7,0,8,1,8,7,1,-1,-1,-1,-1},
        {4,0,3,7,4,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {4,8,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {9,10,8,10,11,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {3,0,9,3,9,11,11,9,10,-1,-1,-1,-1,-1,-1,-1},
        {0,1,10,0,10,8,8,10,11,-1,-1,-1,-1,-1,-1,-1},
        {3,1,10,11,3,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {1,2,11,1,11,9,9,11,8,-1,-1,-1,-1,-1,-1,-1},
        {3,0,9,3,9,11,1,2,9,2,11,9,-1,-1,-1,-1},
        {0,2,11,8,0,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {3,2,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {2,3,8,2,8,10,10,8,9,-1,-1,-1,-1,-1,-1,-1},
        {9,10,2,0,9,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {2,3,8,2,8,10,0,1,8,1,10,8,-1,-1,-1,-1},
        {1,10,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {1,3,8,9,1,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {0,9,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {0,3,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}
    };

    auto isosurfaceImpl = [](Span<const Value> args, size_t nargout,
                             Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();

        const Value *V = nullptr;
        const Value *X = nullptr;
        const Value *Y = nullptr;
        const Value *Z = nullptr;
        double iso = 0.0;
        bool isoSet = false;
        if (args.size() == 2) {
            V = &args[0]; iso = args[1].toScalar(); isoSet = true;
        } else if (args.size() >= 5) {
            X = &args[0]; Y = &args[1]; Z = &args[2]; V = &args[3];
            iso = args[4].toScalar(); isoSet = true;
        } else if (args.size() == 1) {
            V = &args[0];   // iso defaults to mean
        }
        if (!V || V->dims().ndims() != 3) {
            outs[0] = Value::empty();
            return;
        }

        const size_t M = V->dims().rows();
        const size_t N = V->dims().cols();
        const size_t P = V->dims().pages();
        if (M < 2 || N < 2 || P < 2) { outs[0] = Value::empty(); return; }

        std::vector<double> Xs(N), Ys(M), Zs(P);
        if (X && Y && Z && X->numel() >= N && Y->numel() >= M && Z->numel() >= P) {
            for (size_t i = 0; i < N; ++i) Xs[i] = X->doubleData()[i];
            for (size_t i = 0; i < M; ++i) Ys[i] = Y->doubleData()[i];
            for (size_t i = 0; i < P; ++i) Zs[i] = Z->doubleData()[i];
        } else {
            for (size_t i = 0; i < N; ++i) Xs[i] = (double)(i + 1);
            for (size_t i = 0; i < M; ++i) Ys[i] = (double)(i + 1);
            for (size_t i = 0; i < P; ++i) Zs[i] = (double)(i + 1);
        }

        const auto Vat = [&](size_t i, size_t j, size_t k) {
            return V->elemAsDouble(k * M * N + j * M + i);
        };
        if (!isoSet) {
            double sum = 0; size_t n = 0;
            for (size_t k = 0; k < P; ++k)
                for (size_t j = 0; j < N; ++j)
                    for (size_t i = 0; i < M; ++i) {
                        const double v = Vat(i, j, k);
                        if (std::isfinite(v)) { sum += v; ++n; }
                    }
            iso = n ? sum / n : 0.0;
        }

        // Cube corner offsets matching Bourke's convention. Corner k
        // ↔ (di, dj, dk) where di stripe X axis, dj stripe Y axis, dk
        // stripe Z axis (depth).
        static const int kCornerDX[8] = {0, 1, 1, 0, 0, 1, 1, 0};
        static const int kCornerDY[8] = {0, 0, 1, 1, 0, 0, 1, 1};
        static const int kCornerDZ[8] = {0, 0, 0, 0, 1, 1, 1, 1};
        // Edges connect pairs of corners. edgeCorners[edge] = (a, b).
        static const int kEdge0[12] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3};
        static const int kEdge1[12] = {1, 2, 3, 0, 5, 6, 7, 4, 4, 5, 6, 7};

        std::ostringstream xs, ys, zs;
        xs << '['; ys << '['; zs << '[';
        bool first = true;

        // Convert data-axis order to MATLAB convention: axis-0 stripes
        // X (j → cols), axis-1 stripes Y (i → rows), axis-2 stripes Z
        // (k → pages).
        for (size_t k = 0; k + 1 < P; ++k) {
            for (size_t j = 0; j + 1 < N; ++j) {
                for (size_t i = 0; i + 1 < M; ++i) {
                    double cv[8];
                    cv[0] = Vat(i,     j,     k    );
                    cv[1] = Vat(i,     j + 1, k    );
                    cv[2] = Vat(i + 1, j + 1, k    );
                    cv[3] = Vat(i + 1, j,     k    );
                    cv[4] = Vat(i,     j,     k + 1);
                    cv[5] = Vat(i,     j + 1, k + 1);
                    cv[6] = Vat(i + 1, j + 1, k + 1);
                    cv[7] = Vat(i + 1, j,     k + 1);
                    bool anyNan = false;
                    for (int c = 0; c < 8; ++c)
                        if (!std::isfinite(cv[c])) { anyNan = true; break; }
                    if (anyNan) continue;
                    int code = 0;
                    for (int c = 0; c < 8; ++c) if (cv[c] < iso) code |= (1 << c);
                    const int em = kMcEdgeTable[code];
                    if (em == 0) continue;
                    // Compute the world coords of the cube's corners.
                    auto cornerXYZ = [&](int c) {
                        const size_t ic = i + (size_t)kCornerDY[c];
                        const size_t jc = j + (size_t)kCornerDX[c];
                        const size_t kc = k + (size_t)kCornerDZ[c];
                        return std::array<double, 3>{Xs[jc], Ys[ic], Zs[kc]};
                    };
                    std::array<double, 3> verts[12] = {};
                    for (int e = 0; e < 12; ++e) {
                        if (!(em & (1 << e))) continue;
                        const int a = kEdge0[e], b = kEdge1[e];
                        const auto pA = cornerXYZ(a);
                        const auto pB = cornerXYZ(b);
                        const double va = cv[a], vb = cv[b];
                        const double t = (std::abs(vb - va) < 1e-15)
                            ? 0.5 : (iso - va) / (vb - va);
                        verts[e] = { pA[0] + t * (pB[0] - pA[0]),
                                     pA[1] + t * (pB[1] - pA[1]),
                                     pA[2] + t * (pB[2] - pA[2]) };
                    }
                    // Emit triangles per the lookup. The fill3 wire
                    // path expects polygons separated by null markers;
                    // a triangle is 3 vertices. We keep `first=false`
                    // after writing ",null" so the next triangle's
                    // first vertex picks up its leading comma in the
                    // "if(!first)" branch — yielding the correct
                    // "v,v,v,null,v,v,v,null,…" sequence.
                    const int *tri = kMcTriTable[code];
                    for (int t = 0; tri[t] != -1; t += 3) {
                        for (int v = 0; v < 3; ++v) {
                            if (!first) { xs << ','; ys << ','; zs << ','; }
                            first = false;
                            const auto &p = verts[tri[t + v]];
                            xs << p[0]; ys << p[1]; zs << p[2];
                        }
                        xs << ",null"; ys << ",null"; zs << ",null";
                    }
                }
            }
        }
        xs << ']'; ys << ']'; zs << ']';

        DatasetInfo ds;
        ds.type = "fill3";
        ds.xJson = xs.str();
        ds.yJson = ys.str();
        ds.zJson = zs.str();
        // Single representative colour from the iso level (relative
        // to the volume's own range).
        double vmn = std::numeric_limits<double>::infinity();
        double vmx = -std::numeric_limits<double>::infinity();
        const size_t Vn = M * N * P;
        for (size_t ii = 0; ii < Vn; ++ii) {
            const double v = V->elemAsDouble(ii);
            if (std::isfinite(v)) {
                if (v < vmn) vmn = v;
                if (v > vmx) vmx = v;
            }
        }
        if (!std::isfinite(vmn)) { vmn = 0; vmx = 1; }
        const double t = (vmx == vmn) ? 0.5 : (iso - vmn) / (vmx - vmn);
        const int rcomp = (int)std::clamp(t * 255.0,        0.0, 255.0);
        const int gcomp = (int)std::clamp(120.0,            0.0, 255.0);
        const int bcomp = (int)std::clamp((1 - t) * 255.0,  0.0, 255.0);
        char buf[64];
        std::snprintf(buf, sizeof buf,
                      "color=#%02x%02x%02x;fillOpacity=0.85;smoothNormals=1",
                      rcomp, gcomp, bcomp);
        ds.style = buf;
        fm.pushDataset(std::move(ds));
        fm.emitModified();
        outs[0] = Value::empty();
    };
    reg("surface", "isosurface", isosurfaceImpl);

    // ────────────────────────────────────────────────────────────────
    // coneplot — cone-headed arrows over a 3-D vector field.
    //
    // Forms supported:
    //   coneplot(U, V, W)
    //     — cones at integer grid (1..N, 1..M, 1..P) aligned with
    //       (U(i,j,k), V(i,j,k), W(i,j,k)).
    //   coneplot(X, Y, Z, U, V, W)
    //     — same but with explicit grid coords.
    //   coneplot(X, Y, Z, U, V, W, Cx, Cy, Cz [, S])
    //     — cones at user-specified positions with U/V/W taken as the
    //       value AT those positions (nearest-neighbour for the v1).
    //       S is a scalar magnitude factor.
    //
    // Each cone is a 6-sided pyramid (apex + 6-vertex base ring) →
    // 6 side triangles + 4 cap triangles = 10 triangles per cone,
    // emitted into a single fill3 dataset (null-separated triangles).
    // ────────────────────────────────────────────────────────────────
    auto coneplotImpl = [](Span<const Value> args, size_t nargout,
                           Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();

        const Value *X = nullptr, *Y = nullptr, *Z = nullptr;
        const Value *U = nullptr, *Vv = nullptr, *W = nullptr;
        const Value *Cx = nullptr, *Cy = nullptr, *Cz = nullptr;
        double S = 1.0;
        if (args.size() == 3) {
            U = &args[0]; Vv = &args[1]; W = &args[2];
        } else if (args.size() == 6) {
            X = &args[0]; Y = &args[1]; Z = &args[2];
            U = &args[3]; Vv = &args[4]; W = &args[5];
        } else if (args.size() >= 9) {
            X = &args[0]; Y = &args[1]; Z = &args[2];
            U = &args[3]; Vv = &args[4]; W = &args[5];
            Cx = &args[6]; Cy = &args[7]; Cz = &args[8];
            if (args.size() >= 10) S = args[9].toScalar();
        } else {
            outs[0] = Value::empty();
            return;
        }
        if (!U || !Vv || !W) { outs[0] = Value::empty(); return; }
        const auto &dims = U->dims();
        if (dims.ndims() != 3) { outs[0] = Value::empty(); return; }
        const size_t M = dims.rows();
        const size_t N = dims.cols();
        const size_t P = dims.pages();

        // Resolve grid coords. X/Y/Z come in as meshgrid 3-D outputs:
        //   X(i,j,k) = x(j), Y(i,j,k) = y(i), Z(i,j,k) = z(k)
        // Column-major linear index for (i,j,k) is k*M*N + j*M + i. So
        // unique x-values live at stride M (along columns), z-values at
        // stride M*N (along pages). Y already varies along i so the
        // first M elements are exactly y(1..M). The pre-fix code read
        // `X[i]` which gave x(1) for every i — all cones collapsed into
        // a single x/z slice and the field rendered as a flat row.
        std::vector<double> Xs(N), Ys(M), Zs(P);
        if (X && Y && Z && X->numel() >= M * N * P
            && Y->numel() >= M * N * P && Z->numel() >= M * N * P) {
            for (size_t j = 0; j < N; ++j) Xs[j] = X->doubleData()[j * M];
            for (size_t i = 0; i < M; ++i) Ys[i] = Y->doubleData()[i];
            for (size_t k = 0; k < P; ++k) Zs[k] = Z->doubleData()[k * M * N];
        } else if (X && Y && Z && X->numel() >= N && Y->numel() >= M && Z->numel() >= P) {
            // Fallback for the 1-D vector form: x, y, z passed as
            // straight vectors rather than meshgrid output.
            for (size_t i = 0; i < N; ++i) Xs[i] = X->doubleData()[i];
            for (size_t i = 0; i < M; ++i) Ys[i] = Y->doubleData()[i];
            for (size_t i = 0; i < P; ++i) Zs[i] = Z->doubleData()[i];
        } else {
            for (size_t i = 0; i < N; ++i) Xs[i] = (double)(i + 1);
            for (size_t i = 0; i < M; ++i) Ys[i] = (double)(i + 1);
            for (size_t i = 0; i < P; ++i) Zs[i] = (double)(i + 1);
        }

        // Build the cone-position list. Without Cx/Cy/Cz we place a
        // cone at every grid point (M*N*P cones — fine for small
        // demos; the BACKLOG carries skipping).
        struct ConeAt { double x, y, z, u, v, w; };
        std::vector<ConeAt> cones;
        const auto Uat = [&](size_t i, size_t j, size_t k) {
            return U->elemAsDouble(k * M * N + j * M + i);
        };
        const auto Vat = [&](size_t i, size_t j, size_t k) {
            return Vv->elemAsDouble(k * M * N + j * M + i);
        };
        const auto Wat = [&](size_t i, size_t j, size_t k) {
            return W->elemAsDouble(k * M * N + j * M + i);
        };
        if (Cx && Cy && Cz) {
            const size_t nc = std::min({Cx->numel(), Cy->numel(), Cz->numel()});
            for (size_t s = 0; s < nc; ++s) {
                const double cxv = Cx->elemAsDouble(s);
                const double cyv = Cy->elemAsDouble(s);
                const double czv = Cz->elemAsDouble(s);
                // Nearest-neighbour sample of (U, V, W) at (cxv, cyv, czv).
                const auto nearest = [](const std::vector<double> &v, double q) {
                    size_t best = 0;
                    double bestD = std::abs(v[0] - q);
                    for (size_t i = 1; i < v.size(); ++i) {
                        const double d = std::abs(v[i] - q);
                        if (d < bestD) { bestD = d; best = i; }
                    }
                    return best;
                };
                const size_t jj = nearest(Xs, cxv);
                const size_t ii = nearest(Ys, cyv);
                const size_t kk = nearest(Zs, czv);
                cones.push_back({ cxv, cyv, czv,
                                  Uat(ii, jj, kk),
                                  Vat(ii, jj, kk),
                                  Wat(ii, jj, kk) });
            }
        } else {
            cones.reserve(M * N * P);
            for (size_t k = 0; k < P; ++k)
                for (size_t j = 0; j < N; ++j)
                    for (size_t i = 0; i < M; ++i)
                        cones.push_back({ Xs[j], Ys[i], Zs[k],
                                          Uat(i, j, k),
                                          Vat(i, j, k),
                                          Wat(i, j, k) });
        }
        if (cones.empty()) { outs[0] = Value::empty(); return; }

        // Auto-scale: largest vector magnitude in the field divided
        // by 0.4 × the smallest grid spacing → cones don't overlap.
        double maxMag = 0;
        for (const auto &c : cones) {
            const double m = std::sqrt(c.u * c.u + c.v * c.v + c.w * c.w);
            if (m > maxMag) maxMag = m;
        }
        if (maxMag <= 0) { outs[0] = Value::empty(); return; }
        const double dx = (N > 1) ? (Xs[N - 1] - Xs[0]) / (N - 1) : 1.0;
        const double dy = (M > 1) ? (Ys[M - 1] - Ys[0]) / (M - 1) : 1.0;
        const double dz = (P > 1) ? (Zs[P - 1] - Zs[0]) / (P - 1) : 1.0;
        const double smallStep = std::min({std::abs(dx), std::abs(dy), std::abs(dz)});
        const double coneLen = (smallStep > 0 ? smallStep : 1.0) * 0.7 * S;
        const double coneRad = coneLen * 0.25;

        // Emit cones into a single fill3 dataset.
        std::ostringstream xs, ys, zs;
        xs << '['; ys << '['; zs << '[';
        bool first = true;
        constexpr int kRingN = 6;

        for (const auto &c : cones) {
            const double mag = std::sqrt(c.u * c.u + c.v * c.v + c.w * c.w);
            if (mag <= 0) continue;
            // Axis = normalized (u, v, w). Apex at base + axis * len.
            const double ax = c.u / mag, ay = c.v / mag, az = c.w / mag;
            const double scale = coneLen * (mag / maxMag);
            // Build orthonormal basis (e1, e2) ⊥ axis.
            double e1x, e1y, e1z;
            if (std::abs(ax) < 0.9) { e1x = 0; e1y = -az; e1z = ay; }
            else                    { e1x = -az; e1y = 0; e1z = ax; }
            // Normalize e1.
            const double e1n = std::sqrt(e1x*e1x + e1y*e1y + e1z*e1z);
            if (e1n > 0) { e1x /= e1n; e1y /= e1n; e1z /= e1n; }
            // e2 = axis × e1
            const double e2x = ay * e1z - az * e1y;
            const double e2y = az * e1x - ax * e1z;
            const double e2z = ax * e1y - ay * e1x;
            const double bx = c.x;
            const double by = c.y;
            const double bz = c.z;
            const double apexX = bx + ax * scale;
            const double apexY = by + ay * scale;
            const double apexZ = bz + az * scale;
            // Build the 6-vertex base ring.
            std::array<double, kRingN> rx, ry, rz;
            for (int n = 0; n < kRingN; ++n) {
                const double theta = (2 * M_PI) * n / kRingN;
                const double cs = std::cos(theta), sn = std::sin(theta);
                rx[n] = bx + coneRad * (e1x * cs + e2x * sn);
                ry[n] = by + coneRad * (e1y * cs + e2y * sn);
                rz[n] = bz + coneRad * (e1z * cs + e2z * sn);
            }
            // Helper — emit one triangle (3 verts) followed by null.
            const auto tri = [&](double X1, double Y1, double Z1,
                                 double X2, double Y2, double Z2,
                                 double X3, double Y3, double Z3) {
                if (!first) { xs << ','; ys << ','; zs << ','; }
                first = false;
                xs << X1 << ',' << X2 << ',' << X3;
                ys << Y1 << ',' << Y2 << ',' << Y3;
                zs << Z1 << ',' << Z2 << ',' << Z3;
                xs << ",null"; ys << ",null"; zs << ",null";
            };
            // Side triangles: apex - ring[n] - ring[(n+1)%N]
            for (int n = 0; n < kRingN; ++n) {
                const int n1 = (n + 1) % kRingN;
                tri(apexX, apexY, apexZ,
                    rx[n], ry[n], rz[n],
                    rx[n1], ry[n1], rz[n1]);
            }
            // Cap fan: ring[0] - ring[k] - ring[k+1] for k = 1..N-2
            for (int n = 1; n + 1 < kRingN; ++n) {
                tri(rx[0],     ry[0],     rz[0],
                    rx[n + 1], ry[n + 1], rz[n + 1],
                    rx[n],     ry[n],     rz[n]);
            }
        }
        xs << ']'; ys << ']'; zs << ']';

        DatasetInfo ds;
        ds.type = "fill3";
        ds.xJson = xs.str();
        ds.yJson = ys.str();
        ds.zJson = zs.str();
        ds.style = "color=#1f77b4;fillOpacity=0.9";
        fm.pushDataset(std::move(ds));
        fm.emitModified();
        outs[0] = Value::empty();
    };
    reg("surface", "coneplot", coneplotImpl);

    // ────────────────────────────────────────────────────────────────
    // streamtube — tubes wrapped around streamlines through a 3-D
    // vector field. Forms supported:
    //   streamtube(X, Y, Z, U, V, W, sx, sy, sz)
    //   streamtube(U, V, W, sx, sy, sz)         — implicit grid
    //
    // Per seed point, integrate the field with fixed-step Euler (RK1)
    // forward. Tube generated with an N-vertex ring around each
    // streamline sample; radii proportional to local |V| with a hard
    // cap of the grid spacing. Output: one fill3 dataset per
    // streamline, triangle list (quads split into two triangles).
    // ────────────────────────────────────────────────────────────────
    auto streamtubeImpl = [](Span<const Value> args, size_t nargout,
                             Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();

        const Value *X = nullptr, *Y = nullptr, *Z = nullptr;
        const Value *U = nullptr, *Vv = nullptr, *W = nullptr;
        const Value *sxV = nullptr, *syV = nullptr, *szV = nullptr;
        if (args.size() == 6) {
            U = &args[0]; Vv = &args[1]; W = &args[2];
            sxV = &args[3]; syV = &args[4]; szV = &args[5];
        } else if (args.size() >= 9) {
            X = &args[0]; Y = &args[1]; Z = &args[2];
            U = &args[3]; Vv = &args[4]; W = &args[5];
            sxV = &args[6]; syV = &args[7]; szV = &args[8];
        } else {
            outs[0] = Value::empty();
            return;
        }
        if (!U || U->dims().ndims() != 3) {
            outs[0] = Value::empty();
            return;
        }
        const auto &dims = U->dims();
        const size_t M = dims.rows();
        const size_t N = dims.cols();
        const size_t P = dims.pages();

        std::vector<double> Xs(N), Ys(M), Zs(P);
        if (X && Y && Z && X->numel() >= N && Y->numel() >= M && Z->numel() >= P) {
            for (size_t i = 0; i < N; ++i) Xs[i] = X->doubleData()[i];
            for (size_t i = 0; i < M; ++i) Ys[i] = Y->doubleData()[i];
            for (size_t i = 0; i < P; ++i) Zs[i] = Z->doubleData()[i];
        } else {
            for (size_t i = 0; i < N; ++i) Xs[i] = (double)(i + 1);
            for (size_t i = 0; i < M; ++i) Ys[i] = (double)(i + 1);
            for (size_t i = 0; i < P; ++i) Zs[i] = (double)(i + 1);
        }

        const auto Uat = [&](size_t i, size_t j, size_t k) {
            return U->elemAsDouble(k * M * N + j * M + i);
        };
        const auto Vat = [&](size_t i, size_t j, size_t k) {
            return Vv->elemAsDouble(k * M * N + j * M + i);
        };
        const auto Wat = [&](size_t i, size_t j, size_t k) {
            return W->elemAsDouble(k * M * N + j * M + i);
        };
        // Nearest-neighbour sampling at world position (x, y, z).
        const auto sampleAt = [&](double xq, double yq, double zq,
                                  double &uo, double &vo, double &wo) {
            const auto nearest = [](const std::vector<double> &v, double q) {
                size_t best = 0;
                double bestD = std::abs(v[0] - q);
                for (size_t i = 1; i < v.size(); ++i) {
                    const double d = std::abs(v[i] - q);
                    if (d < bestD) { bestD = d; best = i; }
                }
                return best;
            };
            const size_t jj = nearest(Xs, xq);
            const size_t ii = nearest(Ys, yq);
            const size_t kk = nearest(Zs, zq);
            uo = Uat(ii, jj, kk);
            vo = Vat(ii, jj, kk);
            wo = Wat(ii, jj, kk);
        };
        const auto inRange = [&](double xq, double yq, double zq) {
            const double xMn = std::min(Xs.front(), Xs.back());
            const double xMx = std::max(Xs.front(), Xs.back());
            const double yMn = std::min(Ys.front(), Ys.back());
            const double yMx = std::max(Ys.front(), Ys.back());
            const double zMn = std::min(Zs.front(), Zs.back());
            const double zMx = std::max(Zs.front(), Zs.back());
            return xq >= xMn && xq <= xMx
                && yq >= yMn && yq <= yMx
                && zq >= zMn && zq <= zMx;
        };

        const double dx = (N > 1) ? std::abs(Xs[N - 1] - Xs[0]) / (N - 1) : 1.0;
        const double dy = (M > 1) ? std::abs(Ys[M - 1] - Ys[0]) / (M - 1) : 1.0;
        const double dz = (P > 1) ? std::abs(Zs[P - 1] - Zs[0]) / (P - 1) : 1.0;
        const double smallStep = std::min({dx, dy, dz});
        const double tubeR = smallStep * 0.18;
        constexpr int kMaxSteps = 80;
        constexpr int kRingN    = 8;

        const size_t nSeeds = std::min({sxV->numel(), syV->numel(), szV->numel()});
        for (size_t s = 0; s < nSeeds; ++s) {
            std::vector<std::array<double, 3>> path;
            double xq = sxV->elemAsDouble(s);
            double yq = syV->elemAsDouble(s);
            double zq = szV->elemAsDouble(s);
            for (int step = 0; step < kMaxSteps; ++step) {
                if (!inRange(xq, yq, zq)) break;
                path.push_back({xq, yq, zq});
                double u, v, w;
                sampleAt(xq, yq, zq, u, v, w);
                const double mag = std::sqrt(u * u + v * v + w * w);
                if (mag < 1e-12) break;
                const double h = smallStep / mag * 0.5;   // half-cell stride
                xq += u * h;
                yq += v * h;
                zq += w * h;
            }
            if (path.size() < 2) continue;

            // Per-segment Frenet-ish frame: tangent = path[i+1] - path[i],
            // normalised; pick a stable e1 ⊥ tangent, e2 = tangent × e1.
            std::ostringstream xs, ys, zs;
            xs << '['; ys << '['; zs << '[';
            bool first = true;
            // Pre-compute ring vertices per path point.
            std::vector<std::array<std::array<double, 3>, kRingN>> rings(path.size());
            std::array<double, 3> prevE1 = {0, 0, 1};
            for (size_t i = 0; i < path.size(); ++i) {
                std::array<double, 3> tan;
                if (i + 1 < path.size()) {
                    tan = { path[i + 1][0] - path[i][0],
                            path[i + 1][1] - path[i][1],
                            path[i + 1][2] - path[i][2] };
                } else {
                    tan = { path[i][0] - path[i - 1][0],
                            path[i][1] - path[i - 1][1],
                            path[i][2] - path[i - 1][2] };
                }
                const double tn = std::sqrt(tan[0] * tan[0] + tan[1] * tan[1] + tan[2] * tan[2]);
                if (tn > 0) { tan[0] /= tn; tan[1] /= tn; tan[2] /= tn; }
                // Project prevE1 onto plane ⊥ tan and renormalise.
                const double dot = tan[0] * prevE1[0] + tan[1] * prevE1[1] + tan[2] * prevE1[2];
                std::array<double, 3> e1 = {
                    prevE1[0] - dot * tan[0],
                    prevE1[1] - dot * tan[1],
                    prevE1[2] - dot * tan[2],
                };
                double e1n = std::sqrt(e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2]);
                if (e1n < 1e-9) {
                    // prev was nearly parallel — pick a new e1.
                    if (std::abs(tan[0]) < 0.9) e1 = {0, -tan[2], tan[1]};
                    else                        e1 = {-tan[2], 0, tan[0]};
                    e1n = std::sqrt(e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2]);
                }
                if (e1n > 0) { e1[0] /= e1n; e1[1] /= e1n; e1[2] /= e1n; }
                prevE1 = e1;
                std::array<double, 3> e2 = {
                    tan[1] * e1[2] - tan[2] * e1[1],
                    tan[2] * e1[0] - tan[0] * e1[2],
                    tan[0] * e1[1] - tan[1] * e1[0],
                };
                for (int n = 0; n < kRingN; ++n) {
                    const double theta = (2 * M_PI) * n / kRingN;
                    const double cs = std::cos(theta), sn = std::sin(theta);
                    rings[i][n] = {
                        path[i][0] + tubeR * (e1[0] * cs + e2[0] * sn),
                        path[i][1] + tubeR * (e1[1] * cs + e2[1] * sn),
                        path[i][2] + tubeR * (e1[2] * cs + e2[2] * sn),
                    };
                }
            }
            // Connect consecutive rings with quads (split into 2 tris).
            const auto tri = [&](const std::array<double, 3> &A,
                                 const std::array<double, 3> &B,
                                 const std::array<double, 3> &C) {
                if (!first) { xs << ','; ys << ','; zs << ','; }
                first = false;
                xs << A[0] << ',' << B[0] << ',' << C[0];
                ys << A[1] << ',' << B[1] << ',' << C[1];
                zs << A[2] << ',' << B[2] << ',' << C[2];
                xs << ",null"; ys << ",null"; zs << ",null";
            };
            for (size_t i = 0; i + 1 < rings.size(); ++i) {
                for (int n = 0; n < kRingN; ++n) {
                    const int n1 = (n + 1) % kRingN;
                    const auto &A = rings[i][n];
                    const auto &B = rings[i + 1][n];
                    const auto &C = rings[i + 1][n1];
                    const auto &D = rings[i][n1];
                    tri(A, B, C);
                    tri(A, C, D);
                }
            }
            xs << ']'; ys << ']'; zs << ']';

            DatasetInfo ds;
            ds.type = "fill3";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.zJson = zs.str();
            ds.style = "color=#9467bd;fillOpacity=0.85";
            fm.pushDataset(std::move(ds));
        }
        fm.emitModified();
        outs[0] = Value::empty();
    };
    reg("surface", "streamtube", streamtubeImpl);

    // quiver3(x, y, z, u, v, w[, scale]) — 3-D vector field. Each (x,
    // y, z, u, v, w) row becomes one arrow from (x, y, z) to
    // (x + s·u, y + s·v, z + s·w). Default scale = 1.
    reg("line", "quiver3",
        [vecToJson](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.size() < 6) { outs[0] = Value::empty(); return; }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "quiver3";
            ds.xJson = vecToJson(args[0]);
            ds.yJson = vecToJson(args[1]);
            ds.zJson = vecToJson(args[2]);
            ds.uJson = vecToJson(args[3]);
            ds.vJson = vecToJson(args[4]);
            // w as a separate JSON field — adapter knows about uJson +
            // vJson (2-D quiver) so we tuck w into style as scale-style
            // extras for now: encode as `w=[...]`. Cleaner would be a
            // dedicated wJson field on DatasetInfo; deferred.
            std::string wjson = vecToJson(args[5]);
            std::ostringstream sty;
            sty << "wJson=" << wjson;
            if (args.size() >= 7 && args[6].numel() == 1)
                sty << ";scale=" << args[6].toScalar();
            ds.style = sty.str();
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // contour3(Z[, n|levels]) — contour lines on the surface defined
    // by Z. Same algorithm as 2-D contour but each segment carries the
    // Z value of the level so it can be drawn at the surface height.
    // Wire format: type='contour3' with X, Y vectors, Z-matrix, plus
    // a separate `levels` style key.
    reg("surface", "contour3",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.empty()) { outs[0] = Value::empty(); return; }
            const Value *Z_arg = nullptr;
            const Value *levels_arg = nullptr;
            if (args.size() == 1) Z_arg = &args[0];
            else if (args.size() >= 2) {
                Z_arg = &args[0];
                levels_arg = &args[1];
            }
            if (!Z_arg) { outs[0] = Value::empty(); return; }
            const size_t R = Z_arg->dims().rows();
            const size_t C = Z_arg->dims().cols();
            if (R < 2 || C < 2) { outs[0] = Value::empty(); return; }

            std::ostringstream xs, ys, zs;
            xs << '[';
            for (size_t c = 0; c < C; ++c) { if (c) xs << ','; xs << (c + 1); }
            xs << ']';
            ys << '[';
            for (size_t r = 0; r < R; ++r) { if (r) ys << ','; ys << (r + 1); }
            ys << ']';
            zs << '[';
            for (size_t r = 0; r < R; ++r) {
                if (r) zs << ',';
                zs << '[';
                for (size_t c = 0; c < C; ++c) {
                    if (c) zs << ',';
                    const double v = Z_arg->doubleData()[c * R + r];
                    if (std::isfinite(v)) zs << v;
                    else                  zs << "null";
                }
                zs << ']';
            }
            zs << ']';

            std::ostringstream sty;
            if (levels_arg) {
                if (levels_arg->numel() == 1) {
                    sty << "n=" << (int)levels_arg->toScalar();
                } else {
                    sty << "levels=[";
                    const double *p = levels_arg->doubleData();
                    for (size_t i = 0; i < levels_arg->numel(); ++i) {
                        if (i) sty << ',';
                        sty << p[i];
                    }
                    sty << "]";
                }
            }

            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "contour3";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.zJson = zs.str();
            ds.style = sty.str();
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // surfc(Z) / meshc(Z) — surf/mesh + contour3 in a single figure.
    // Implemented as wrappers that invoke compat.surf (or compat.mesh)
    // followed by compat.contour3. hold on between calls so both
    // datasets land on the same axes.
    auto surfMeshContour = [](const char *base,
                              Span<const Value> args, size_t nargout,
                              Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        const ExternalFunc *cf = ctx.engine->findExternal(base, ctx.env);
        if (!cf) { outs[0] = Value::empty(); return; }
        std::array<Value, 1> tmp;
        (*cf)(args, 0, Span<Value>(tmp.data(), 1), ctx);
        // Hold on so contour3 doesn't clear the axes.
        ctx.engine->figureManager().currentAxes().holdOn = true;
        const ExternalFunc *cc = ctx.engine->findExternal("contour3", ctx.env);
        if (cc) (*cc)(args, 0, Span<Value>(tmp.data(), 1), ctx);
        ctx.engine->figureManager().currentAxes().holdOn = false;
        outs[0] = Value::empty();
    };
    reg("surface", "surfc", [surfMeshContour](Span<const Value> a, size_t n, Span<Value> o, CallContext &c) {
        surfMeshContour("surf", a, n, o, c);
    });
    reg("surface", "meshc", [surfMeshContour](Span<const Value> a, size_t n, Span<Value> o, CallContext &c) {
        surfMeshContour("mesh", a, n, o, c);
    });

    // ── patch / fill — generic filled polygon ────────────────────────
    // Calling forms:
    //   patch(X, Y)             — single polygon, default fill
    //   patch(X, Y, C)          — C is a colour spec ('r', '#ff8800',
    //                              [r g b] triplet 0..1)
    //   patch(X, Y, C, 'EdgeColor', '...')   — N-V pairs (subset)
    // X and Y may be column-vectors (one polygon) or matrices (one
    // polygon per column). For matrix inputs we serialise polygons
    // separated by null markers in the wire JSON; the renderer breaks
    // sub-paths on null and closes each.
    auto patchImpl = [](Span<const Value> args, size_t nargout,
                        Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        if (args.size() < 2) { outs[0] = Value::empty(); return; }
        const auto &X = args[0];
        const auto &Y = args[1];
        const size_t R = X.dims().rows();
        const size_t C = std::max<size_t>(1, X.dims().cols());
        if (R == 0) { outs[0] = Value::empty(); return; }
        if (Y.dims().rows() != R || std::max<size_t>(1, Y.dims().cols()) != C) {
            outs[0] = Value::empty();
            return;
        }

        std::ostringstream xs, ys;
        xs << '['; ys << '[';
        for (size_t c = 0; c < C; ++c) {
            if (c > 0) { xs << ",null,"; ys << ",null,"; }
            for (size_t r = 0; r < R; ++r) {
                if (r > 0) { xs << ','; ys << ','; }
                xs << X.doubleData()[c * R + r];
                ys << Y.doubleData()[c * R + r];
            }
        }
        xs << ']'; ys << ']';

        // Color: 3rd arg accepts a single-char colour, "#rrggbb",
        // or a 3-element [r g b] vector with values in [0, 1].
        std::string color = "#1f77b4";
        if (args.size() >= 3) {
            const auto &Carg = args[2];
            if (Carg.isChar()) {
                std::string s = Carg.toString();
                if (!s.empty()) {
                    static const std::map<char, const char*> kShort = {
                        {'r', "#d62728"}, {'g', "#2ca02c"}, {'b', "#1f77b4"},
                        {'y', "#ffd700"}, {'m', "#bf40bf"}, {'c', "#17becf"},
                        {'k', "#000000"}, {'w', "#ffffff"},
                    };
                    if (s.size() == 1) {
                        auto it = kShort.find(s[0]);
                        if (it != kShort.end()) color = it->second;
                    } else if (s.size() >= 4 && s[0] == '#') {
                        color = s;
                    }
                }
            } else if (Carg.numel() >= 3) {
                const int r = (int)std::round(255 * std::clamp(Carg.doubleData()[0], 0.0, 1.0));
                const int g = (int)std::round(255 * std::clamp(Carg.doubleData()[1], 0.0, 1.0));
                const int b = (int)std::round(255 * std::clamp(Carg.doubleData()[2], 0.0, 1.0));
                char buf[16];
                std::snprintf(buf, sizeof buf, "#%02x%02x%02x", r, g, b);
                color = buf;
            }
        }

        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();
        DatasetInfo ds;
        ds.type = "polygon";
        ds.xJson = xs.str();
        ds.yJson = ys.str();
        std::ostringstream sty;
        sty << "color=" << color << ";fillOpacity=0.7";
        ds.style = sty.str();
        fm.pushDataset(std::move(ds));
        fm.emitModified();
        outs[0] = Value::empty();
    };
    reg("bar", "patch", patchImpl);
    reg("bar", "fill",  patchImpl);

    // pie(X) — circular slices proportional to X. Each wedge is its
    // own polygon dataset so it picks up a distinct palette colour.
    // Calling forms (subset of MATLAB's):
    //   pie(X)             — slice for every X(i) > 0
    //   pie(X, explode)    — explode(i) ≠ 0 displaces that wedge
    //                        radially by ~10% of the unit radius
    auto pieImpl = [](Span<const Value> args, size_t nargout, Span<Value> outs,
                      CallContext &ctx, bool tilt3d) {
        (void)nargout;
        if (args.empty() || args[0].numel() == 0) {
            outs[0] = Value::empty();
            return;
        }
        const auto &X = args[0];
        const size_t N = X.numel();
        double total = 0;
        for (size_t i = 0; i < N; ++i) {
            const double v = X.doubleData()[i];
            if (std::isfinite(v) && v > 0) total += v;
        }
        if (total <= 0) { outs[0] = Value::empty(); return; }

        const Value *expl = (args.size() >= 2) ? &args[1] : nullptr;
        static const char *kPalette[] = {
            "#1f77b4", "#ff7f0e", "#2ca02c", "#d62728",
            "#9467bd", "#8c564b", "#e377c2", "#7f7f7f",
            "#bcbd22", "#17becf",
        };

        const double TAU = 2 * 3.14159265358979323846;
        const int arcSamples = 24;          // vertices per wedge arc
        const double tiltY = tilt3d ? 0.4 : 1.0;   // pie3 squashes Y

        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();
        // Force equal axis so the pie stays circular.
        fm.currentAxes().axisMode = "equal";

        double angle = 0;
        for (size_t i = 0; i < N; ++i) {
            const double v = X.doubleData()[i];
            if (!std::isfinite(v) || v <= 0) continue;
            const double dθ = v / total * TAU;
            const double θ0 = angle;
            const double θ1 = angle + dθ;
            angle = θ1;

            // Explode: shift centre along the wedge bisector.
            double cx = 0, cy = 0;
            if (expl && expl->numel() > i && expl->doubleData()[i] != 0) {
                const double mid = (θ0 + θ1) / 2;
                const double off = 0.1;
                cx = off * std::cos(mid);
                cy = off * std::sin(mid) * tiltY;
            }

            std::ostringstream xs, ys;
            xs << '['; ys << '[';
            xs << cx; ys << cy;
            for (int s = 0; s <= arcSamples; ++s) {
                const double t = θ0 + (θ1 - θ0) * s / arcSamples;
                xs << ',' << (cx + std::cos(t));
                ys << ',' << (cy + std::sin(t) * tiltY);
            }
            xs << ']'; ys << ']';

            DatasetInfo ds;
            ds.type = "polygon";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            std::ostringstream sty;
            sty << "color=" << kPalette[i % 10] << ";fillOpacity=0.85";
            ds.style = sty.str();
            // Auto-label "p%" so the legend (if user calls legend())
            // shows percentages. MATLAB uses cell labels — we ship the
            // numeric form by default.
            char buf[16];
            std::snprintf(buf, sizeof buf, "%.0f%%", v / total * 100.0);
            ds.label = buf;
            fm.pushDataset(std::move(ds));
        }
        fm.emitModified();
        outs[0] = Value::empty();
    };
    reg("bar", "pie",  [pieImpl](Span<const Value> a, size_t n, Span<Value> o, CallContext &c) {
        pieImpl(a, n, o, c, false);
    });
    reg("bar", "pie3", [pieImpl](Span<const Value> a, size_t n, Span<Value> o, CallContext &c) {
        pieImpl(a, n, o, c, true);
    });

    // boxplot(X) / boxchart(X) — Tukey box-and-whisker plot.
    // X as vector → one box at x=1.
    // X as matrix → one box per column at x = 1..C.
    // Per box we emit:
    //   • polygon for the IQR rectangle (Q1..Q3)
    //   • line for the median (horizontal across the box)
    //   • line dataset for the two whisker stems + the two caps
    //   • scatter dataset for outliers (beyond ±1.5·IQR)
    auto boxImpl = [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        if (args.empty() || args[0].numel() == 0) {
            outs[0] = Value::empty();
            return;
        }
        const auto &M = args[0];
        const size_t R = std::max<size_t>(1, M.dims().rows());
        const size_t C = std::max<size_t>(1, M.dims().cols());
        // Treat 1×N or N×1 as a single column; matrix → C columns.
        const bool single = (R == 1 || C == 1);
        const size_t nBoxes = single ? 1 : C;
        const size_t nPerBox = single ? M.numel() : R;

        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();

        const double w = 0.4;     // half-width of each box (so total is 0.8)
        for (size_t bi = 0; bi < nBoxes; ++bi) {
            std::vector<double> col;
            col.reserve(nPerBox);
            for (size_t r = 0; r < nPerBox; ++r) {
                const double v = single
                    ? M.doubleData()[r]
                    : M.doubleData()[bi * R + r];
                if (std::isfinite(v)) col.push_back(v);
            }
            if (col.size() < 2) continue;
            std::sort(col.begin(), col.end());
            const double q1 = col[(size_t)((col.size() - 1) * 0.25)];
            const double q2 = col[(size_t)((col.size() - 1) * 0.50)];
            const double q3 = col[(size_t)((col.size() - 1) * 0.75)];
            const double iqr = q3 - q1;
            const double loFence = q1 - 1.5 * iqr;
            const double hiFence = q3 + 1.5 * iqr;
            double whLo = col.front(), whHi = col.back();
            // Whiskers extend to the most extreme finite point WITHIN
            // the fence, not the fence itself.
            for (double v : col) {
                if (v >= loFence && v < whLo) whLo = v;
                if (v <= hiFence && v > whHi) whHi = v;
            }
            // The simple computation above seeds whLo/whHi with the
            // overall extremes; tighten if those are outside the fence.
            if (whLo < loFence) whLo = loFence;
            if (whHi > hiFence) whHi = hiFence;
            // Re-clamp to the closest in-fence point (more reliable
            // when there are no in-fence extremes).
            for (double v : col) {
                if (v >= loFence && v <= q1 && v < whLo) whLo = v;
                if (v <= hiFence && v >= q3 && v > whHi) whHi = v;
            }

            const double cx = (double)(bi + 1);
            const double xL = cx - w, xR = cx + w;

            // 1. IQR box (polygon).
            std::ostringstream bx, by;
            bx << '[' << xL << ',' << xR << ',' << xR << ',' << xL << ']';
            by << '[' << q1 << ',' << q1 << ',' << q3 << ',' << q3 << ']';
            DatasetInfo dsBox;
            dsBox.type = "polygon";
            dsBox.xJson = bx.str();
            dsBox.yJson = by.str();
            dsBox.style = "color=#1f77b4;fillOpacity=0.35";
            fm.pushDataset(std::move(dsBox));

            // 2. Median line (horizontal across the box).
            std::ostringstream mx, my;
            mx << '[' << xL << ',' << xR << ']';
            my << '[' << q2 << ',' << q2 << ']';
            DatasetInfo dsMed;
            dsMed.type = "line";
            dsMed.xJson = mx.str();
            dsMed.yJson = my.str();
            dsMed.style = "color=#d62728";
            dsMed.lineWidth = 2;
            fm.pushDataset(std::move(dsMed));

            // 3. Whisker stems + caps as one line dataset with null
            //    separators between segments.
            std::ostringstream wx, wy;
            wx << '['; wy << '[';
            // Lower stem: (cx, whLo) → (cx, q1)
            wx << cx << ',' << cx;
            wy << whLo << ',' << q1;
            // Lower cap
            wx << ",null," << (cx - w/2) << ',' << (cx + w/2);
            wy << ",null," << whLo << ',' << whLo;
            // Upper stem
            wx << ",null," << cx << ',' << cx;
            wy << ",null," << q3 << ',' << whHi;
            // Upper cap
            wx << ",null," << (cx - w/2) << ',' << (cx + w/2);
            wy << ",null," << whHi << ',' << whHi;
            wx << ']'; wy << ']';
            DatasetInfo dsW;
            dsW.type = "line";
            dsW.xJson = wx.str();
            dsW.yJson = wy.str();
            dsW.style = "color=#1f77b4";
            fm.pushDataset(std::move(dsW));

            // 4. Outliers — scatter at (cx, v) for v outside fences.
            std::ostringstream ox, oy;
            ox << '['; oy << '[';
            bool first = true;
            for (double v : col) {
                if (v >= loFence && v <= hiFence) continue;
                if (!first) { ox << ','; oy << ','; }
                first = false;
                ox << cx;
                oy << v;
            }
            ox << ']'; oy << ']';
            if (!first) {
                DatasetInfo dsO;
                dsO.type = "scatter";
                dsO.xJson = ox.str();
                dsO.yJson = oy.str();
                dsO.style = "color=#d62728";
                dsO.markerSize = 3;
                fm.pushDataset(std::move(dsO));
            }
        }
        fm.emitModified();
        outs[0] = Value::empty();
    };
    reg("bar", "boxplot",  boxImpl);
    reg("bar", "boxchart", boxImpl);

    // violinplot(X) — Gaussian-KDE shape + slim box + median dot.
    // Layout mirrors boxplot: vector → one violin at x=1, matrix
    // → one violin per column at x = 1..C. KDE bandwidth uses
    // Silverman's rule (1.06 · σ · N^(-1/5)).
    reg("bar", "violinplot",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) {
                outs[0] = Value::empty();
                return;
            }
            const auto &M = args[0];
            const size_t R = std::max<size_t>(1, M.dims().rows());
            const size_t Cmat = std::max<size_t>(1, M.dims().cols());
            const bool single = (R == 1 || Cmat == 1);
            const size_t nViolins = single ? 1 : Cmat;
            const size_t nPerColumn = single ? M.numel() : R;

            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();

            const int kdePts = 50;
            for (size_t bi = 0; bi < nViolins; ++bi) {
                std::vector<double> col;
                col.reserve(nPerColumn);
                for (size_t r = 0; r < nPerColumn; ++r) {
                    const double v = single
                        ? M.doubleData()[r]
                        : M.doubleData()[bi * R + r];
                    if (std::isfinite(v)) col.push_back(v);
                }
                if (col.size() < 2) continue;
                double mean = 0;
                for (double v : col) mean += v;
                mean /= col.size();
                double var = 0;
                for (double v : col) var += (v - mean) * (v - mean);
                var /= col.size() - 1;
                const double sd = std::sqrt(var);
                if (sd <= 0) continue;
                const double bw = 1.06 * sd * std::pow((double)col.size(), -0.2);
                if (bw <= 0) continue;

                std::sort(col.begin(), col.end());
                const double mn = col.front(), mx = col.back();
                const double q1 = col[(size_t)((col.size() - 1) * 0.25)];
                const double q2 = col[(size_t)((col.size() - 1) * 0.50)];
                const double q3 = col[(size_t)((col.size() - 1) * 0.75)];

                // Sample KDE density on a uniform y grid [mn, mx].
                std::vector<double> ys(kdePts), den(kdePts);
                double maxD = 0;
                for (int g = 0; g < kdePts; ++g) {
                    const double y = mn + (mx - mn) * g / (double)(kdePts - 1);
                    double s = 0;
                    for (double v : col) {
                        const double t = (y - v) / bw;
                        s += std::exp(-0.5 * t * t);
                    }
                    s /= (col.size() * bw * std::sqrt(2 * 3.14159265358979323846));
                    ys[g] = y;
                    den[g] = s;
                    if (s > maxD) maxD = s;
                }
                if (maxD <= 0) continue;
                const double cx = (double)(bi + 1);
                const double halfW = 0.4;
                const double scale = halfW / maxD;

                // Violin polygon: right side bottom-to-top + left side
                // top-to-bottom. Closed by the polygon renderer.
                std::ostringstream xs, vy;
                xs << '['; vy << '[';
                bool first = true;
                for (int g = 0; g < kdePts; ++g) {
                    if (!first) { xs << ','; vy << ','; }
                    first = false;
                    xs << (cx + den[g] * scale);
                    vy << ys[g];
                }
                for (int g = kdePts - 1; g >= 0; --g) {
                    xs << ',' << (cx - den[g] * scale);
                    vy << ',' << ys[g];
                }
                xs << ']'; vy << ']';
                DatasetInfo dsV;
                dsV.type = "polygon";
                dsV.xJson = xs.str();
                dsV.yJson = vy.str();
                dsV.style = "color=#9467bd;fillOpacity=0.45";
                fm.pushDataset(std::move(dsV));

                // Slim box (Q1..Q3) at the centre.
                const double bxW = 0.06;
                std::ostringstream bxs, bys;
                bxs << '[' << (cx - bxW) << ',' << (cx + bxW) << ','
                    << (cx + bxW) << ',' << (cx - bxW) << ']';
                bys << '[' << q1 << ',' << q1 << ',' << q3 << ',' << q3 << ']';
                DatasetInfo dsBx;
                dsBx.type = "polygon";
                dsBx.xJson = bxs.str();
                dsBx.yJson = bys.str();
                dsBx.style = "color=#222;fillOpacity=0.85";
                fm.pushDataset(std::move(dsBx));

                // Median dot.
                std::ostringstream mx2, my2;
                mx2 << '[' << cx << ']';
                my2 << '[' << q2 << ']';
                DatasetInfo dsM;
                dsM.type = "scatter";
                dsM.xJson = mx2.str();
                dsM.yJson = my2.str();
                dsM.style = "color=#ffffff";
                dsM.markerSize = 4;
                fm.pushDataset(std::move(dsM));
            }
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // bar3(Z) — 3-D bars. Emits raw 3-D coords so the WebGL renderer
    // can build cuboids with real depth + lighting; no more cabinet
    // pre-projection. Wire format: type='bar3' with the Z-matrix as
    // nested rows (same shape as surf), plus implicit (x, y) coords =
    // (1..C, 1..R).
    reg("bar", "bar3",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.empty()) { outs[0] = Value::empty(); return; }
            const auto &Z = args[0];
            const size_t R = Z.dims().rows();
            const size_t C = Z.dims().cols();
            if (R == 0 || C == 0) { outs[0] = Value::empty(); return; }

            std::ostringstream xs, ys, zs;
            xs << '[';
            for (size_t c = 0; c < C; ++c) { if (c) xs << ','; xs << (c + 1); }
            xs << ']';
            ys << '[';
            for (size_t r = 0; r < R; ++r) { if (r) ys << ','; ys << (r + 1); }
            ys << ']';
            zs << '[';
            for (size_t r = 0; r < R; ++r) {
                if (r) zs << ',';
                zs << '[';
                for (size_t c = 0; c < C; ++c) {
                    if (c) zs << ',';
                    const double v = Z.doubleData()[c * R + r];
                    if (std::isfinite(v)) zs << v;
                    else                  zs << "null";
                }
                zs << ']';
            }
            zs << ']';

            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "bar3";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.zJson = zs.str();
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // waterfall(Z) — row-by-row 3-D ribbons. Emits the same Z-matrix
    // wire format as surf/bar3; the WebGL renderer builds per-row
    // ribbons (row Z values down to baseline z=0).
    reg("surface", "waterfall",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.empty()) { outs[0] = Value::empty(); return; }
            const auto &Z = args[0];
            const size_t R = Z.dims().rows();
            const size_t C = Z.dims().cols();
            if (R < 1 || C < 2) { outs[0] = Value::empty(); return; }

            std::ostringstream xs, ys, zs;
            xs << '[';
            for (size_t c = 0; c < C; ++c) { if (c) xs << ','; xs << (c + 1); }
            xs << ']';
            ys << '[';
            for (size_t r = 0; r < R; ++r) { if (r) ys << ','; ys << (r + 1); }
            ys << ']';
            zs << '[';
            for (size_t r = 0; r < R; ++r) {
                if (r) zs << ',';
                zs << '[';
                for (size_t c = 0; c < C; ++c) {
                    if (c) zs << ',';
                    const double v = Z.doubleData()[c * R + r];
                    if (std::isfinite(v)) zs << v;
                    else                  zs << "null";
                }
                zs << ']';
            }
            zs << ']';

            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "waterfall";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.zJson = zs.str();
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // fill3(X, Y, Z[, C]) — emits raw 3-D vertex arrays. Each column
    // of (X, Y, Z) is one polygon (one polygon for vector inputs).
    // The WebGL renderer builds a triangle fan per polygon under
    // perspective.
    reg("bar", "fill3",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.size() < 3) { outs[0] = Value::empty(); return; }
            const auto &X = args[0], &Y = args[1], &Z = args[2];
            const size_t R = X.dims().rows();
            const size_t C = std::max<size_t>(1, X.dims().cols());
            if (R == 0) { outs[0] = Value::empty(); return; }
            if (Y.dims().rows() != R || Z.dims().rows() != R) {
                outs[0] = Value::empty(); return;
            }

            // Wire format: x/y/z each as an array with `null`
            // separators between polygon groups (matches the existing
            // 2-D polygon convention).
            std::ostringstream xs, ys, zs;
            xs << '['; ys << '['; zs << '[';
            for (size_t c = 0; c < C; ++c) {
                if (c > 0) { xs << ",null,"; ys << ",null,"; zs << ",null,"; }
                for (size_t r = 0; r < R; ++r) {
                    if (r > 0) { xs << ','; ys << ','; zs << ','; }
                    const double xv = X.doubleData()[c * R + r];
                    const double yv = Y.doubleData()[c * R + r];
                    const double zv = Z.doubleData()[c * R + r];
                    if (std::isfinite(xv)) xs << xv; else xs << "null";
                    if (std::isfinite(yv)) ys << yv; else ys << "null";
                    if (std::isfinite(zv)) zs << zv; else zs << "null";
                }
            }
            xs << ']'; ys << ']'; zs << ']';

            std::string color = "#9467bd";
            if (args.size() >= 4 && args[3].isChar()) {
                std::string s = args[3].toString();
                if (s.size() >= 4 && s[0] == '#') color = s;
            }

            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "fill3";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.zJson = zs.str();
            std::ostringstream sty;
            sty << "color=" << color << ";fillOpacity=0.7";
            ds.style = sty.str();
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // ── Function-based plots (fplot / fcontour / fsurf / fmesh) ─────
    // Eval the user's function-handle on a sampled grid in C++ and
    // route the resulting matrices through the existing 2-D / 3-D
    // pipelines. Engine::callFunctionHandle works on both backends
    // (TW + VM) so the same body services either scripts.
    auto fplotImpl = [](Span<const Value> args, size_t nargout,
                        Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        if (args.empty() || !args[0].isFuncHandle()) {
            outs[0] = Value::empty();
            return;
        }
        const Value &fh = args[0];
        double a = -5, b = 5;
        if (args.size() >= 2 && args[1].numel() >= 2) {
            a = args[1].doubleData()[0];
            b = args[1].doubleData()[1];
        }
        const int N = 200;
        auto *mr = ctx.engine->resource();

        std::ostringstream xs, ys;
        xs << '['; ys << '[';
        for (int i = 0; i < N; ++i) {
            const double x = a + (b - a) * i / (double)(N - 1);
            Value xv = Value::scalar(x, mr);
            std::array<Value, 1> argv{ xv };
            Value res;
            try {
                res = ctx.engine->callFunctionHandle(fh,
                    Span<const Value>(argv.data(), 1));
            } catch (...) {
                continue;       // f(x) blew up at this sample; skip
            }
            const double y = std::isfinite(res.toScalar()) ? res.toScalar()
                                                           : std::nan("");
            if (i) { xs << ','; ys << ','; }
            xs << x;
            if (std::isfinite(y)) ys << y;
            else                   ys << "null";
        }
        xs << ']'; ys << ']';

        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();
        DatasetInfo ds;
        ds.type = "line";
        ds.xJson = xs.str();
        ds.yJson = ys.str();
        ds.style = "color=#1f77b4";
        fm.pushDataset(std::move(ds));
        fm.emitModified();
        outs[0] = Value::empty();
    };
    reg("line", "fplot", fplotImpl);

    // fcontour / fsurf / fmesh — sample f(x, y) on a grid then route
    // through the existing contour / surf paths. We stitch a Z matrix
    // value via Value::matrix and proxy the call.
    auto sampleGrid = [](const Value &fh, double xa, double xb,
                          double ya, double yb, int N,
                          CallContext &ctx) {
        auto *mr = ctx.engine->resource();
        Value Z = Value::matrix((size_t)N, (size_t)N, ValueType::DOUBLE, mr);
        for (int i = 0; i < N; ++i) {
            const double y = ya + (yb - ya) * i / (double)(N - 1);
            for (int j = 0; j < N; ++j) {
                const double x = xa + (xb - xa) * j / (double)(N - 1);
                Value xv = Value::scalar(x, mr);
                Value yv = Value::scalar(y, mr);
                std::array<Value, 2> argv{ xv, yv };
                double r = std::nan("");
                try {
                    Value res = ctx.engine->callFunctionHandle(fh,
                        Span<const Value>(argv.data(), 2));
                    r = res.toScalar();
                } catch (...) { /* leave as nan */ }
                Z.doubleDataMut()[(size_t)j * (size_t)N + (size_t)i] = r;
            }
        }
        return Z;
    };

    reg("line", "fcontour",
        [sampleGrid](Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.empty() || !args[0].isFuncHandle()) {
                outs[0] = Value::empty();
                return;
            }
            double xa = -5, xb = 5, ya = -5, yb = 5;
            if (args.size() >= 2 && args[1].numel() >= 4) {
                xa = args[1].doubleData()[0];
                xb = args[1].doubleData()[1];
                ya = args[1].doubleData()[2];
                yb = args[1].doubleData()[3];
            } else if (args.size() >= 2 && args[1].numel() >= 2) {
                xa = args[1].doubleData()[0];
                xb = args[1].doubleData()[1];
                ya = xa; yb = xb;
            }
            const int N = 30;
            auto *mr = ctx.engine->resource();
            Value Z = sampleGrid(args[0], xa, xb, ya, yb, N, ctx);
            Value Xv = Value::matrix(1, (size_t)N, ValueType::DOUBLE, mr);
            Value Yv = Value::matrix(1, (size_t)N, ValueType::DOUBLE, mr);
            for (int j = 0; j < N; ++j)
                Xv.doubleDataMut()[j] = xa + (xb - xa) * j / (double)(N - 1);
            for (int i = 0; i < N; ++i)
                Yv.doubleDataMut()[i] = ya + (yb - ya) * i / (double)(N - 1);
            // Forward to the engine-registered `contour` builtin so the
            // marching-squares body is reused as-is.
            std::array<Value, 3> proxied{ Xv, Yv, Z };
            std::array<Value, 1> outBuf;
            const ExternalFunc *cf = ctx.engine->findExternal("contour", ctx.env);
            if (!cf) { outs[0] = Value::empty(); return; }
            (*cf)(Span<const Value>(proxied.data(), 3), 0,
                  Span<Value>(outBuf.data(), 1), ctx);
            outs[0] = Value::empty();
        });

    auto fSurfMeshImpl = [sampleGrid](Span<const Value> args, size_t nargout,
                                      Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        if (args.empty() || !args[0].isFuncHandle()) {
            outs[0] = Value::empty();
            return;
        }
        double xa = -5, xb = 5, ya = -5, yb = 5;
        if (args.size() >= 2 && args[1].numel() >= 4) {
            xa = args[1].doubleData()[0];
            xb = args[1].doubleData()[1];
            ya = args[1].doubleData()[2];
            yb = args[1].doubleData()[3];
        } else if (args.size() >= 2 && args[1].numel() >= 2) {
            xa = args[1].doubleData()[0];
            xb = args[1].doubleData()[1];
            ya = xa; yb = xb;
        }
        const int N = 30;
        auto *mr = ctx.engine->resource();
        Value Z = sampleGrid(args[0], xa, xb, ya, yb, N, ctx);
        Value Xv = Value::matrix(1, (size_t)N, ValueType::DOUBLE, mr);
        Value Yv = Value::matrix(1, (size_t)N, ValueType::DOUBLE, mr);
        for (int j = 0; j < N; ++j)
            Xv.doubleDataMut()[j] = xa + (xb - xa) * j / (double)(N - 1);
        for (int i = 0; i < N; ++i)
            Yv.doubleDataMut()[i] = ya + (yb - ya) * i / (double)(N - 1);
        std::array<Value, 3> proxied{ Xv, Yv, Z };
        std::array<Value, 1> outBuf;
        const ExternalFunc *cf = ctx.engine->findExternal("surf", ctx.env);
        if (!cf) { outs[0] = Value::empty(); return; }
        (*cf)(Span<const Value>(proxied.data(), 3), 0,
              Span<Value>(outBuf.data(), 1), ctx);
        outs[0] = Value::empty();
    };
    reg("line", "fsurf", fSurfMeshImpl);
    reg("line", "fmesh", fSurfMeshImpl);

    // ── Streamlines — RK4 integration over a 2-D vector field ────────
    //   streamline(X, Y, U, V, sx, sy)    — explicit seed points
    //   streamslice(X, Y, U, V)           — auto 5×5 seed grid
    // Uniform grid is assumed (linspace-style X / Y). Bilinear interp
    // for (U, V) at integration points; integration stops when the
    // particle leaves the grid, hits a NaN cell, or stalls (|F| < eps).
    auto streamImpl = [](bool autoSlice,
                         Span<const Value> args, size_t nargout,
                         Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        if (args.size() < 4) { outs[0] = Value::empty(); return; }
        const auto &Xv = args[0];
        const auto &Yv = args[1];
        const auto &Uv = args[2];
        const auto &Vv = args[3];
        const size_t Cx = Xv.numel();
        const size_t Ry = Yv.numel();
        if (Cx < 2 || Ry < 2) { outs[0] = Value::empty(); return; }
        if (Uv.dims().rows() != Ry || Uv.dims().cols() != Cx) {
            outs[0] = Value::empty(); return;
        }
        if (Vv.dims().rows() != Ry || Vv.dims().cols() != Cx) {
            outs[0] = Value::empty(); return;
        }

        std::vector<double> Xs(Cx), Ys(Ry);
        for (size_t c = 0; c < Cx; ++c) Xs[c] = Xv.doubleData()[c];
        for (size_t r = 0; r < Ry; ++r) Ys[r] = Yv.doubleData()[r];
        const double xMin = Xs.front(), xMax = Xs.back();
        const double yMin = Ys.front(), yMax = Ys.back();
        const double dxAvg = (xMax - xMin) / (double)(Cx - 1);
        const double dyAvg = (yMax - yMin) / (double)(Ry - 1);

        const auto Uat = [&](size_t r, size_t c) {
            return Uv.doubleData()[c * Ry + r];
        };
        const auto Vat = [&](size_t r, size_t c) {
            return Vv.doubleData()[c * Ry + r];
        };

        // Bilinear interp at (x, y). Returns false if out-of-bounds or
        // a corner is non-finite.
        const auto sampleField = [&](double x, double y, double &u, double &v) {
            if (x < xMin || x > xMax || y < yMin || y > yMax) return false;
            // Locate cell index by binary search on Xs / Ys (uniform-
            // friendly but works for any monotonically-increasing grid).
            size_t c0 = 0;
            while (c0 + 1 < Cx && Xs[c0 + 1] <= x) ++c0;
            if (c0 + 1 >= Cx) c0 = Cx - 2;
            size_t r0 = 0;
            while (r0 + 1 < Ry && Ys[r0 + 1] <= y) ++r0;
            if (r0 + 1 >= Ry) r0 = Ry - 2;
            const double xL = Xs[c0], xR = Xs[c0 + 1];
            const double yT = Ys[r0], yB = Ys[r0 + 1];
            const double tx = (xR > xL) ? (x - xL) / (xR - xL) : 0.0;
            const double ty = (yB > yT) ? (y - yT) / (yB - yT) : 0.0;
            const double u00 = Uat(r0,     c0);
            const double u01 = Uat(r0,     c0 + 1);
            const double u10 = Uat(r0 + 1, c0);
            const double u11 = Uat(r0 + 1, c0 + 1);
            const double v00 = Vat(r0,     c0);
            const double v01 = Vat(r0,     c0 + 1);
            const double v10 = Vat(r0 + 1, c0);
            const double v11 = Vat(r0 + 1, c0 + 1);
            if (!std::isfinite(u00) || !std::isfinite(u01)
             || !std::isfinite(u10) || !std::isfinite(u11)
             || !std::isfinite(v00) || !std::isfinite(v01)
             || !std::isfinite(v10) || !std::isfinite(v11)) return false;
            const double u0 = u00 * (1 - tx) + u01 * tx;
            const double u1 = u10 * (1 - tx) + u11 * tx;
            u = u0 * (1 - ty) + u1 * ty;
            const double v0 = v00 * (1 - tx) + v01 * tx;
            const double v1 = v10 * (1 - tx) + v11 * tx;
            v = v0 * (1 - ty) + v1 * ty;
            return true;
        };

        // Build the seed list.
        std::vector<std::pair<double, double>> seeds;
        if (autoSlice) {
            // 5×5 evenly spaced inside the grid (1 cell margin so the
            // first integration step has data on every side).
            const int nS = 5;
            for (int j = 0; j < nS; ++j) {
                for (int i = 0; i < nS; ++i) {
                    const double x = xMin + (xMax - xMin) * (i + 0.5) / nS;
                    const double y = yMin + (yMax - yMin) * (j + 0.5) / nS;
                    seeds.emplace_back(x, y);
                }
            }
        } else {
            if (args.size() < 6) { outs[0] = Value::empty(); return; }
            const auto &SX = args[4];
            const auto &SY = args[5];
            const size_t N = std::min(SX.numel(), SY.numel());
            for (size_t k = 0; k < N; ++k)
                seeds.emplace_back(SX.doubleData()[k], SY.doubleData()[k]);
        }

        // RK4 trace from a single seed in `dir` ∈ {+1, -1}. Appends
        // points to xs/ys (with a leading null when we already have
        // points from a prior trace).
        const double h = std::min(dxAvg, dyAvg) * 0.4;
        const int maxSteps = 800;
        const double eps = 1e-9;

        const auto traceFrom = [&](double x0, double y0, int dir,
                                   std::ostringstream &xs,
                                   std::ostringstream &ys, size_t &count) {
            double x = x0, y = y0;
            for (int step = 0; step < maxSteps; ++step) {
                double u, v;
                if (!sampleField(x, y, u, v)) break;
                const double mag = std::hypot(u, v);
                if (!std::isfinite(mag) || mag < eps) break;
                if (count > 0) { xs << ','; ys << ','; }
                xs << x; ys << y; ++count;

                // RK4
                double k1u, k1v;
                if (!sampleField(x, y, k1u, k1v)) break;
                double k2u, k2v;
                if (!sampleField(x + dir * h * k1u / 2,
                                 y + dir * h * k1v / 2, k2u, k2v)) break;
                double k3u, k3v;
                if (!sampleField(x + dir * h * k2u / 2,
                                 y + dir * h * k2v / 2, k3u, k3v)) break;
                double k4u, k4v;
                if (!sampleField(x + dir * h * k3u,
                                 y + dir * h * k3v, k4u, k4v)) break;
                x += dir * h * (k1u + 2 * k2u + 2 * k3u + k4u) / 6;
                y += dir * h * (k1v + 2 * k2v + 2 * k3v + k4v) / 6;
            }
        };

        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();

        std::ostringstream xs, ys;
        xs << '['; ys << '[';
        size_t count = 0;
        for (auto [sx0, sy0] : seeds) {
            // Trace forward then backward; insert a null break before
            // each new seed *and* between forward / backward halves so
            // they render as separate strokes.
            std::ostringstream fwdX, fwdY, bwdX, bwdY;
            size_t fwdN = 0, bwdN = 0;
            traceFrom(sx0, sy0,  1, fwdX, fwdY, fwdN);
            traceFrom(sx0, sy0, -1, bwdX, bwdY, bwdN);
            if (fwdN == 0 && bwdN == 0) continue;
            if (count > 0) { xs << ",null,"; ys << ",null,"; }
            // Backward trace runs sx0→edge — emit reversed so the line
            // flows naturally seedToEdge twice. We just dump bwd points
            // as captured (in time-order from seed); a break separates
            // it from the forward half. Renderer doesn't care about
            // direction — it just connects consecutive non-NaN points.
            if (bwdN > 0) {
                xs << bwdX.str(); ys << bwdY.str();
                count += bwdN;
                if (fwdN > 0) { xs << ",null,"; ys << ",null,"; }
            }
            if (fwdN > 0) {
                xs << fwdX.str(); ys << fwdY.str();
                count += fwdN;
            }
        }
        xs << ']'; ys << ']';

        DatasetInfo ds;
        ds.type = "line";
        ds.xJson = xs.str();
        ds.yJson = ys.str();
        ds.style = "color=#2078b4";
        fm.pushDataset(std::move(ds));
        fm.emitModified();
        outs[0] = Value::empty();
    };
    reg("line", "streamline",
        [streamImpl](Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx) {
            streamImpl(false, args, nargout, outs, ctx);
        });
    reg("line", "streamslice",
        [streamImpl](Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx) {
            streamImpl(true, args, nargout, outs, ctx);
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
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // polarscatter(theta, rho) — markers at each (θ, ρ) on the polar
    // axes. Same wire format as polarplot but type='scatter' so the
    // PolarPlot renderer draws circles instead of polylines.
    reg("polar", "polarscatter",
        [vecToJson, parsePlotArgs](Span<const Value> args, size_t nargout,
                                   Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2) { outs[0] = Value::empty(); return; }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            fm.currentAxes().polar = true;
            DatasetInfo ds;
            ds.type = "scatter";
            ds.xJson = vecToJson(args[0]);
            ds.yJson = vecToJson(args[1]);
            size_t nvStart = 2;
            if (args.size() >= 3 && args[2].isChar()) {
                ds.style = args[2].toString();
                nvStart = 3;
            }
            parsePlotArgs(args, nvStart, ds);
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // polarhistogram(theta[, nbins]) — bins θ values into nbins angular
    // sectors over [0, 2π) and emits a polar bar dataset where each
    // bin centre carries its count. PolarPlot renders the bars as
    // wedges from the origin.
    reg("polar", "polarhistogram",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) { outs[0] = Value::empty(); return; }
            const auto &theta = args[0];
            const size_t N = theta.numel();
            int nbins = (args.size() >= 2 && args[1].numel() == 1)
                        ? std::max(1, (int)args[1].toScalar())
                        : 36;   // 10° bins by default
            const double TAU = 2 * 3.14159265358979323846;
            const double bw = TAU / nbins;

            std::vector<double> counts(nbins, 0.0);
            for (size_t i = 0; i < N; ++i) {
                double t = theta.doubleData()[i];
                if (!std::isfinite(t)) continue;
                // Wrap into [0, 2π).
                t = std::fmod(t, TAU);
                if (t < 0) t += TAU;
                int b = (int)(t / bw);
                if (b >= nbins) b = nbins - 1;
                if (b < 0) b = 0;
                counts[b] += 1;
            }
            std::ostringstream tx, ty;
            tx << '['; ty << '[';
            for (int i = 0; i < nbins; ++i) {
                if (i) { tx << ','; ty << ','; }
                tx << (bw * (i + 0.5));    // bin centre
                ty << counts[i];
            }
            tx << ']'; ty << ']';

            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            fm.currentAxes().polar = true;
            DatasetInfo ds;
            ds.type = "bar";              // routed to wedge renderer
            ds.xJson = tx.str();
            ds.yJson = ty.str();
            fm.pushDataset(std::move(ds));
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
            fm.pushDataset(std::move(ds));
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
            fm.pushDataset(std::move(ds));
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
            fm.pushDataset(std::move(ds));
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
                fm.pushDataset(std::move(ds));
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
    reg("layout", "subtitle",
        [argStr](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty()) {
                auto &fm = ctx.engine->figureManager();
                fm.currentAxes().subtitle = argStr(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });
    // sgtitle — figure-level "super title". numkit doesn't have a
    // dedicated figure title slot, so v1 routes to the first axes'
    // title field. Visually equivalent for single-cell figures;
    // subplot grid figures get the title only on cell 1 — full
    // figure-level rendering is BACKLOG.
    reg("layout", "sgtitle",
        [argStr](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty()) {
                auto &fm = ctx.engine->figureManager();
                if (!fm.current().axes.empty())
                    fm.current().axes[0].title = argStr(args[0]);
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
            fm.pushDataset(std::move(ds));
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });

    reg("layout", "ylabel",
        [argStr](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (!args.empty()) {
                auto &fm = ctx.engine->figureManager();
                auto &ax = fm.currentAxes();
                // yyaxis: route to the active side's label slot.
                if (ax.yyEnabled && ax.activeYside == "right")
                    ax.ylabel2 = argStr(args[0]);
                else
                    ax.ylabel = argStr(args[0]);
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
                auto &ax = fm.currentAxes();
                if (ax.yyEnabled && ax.activeYside == "right")
                    ax.ylim2Json = vecToJson(args[0]);
                else
                    ax.ylimJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
        });

    reg("layout", "grid",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto &fm = ctx.engine->figureManager();
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
            //
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
                    outs[0] = Value::empty();
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
            if (args.empty()) { outs[0] = Value::empty(); return; }
            auto &fm = ctx.engine->figureManager();
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
                    outs[0] = Value::empty();
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
            [field, fnName](Span<const Value> args, size_t nargout, Span<Value> outs,
                    CallContext &ctx) {
                auto &fm = ctx.engine->figureManager();
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
                outs[0] = Value::empty();
            });
    };
    regAxisScale("xscale", &AxesState::xscale);
    regAxisScale("yscale", &AxesState::yscale);

    // yyaxis(side) — switch the active Y-side. MATLAB: yyaxis left|right.
    // First call enables the dual-axis state and stays enabled for the
    // life of the current axes; toggling only changes which side new
    // plots / ylim / ylabel / yscale calls write to.
    reg("layout", "yyaxis",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto &fm = ctx.engine->figureManager();
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

    // linkaxes — connect all subplot cells in the current figure for
    // synchronised pan/zoom. MATLAB:
    //   linkaxes(h)            — link on x and y (alias for 'xy')
    //   linkaxes(h, 'x'|'y'|'xy'|'off')
    // numkit doesn't model graphics handles, so we ignore the first
    // argument and link every subplot cell in the current figure
    // unconditionally. The mode is stored on the FigureState and
    // applied by the SubplotGrid renderer.
    reg("layout", "linkaxes",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto &fm = ctx.engine->figureManager();
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
            outs[0] = Value::empty();
        });

    // rotate3d / pan3d / zoom3d (on|off) — toggle the OrbitControls
    // axis-of-interaction for the 3-D renderer. MATLAB also accepts
    // these as no-arg toggles but the e2e parity tests pass strings.
    auto interactionToggle = [](std::string AxesState::*field,
                                Span<const Value> args, Span<Value> outs,
                                CallContext &ctx) {
        std::string mode = "on";
        if (!args.empty() && args[0].isChar()) {
            std::string s = args[0].toString();
            for (auto &c : s) c = (char)std::tolower((unsigned char)c);
            if (s == "on" || s == "off") mode = s;
        }
        ctx.engine->figureManager().currentAxes().*field = mode;
        ctx.engine->figureManager().current().modified = true;
        ctx.engine->figureManager().emitModified();
        outs[0] = Value::empty();
    };
    reg("layout", "rotate3d",
        [interactionToggle](Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
            interactionToggle(&AxesState::rotate3dMode, a, o, c);
        });
    reg("layout", "pan3d",
        [interactionToggle](Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
            interactionToggle(&AxesState::pan3dMode, a, o, c);
        });
    reg("layout", "zoom3d",
        [interactionToggle](Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
            interactionToggle(&AxesState::zoom3dMode, a, o, c);
        });

    // xticks / yticks / zticks — set custom tick positions. Accept a
    // numeric vector OR the string "auto" to clear. With no args,
    // the v1 returns an empty value (true MATLAB returns the
    // auto-generated tick set; needs renderer-side query plumbing).
    auto ticksReg = [](std::string AxesState::*field) {
        return [field](Span<const Value> args, size_t nargout,
                       Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            auto &fm = ctx.engine->figureManager();
            auto &ax = fm.currentAxes();
            if (args.empty()) {
                outs[0] = Value::empty();
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
                outs[0] = Value::empty();
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
            outs[0] = Value::empty();
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
                       Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            auto &fm = ctx.engine->figureManager();
            auto &ax = fm.currentAxes();
            if (args.empty()) { outs[0] = Value::empty(); return; }
            if (args[0].isChar()) {
                std::string s = args[0].toString();
                std::string sl;
                for (char c : s) sl.push_back((char)std::tolower((unsigned char)c));
                if (sl == "auto") {
                    (ax.*field).clear();
                    fm.current().modified = true;
                    fm.emitModified();
                }
                outs[0] = Value::empty();
                return;
            }
            // Build "[\"a\",\"b\",...]" from cell/string-array.
            std::ostringstream os;
            os << '[';
            const size_t n = args[0].numel();
            for (size_t i = 0; i < n; ++i) {
                if (i) os << ',';
                Value elem = args[0].elemAt(i, ctx.engine->resource());
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
            outs[0] = Value::empty();
        };
    };
    reg("layout", "xticklabels", tickLabelsReg(&AxesState::xTickLabelsJson));
    reg("layout", "yticklabels", tickLabelsReg(&AxesState::yTickLabelsJson));
    reg("layout", "zticklabels", tickLabelsReg(&AxesState::zTickLabelsJson));

    // xtickformat / ytickformat / ztickformat — set a sprintf-style
    // format string ("%.2f", "%.0e", etc.). 'auto' clears.
    auto tickFormatReg = [](std::string AxesState::*field) {
        return [field](Span<const Value> args, size_t nargout,
                       Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            auto &fm = ctx.engine->figureManager();
            auto &ax = fm.currentAxes();
            if (args.empty() || !args[0].isChar()) {
                outs[0] = Value::empty();
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
            outs[0] = Value::empty();
        };
    };
    reg("layout", "xtickformat", tickFormatReg(&AxesState::xTickFormat));
    reg("layout", "ytickformat", tickFormatReg(&AxesState::yTickFormat));
    reg("layout", "ztickformat", tickFormatReg(&AxesState::zTickFormat));

    // xtickangle / ytickangle — rotation of tick labels in degrees.
    auto tickAngleReg = [](double AxesState::*field) {
        return [field](Span<const Value> args, size_t nargout,
                       Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            auto &fm = ctx.engine->figureManager();
            if (args.empty()) { outs[0] = Value::empty(); return; }
            fm.currentAxes().*field = args[0].toScalar();
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        };
    };
    reg("layout", "xtickangle", tickAngleReg(&AxesState::xTickAngle));
    reg("layout", "ytickangle", tickAngleReg(&AxesState::yTickAngle));

    // daspect([dx dy dz]) / pbaspect([px py pz]) — data + plot box
    // aspect ratios. v1 maps the canonical [1 1 1] case to
    // axisMode='equal' (equivalent visual). Other ratios accepted but
    // currently no-op — full anisotropic stretching is BACKLOG.
    auto aspectImpl = [](Span<const Value> args, size_t nargout,
                         Span<Value> outs, CallContext &ctx) {
        (void)nargout;
        auto &fm = ctx.engine->figureManager();
        if (args.empty()) { outs[0] = Value::empty(); return; }
        if (args[0].isChar()) {
            std::string s = args[0].toString();
            for (auto &c : s) c = (char)std::tolower((unsigned char)c);
            if (s == "auto") {
                fm.currentAxes().axisMode.clear();
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value::empty();
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
        outs[0] = Value::empty();
    };
    reg("layout", "daspect",  aspectImpl);
    reg("layout", "pbaspect", aspectImpl);

    // box(on|off|toggle) — show/hide the axis frame rectangle (the
    // full closed box vs. just left + bottom edges).
    reg("layout", "box",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            auto &fm = ctx.engine->figureManager();
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
            outs[0] = Value::empty();
        });

    reg("layout", "axes", noop_ret1);
    reg("layout", "gca", noop_ret1);
    reg("layout", "gcf", noop_ret1);
    // cla([reset]) — clear the current axes' datasets. With 'reset'
    // also clears the per-axis config (title, xlabel, etc.). MATLAB's
    // 'reset' is opt-in; default cla preserves axes properties.
    reg("layout", "cla",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            auto &fm = ctx.engine->figureManager();
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
            outs[0] = Value::empty();
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
    //
    // numkit doesn't model graphics handles, so animatedline-cluster
    // calls target the most recent animated dataset on the current
    // axes (axes::animatedDatasetIdx). animatedline returns an
    // opaque scalar (1-based index) for script compat, but every
    // op walks animatedDatasetIdx.
    //
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
                              Span<Value> outs, CallContext &ctx) {
            auto *mr = ctx.engine->resource();
            auto &fm = ctx.engine->figureManager();
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
                              Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            auto &fm = ctx.engine->figureManager();
            auto &ax = fm.currentAxes();
            if (ax.animatedDatasetIdx < 0
                || (size_t)ax.animatedDatasetIdx >= ax.datasets.size()) {
                outs[0] = Value::empty();
                return;
            }
            // Skip args[0] (handle) — addpoints(h, x, y).
            if (args.size() < 3) { outs[0] = Value::empty(); return; }
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
            outs[0] = Value::empty();
        });
    reg("layout", "clearpoints",
        [rebuildAnimatedJson](Span<const Value> args, size_t nargout,
                              Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            (void)args;
            auto &fm = ctx.engine->figureManager();
            auto &ax = fm.currentAxes();
            if (ax.animatedDatasetIdx < 0
                || (size_t)ax.animatedDatasetIdx >= ax.datasets.size()) {
                outs[0] = Value::empty();
                return;
            }
            auto &ds = ax.datasets[(size_t)ax.animatedDatasetIdx];
            ds.animatedX.clear();
            ds.animatedY.clear();
            rebuildAnimatedJson(ds);
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });
    reg("layout", "getpoints",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)args;
            auto *mr = ctx.engine->resource();
            auto &fm = ctx.engine->figureManager();
            auto &ax = fm.currentAxes();
            if (ax.animatedDatasetIdx < 0
                || (size_t)ax.animatedDatasetIdx >= ax.datasets.size()) {
                outs[0] = Value::empty();
                if (nargout >= 2) outs[1] = Value::empty();
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
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)args; (void)nargout;
            auto &fm = ctx.engine->figureManager();
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // ────────────────────────────────────────────────────────────────
    // geoplot / geoscatter / geobubble — geographic plots without a
    // basemap. The basemap-tile path (Web Mercator + WMS fetch) is
    // BACKLOG; for v1 these route through plot/scatter with
    // (X = lon, Y = lat) so user scripts that target geographic
    // axes still produce the right scatter / line shape.
    //
    // Forms supported (matching MATLAB):
    //   geoplot(lat, lon)
    //   geoplot(lat, lon, lineSpec)
    //   geoscatter(lat, lon[, sizes[, color]])
    //   geobubble(lat, lon, sizes)   — sizes drive marker radius
    // ────────────────────────────────────────────────────────────────
    auto geoForward = [](const char *target, Span<const Value> args,
                         Span<Value> outs, CallContext &ctx) {
        if (args.size() < 2) { outs[0] = Value::empty(); return; }
        // Build a new arg list with (lon, lat, ...) — i.e. swap the
        // first two so target's X = lon, Y = lat.
        std::vector<Value> proxied;
        proxied.reserve(args.size());
        proxied.push_back(args[1]);   // lon → X
        proxied.push_back(args[0]);   // lat → Y
        for (size_t i = 2; i < args.size(); ++i) proxied.push_back(args[i]);
        std::array<Value, 4> outBuf;
        const ExternalFunc *cf = ctx.engine->findExternal(target, ctx.env);
        if (!cf) { outs[0] = Value::empty(); return; }
        (*cf)(Span<const Value>(proxied.data(), proxied.size()), 0,
              Span<Value>(outBuf.data(), 1), ctx);
        // Add convenience axis labels so the "no basemap" output is
        // self-explanatory (lat/lon instead of generic X/Y).
        auto &ax = ctx.engine->figureManager().currentAxes();
        if (ax.xlabel.empty()) ax.xlabel = "lon";
        if (ax.ylabel.empty()) ax.ylabel = "lat";
        ctx.engine->figureManager().current().modified = true;
        ctx.engine->figureManager().emitModified();
        outs[0] = Value::empty();
    };
    reg("line", "geoplot",
        [geoForward](Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
            geoForward("plot", a, o, c);
        });
    reg("bar", "geoscatter",
        [geoForward](Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
            geoForward("scatter", a, o, c);
        });
    reg("bar", "geobubble",
        [geoForward](Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
            // bubblechart routing — scatter accepts sizes as its 3rd arg.
            geoForward("scatter", a, o, c);
        });

    // ────────────────────────────────────────────────────────────────
    // bubblechart / bubblechart3 / swarmchart — variants of scatter
    // / scatter3 that emphasise size-modulated markers. We delegate
    // to the underlying scatter family; the size vector flows through
    // unchanged and the renderer applies it as marker radius.
    // ────────────────────────────────────────────────────────────────
    auto delegateTo = [](const char *target, Span<const Value> args,
                         Span<Value> outs, CallContext &ctx) {
        const ExternalFunc *cf = ctx.engine->findExternal(target, ctx.env);
        if (!cf) { outs[0] = Value::empty(); return; }
        std::array<Value, 1> outBuf;
        (*cf)(args, 0, Span<Value>(outBuf.data(), 1), ctx);
        outs[0] = Value::empty();
    };
    reg("bar", "bubblechart",
        [delegateTo](Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
            delegateTo("scatter", a, o, c);
        });
    reg("bar", "bubblechart3",
        [delegateTo](Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
            delegateTo("scatter3", a, o, c);
        });
    // swarmchart(x, y[, sizes[, color]]) — scatter with X-axis jitter
    // applied per unique categorical X. For each cluster of points
    // sharing the same X coordinate, points are spread along X within
    // a small interval; the spread amount scales with the cluster's
    // local rank (so the layout resembles a violin plot's swarm).
    reg("bar", "swarmchart",
        [](Span<const Value> args, size_t nargout,
           Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.size() < 2) { outs[0] = Value::empty(); return; }
            const auto &xv = args[0];
            const auto &yv = args[1];
            const size_t n = std::min(xv.numel(), yv.numel());
            auto *mr = ctx.engine->resource();
            auto Xjit = Value::matrix(1, n, ValueType::DOUBLE, mr);
            double *xj = Xjit.doubleDataMut();
            for (size_t i = 0; i < n; ++i) xj[i] = xv.elemAsDouble(i);
            // Group indices by integer-rounded X (categorical use case).
            std::map<long, std::vector<size_t>> buckets;
            for (size_t i = 0; i < n; ++i) {
                long key = (long)std::llround(xv.elemAsDouble(i));
                buckets[key].push_back(i);
            }
            // Per bucket, spread points uniformly in [-half, +half] of
            // the slot width. Slot = 0.35 × the smallest gap between
            // distinct X values, capped at 0.4 absolute.
            double minGap = 1.0;
            if (buckets.size() > 1) {
                long prev = LONG_MIN;
                for (const auto &[k, _] : buckets) {
                    if (prev != LONG_MIN) {
                        const double g = std::abs((double)(k - prev));
                        if (g < minGap || minGap == 1.0) minGap = g;
                    }
                    prev = k;
                }
            }
            const double half = std::min(0.4, 0.35 * minGap);
            for (auto &[k, idxs] : buckets) {
                const size_t m = idxs.size();
                if (m <= 1) continue;
                for (size_t j = 0; j < m; ++j) {
                    const double t = (m == 1) ? 0.0
                        : (-half + 2 * half * (double)j / (m - 1));
                    xj[idxs[j]] += t;
                }
            }
            std::vector<Value> proxied;
            proxied.push_back(std::move(Xjit));
            proxied.push_back(yv);
            for (size_t i = 2; i < args.size(); ++i) proxied.push_back(args[i]);
            std::array<Value, 1> outBuf;
            const ExternalFunc *cf = ctx.engine->findExternal("scatter", ctx.env);
            if (cf) {
                (*cf)(Span<const Value>(proxied.data(), proxied.size()), 0,
                      Span<Value>(outBuf.data(), 1), ctx);
            }
            outs[0] = Value::empty();
        });
    reg("bar", "swarmchart3",
        [delegateTo](Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
            delegateTo("scatter3", a, o, c);
        });

    // comet(x, y) / comet3(x, y, z) — animated trail. Routes to plot
    // with a `cometAnim=1` hint in the style string; the IDE's
    // CompositePlot picks up the flag and animates the polyline
    // progressively via requestAnimationFrame (the final-state
    // figure is the full curve).
    reg("line", "comet",
        [delegateTo](Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
            delegateTo("plot", a, o, c);
            // Stamp the just-pushed dataset with the animation hint.
            auto &fm = c.engine->figureManager();
            auto &ax = fm.currentAxes();
            if (!ax.datasets.empty()) {
                auto &ds = ax.datasets.back();
                if (!ds.style.empty()) ds.style += ";";
                ds.style += "cometAnim=1";
                fm.current().modified = true;
                fm.emitModified();
            }
        });
    reg("line", "comet3",
        [delegateTo](Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
            delegateTo("plot3", a, o, c);
            auto &fm = c.engine->figureManager();
            auto &ax = fm.currentAxes();
            if (!ax.datasets.empty()) {
                auto &ds = ax.datasets.back();
                if (!ds.style.empty()) ds.style += ";";
                ds.style += "cometAnim=1";
                fm.current().modified = true;
                fm.emitModified();
            }
        });

    // ────────────────────────────────────────────────────────────────
    // voronoi(x, y) — Voronoi diagram via Delaunay dual.
    //
    // Algorithm:
    //   1. Compute brute-force Delaunay triangulation of the cloud
    //      (same logic as delaunay_reg in libs/builtin).
    //   2. For each triangle, compute its circumcenter.
    //   3. For each pair of triangles sharing an edge, draw a line
    //      segment connecting their circumcenters — that's a Voronoi
    //      cell boundary edge.
    //   4. Emit as a single `line` dataset with null separators
    //      between segments, plus scatter markers at the input
    //      points.
    //
    // Cells touching the convex hull don't have a finite second
    // endpoint (the cell is unbounded); v1 just omits those edges.
    // Properly-extended infinite rays are BACKLOG.
    // ────────────────────────────────────────────────────────────────
    reg("line", "voronoi",
        [delegateTo](Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.size() < 2) { outs[0] = Value::empty(); return; }
            const auto &xv = args[0];
            const auto &yv = args[1];
            const size_t n = xv.numel();
            if (yv.numel() != n || n < 3) { outs[0] = Value::empty(); return; }
            std::vector<double> X(n), Y(n);
            for (size_t i = 0; i < n; ++i) {
                X[i] = xv.elemAsDouble(i);
                Y[i] = yv.elemAsDouble(i);
            }
            // Same Delaunay loop as libs/builtin (kept local to avoid
            // cross-lib dependency).
            auto sa2 = [&](size_t a, size_t b, size_t c) {
                return (X[b]-X[a]) * (Y[c]-Y[a]) - (Y[b]-Y[a]) * (X[c]-X[a]);
            };
            auto inC = [&](size_t a, size_t b, size_t c, size_t p) {
                const double ax=X[a]-X[p], ay=Y[a]-Y[p];
                const double bx=X[b]-X[p], by=Y[b]-Y[p];
                const double cx=X[c]-X[p], cy=Y[c]-Y[p];
                const double a2=ax*ax+ay*ay;
                const double b2=bx*bx+by*by;
                const double c2=cx*cx+cy*cy;
                return ax*(by*c2-cy*b2) - ay*(bx*c2-cx*b2) + a2*(bx*cy-cx*by);
            };
            std::vector<std::array<size_t, 3>> tris;
            for (size_t a = 0; a < n; ++a)
                for (size_t b = a + 1; b < n; ++b)
                    for (size_t c = b + 1; c < n; ++c) {
                        const double s = sa2(a, b, c);
                        if (std::abs(s) < 1e-15) continue;
                        size_t va=a, vb=b, vc=c;
                        if (s < 0) std::swap(vb, vc);
                        bool ok = true;
                        for (size_t p = 0; p < n; ++p) {
                            if (p == va || p == vb || p == vc) continue;
                            if (inC(va, vb, vc, p) > 1e-12) { ok = false; break; }
                        }
                        if (ok) tris.push_back({va, vb, vc});
                    }
            // Per-triangle circumcenter.
            struct CC { double x, y; };
            std::vector<CC> ccs(tris.size());
            for (size_t t = 0; t < tris.size(); ++t) {
                const auto &T = tris[t];
                const double ax=X[T[0]], ay=Y[T[0]];
                const double bx=X[T[1]], by=Y[T[1]];
                const double cx=X[T[2]], cy=Y[T[2]];
                const double d = 2.0 * (ax*(by-cy) + bx*(cy-ay) + cx*(ay-by));
                if (std::abs(d) < 1e-15) { ccs[t] = {0, 0}; continue; }
                const double ax2_ay2 = ax*ax + ay*ay;
                const double bx2_by2 = bx*bx + by*by;
                const double cx2_cy2 = cx*cx + cy*cy;
                ccs[t].x = (ax2_ay2*(by-cy) + bx2_by2*(cy-ay) + cx2_cy2*(ay-by)) / d;
                ccs[t].y = (ax2_ay2*(cx-bx) + bx2_by2*(ax-cx) + cx2_cy2*(bx-ax)) / d;
            }
            // For each ordered triangle pair, check if they share an
            // edge — if so emit a line segment between their CCs.
            auto sharesEdge = [&](const std::array<size_t,3> &A,
                                   const std::array<size_t,3> &B) {
                int shared = 0;
                for (auto a : A) for (auto b : B) if (a == b) shared++;
                return shared == 2;
            };
            std::ostringstream xs, ys;
            xs << '['; ys << '[';
            bool first = true;
            for (size_t i = 0; i < tris.size(); ++i) {
                for (size_t j = i + 1; j < tris.size(); ++j) {
                    if (!sharesEdge(tris[i], tris[j])) continue;
                    if (!first) { xs << ",null,"; ys << ",null,"; }
                    first = false;
                    xs << ccs[i].x << ',' << ccs[j].x;
                    ys << ccs[i].y << ',' << ccs[j].y;
                }
            }
            xs << ']'; ys << ']';

            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            DatasetInfo edgeDs;
            edgeDs.type = "line";
            edgeDs.xJson = xs.str();
            edgeDs.yJson = ys.str();
            edgeDs.style = "color=#5fb3d4";
            fm.pushDataset(std::move(edgeDs));
            // Scatter the input points on top.
            std::array<Value, 2> ptArgs{ xv, yv };
            std::array<Value, 1> outBuf;
            const ExternalFunc *cf = ctx.engine->findExternal("scatter", ctx.env);
            if (cf) {
                const bool wasHold = fm.currentAxes().holdOn;
                fm.currentAxes().holdOn = true;
                (*cf)(Span<const Value>(ptArgs.data(), 2), 0,
                      Span<Value>(outBuf.data(), 1), ctx);
                fm.currentAxes().holdOn = wasHold;
            }
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // heatmap(C) — table-style heatmap. Same data layout as imagesc:
    // 2-D matrix of values mapped through a colormap. Optional row/
    // column label strings ('RowNames' / 'ColumnNames') are accepted
    // but currently flow through as labels only — full table-style
    // heatmap with cell-text overlays + row/col headers is BACKLOG.
    //
    // Forms supported (subset):
    //   heatmap(C)
    //   heatmap(C, 'Colormap', name)
    // Helper — emit a text-annotation dataset at (x, y) with label.
    // Picks a colour that contrasts with the cell value (white on
    // dark bins, black on light bins) via a brightness threshold.
    auto pushCellText = [](FigureManager &fm, double x, double y,
                           const std::string &label, bool darkBg) {
        DatasetInfo t;
        t.type = "text";
        std::ostringstream xs, ys;
        xs << '[' << x << ']'; ys << '[' << y << ']';
        t.xJson = xs.str();
        t.yJson = ys.str();
        t.label = label;
        t.style = std::string("color=") + (darkBg ? "#f0f0f0" : "#202020")
                  + ";fontSize=11";
        fm.pushDataset(std::move(t));
    };

    reg("bar", "heatmap",
        [delegateTo, pushCellText](Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
            // Drop trailing N-V pairs that the imagesc adapter doesn't
            // know about — keep only the first numeric argument as the
            // data matrix. 'Colormap' is honoured via a colormap() call
            // right after; 'CellLabelColor' / 'CellLabelFormat' etc.
            // are accepted as no-ops. Cell-label text is enabled by
            // default for heatmap(table); pass 'CellLabel','off' to
            // disable.
            std::vector<Value> proxied;
            std::string cmap;
            bool cellLabel = true;
            std::string labelFmt = "%g";
            for (size_t i = 0; i < a.size(); ++i) {
                if (a[i].isChar()) {
                    if (i + 1 < a.size()) {
                        std::string key = a[i].toString();
                        for (auto &cc : key)
                            cc = (char)std::tolower((unsigned char)cc);
                        if (key == "colormap" && a[i + 1].isChar()) {
                            cmap = a[i + 1].toString();
                            ++i; continue;
                        }
                        if (key == "celllabel" && a[i + 1].isChar()) {
                            std::string v = a[i + 1].toString();
                            for (auto &cc : v) cc = (char)std::tolower((unsigned char)cc);
                            cellLabel = (v != "off");
                            ++i; continue;
                        }
                        if (key == "celllabelformat" && a[i + 1].isChar()) {
                            labelFmt = a[i + 1].toString();
                            ++i; continue;
                        }
                    }
                    if (i + 1 < a.size()) ++i;
                    continue;
                }
                proxied.push_back(a[i]);
            }
            delegateTo("imagesc",
                       Span<const Value>(proxied.data(), proxied.size()), o, c);
            if (!cmap.empty()) {
                c.engine->figureManager().currentAxes().colormapName = cmap;
            }
            // Cell-text overlay — one <text> per cell with the
            // formatted value. Skipped for very large matrices to
            // avoid clutter.
            if (cellLabel && !proxied.empty()) {
                const auto &C = proxied[0];
                const size_t R = C.dims().rows();
                const size_t W = C.dims().cols();
                if (R * W <= 400) {   // ≤ 20×20 grid — show labels
                    double cmn = std::numeric_limits<double>::infinity();
                    double cmx = -std::numeric_limits<double>::infinity();
                    for (size_t i = 0; i < R * W; ++i) {
                        const double v = C.elemAsDouble(i);
                        if (std::isfinite(v)) {
                            if (v < cmn) cmn = v;
                            if (v > cmx) cmx = v;
                        }
                    }
                    auto &fm = c.engine->figureManager();
                    for (size_t r = 0; r < R; ++r) {
                        for (size_t cc = 0; cc < W; ++cc) {
                            const double v = C.doubleData()[cc * R + r];
                            const double t_ = (cmx == cmn) ? 0.5
                                : (v - cmn) / (cmx - cmn);
                            // Dark background ↔ low t (blue/cyan side
                            // of parula). Threshold 0.4 picks readable
                            // colour against parula's gradient.
                            const bool dark = (t_ < 0.4);
                            char buf[32];
                            std::snprintf(buf, sizeof buf, labelFmt.c_str(), v);
                            pushCellText(fm,
                                         (double)(cc + 1),
                                         (double)(r + 1),
                                         std::string(buf), dark);
                        }
                    }
                }
            }
            c.engine->figureManager().current().modified = true;
            c.engine->figureManager().emitModified();
        });

    // confusionchart(C [, classNames]) — confusion matrix heatmap
    // with cell-value labels. Same as heatmap(C) but adds explicit
    // axis labels and (when classNames given) categorical tick
    // labels on both axes.
    reg("bar", "confusionchart",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.empty()) { outs[0] = Value::empty(); return; }
            // Forward to heatmap (cellLabel default on, no Colormap).
            const ExternalFunc *hf = ctx.engine->findExternal("heatmap", ctx.env);
            if (!hf) { outs[0] = Value::empty(); return; }
            std::array<Value, 1> outBuf;
            std::array<Value, 1> proxied{ args[0] };
            (*hf)(Span<const Value>(proxied.data(), 1), 0,
                  Span<Value>(outBuf.data(), 1), ctx);
            auto &fm = ctx.engine->figureManager();
            auto &ax = fm.currentAxes();
            ax.xlabel = "Predicted Class";
            ax.ylabel = "True Class";
            // Optional class names as 2nd arg (cell of chars / string array).
            if (args.size() >= 2) {
                const Value &cn = args[1];
                const size_t n = cn.numel();
                std::ostringstream js;
                js << '[';
                for (size_t i = 0; i < n; ++i) {
                    if (i) js << ',';
                    Value elem = cn.elemAt(i, ctx.engine->resource());
                    std::string s = elem.isChar() ? elem.toString() : "";
                    js << '"';
                    for (char ch : s) {
                        if (ch == '"' || ch == '\\') js << '\\';
                        js << ch;
                    }
                    js << '"';
                }
                js << ']';
                ax.xTickLabelsJson = js.str();
                ax.yTickLabelsJson = js.str();
                // Tick positions = 1..n so they align with cell centres.
                std::ostringstream ts; ts << '[';
                for (size_t i = 0; i < n; ++i) {
                    if (i) ts << ',';
                    ts << (i + 1);
                }
                ts << ']';
                ax.xTicksJson = ts.str();
                ax.yTicksJson = ts.str();
            }
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // parallelplot — parallel-coordinates plot. v1 routes each row of
    // the input matrix as one line plot across N axis indices; the
    // dedicated multi-axis "parallel" rendering is BACKLOG.
    reg("line", "parallelplot",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.empty()) { outs[0] = Value::empty(); return; }
            const auto &T = args[0];
            const size_t R = T.dims().rows();
            const size_t C = T.dims().cols();
            if (R == 0 || C == 0) { outs[0] = Value::empty(); return; }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            // Hold on so all rows pile in one figure.
            const bool wasHold = fm.currentAxes().holdOn;
            fm.currentAxes().holdOn = true;
            const ExternalFunc *cf = ctx.engine->findExternal("plot", ctx.env);
            if (!cf) { outs[0] = Value::empty(); return; }
            auto *mr = ctx.engine->resource();
            auto xv = Value::matrix(1, C, ValueType::DOUBLE, mr);
            for (size_t c = 0; c < C; ++c) xv.doubleDataMut()[c] = (double)(c + 1);
            for (size_t r = 0; r < R; ++r) {
                auto yv = Value::matrix(1, C, ValueType::DOUBLE, mr);
                for (size_t c = 0; c < C; ++c)
                    yv.doubleDataMut()[c] = T.elemAsDouble(c * R + r);
                std::array<Value, 2> proxied{ xv, yv };
                std::array<Value, 1> outBuf;
                (*cf)(Span<const Value>(proxied.data(), 2), 0,
                      Span<Value>(outBuf.data(), 1), ctx);
            }
            fm.currentAxes().holdOn = wasHold;
            outs[0] = Value::empty();
        });

    // zlabel(text) — 3-D Z-axis label.
    reg("layout", "zlabel",
        [argStr](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.empty()) { outs[0] = Value::empty(); return; }
            auto &fm = ctx.engine->figureManager();
            fm.currentAxes().zlabel = argStr(args[0]);
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // zlim([z0, z1]) — 3-D Z-axis limits.
    reg("layout", "zlim",
        [vecToJson](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.empty() || args[0].numel() < 2) { outs[0] = Value::empty(); return; }
            auto &fm = ctx.engine->figureManager();
            fm.currentAxes().zlimJson = vecToJson(args[0]);
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });
    // view(az, el) — set the 3-D camera azimuth/elevation in degrees.
    // Both args required for the (az, el) form; 2-element vector also
    // accepted (view([az, el])). The renderer reads cfg.view on mount
    // of Composite3DPlot. Stash on AxesState so it survives prepareForPlot.
    reg("layout", "view",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.empty()) { outs[0] = Value::empty(); return; }
            auto &fm = ctx.engine->figureManager();
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
            if (!ok) { outs[0] = Value::empty(); return; }
            std::ostringstream os;
            os << "[" << az << "," << el << "]";
            fm.currentAxes().viewJson = os.str();
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });
    reg("layout", "set", noop);
    reg("layout", "get", noop_ret1);

    // scatter3 — real impl registered earlier via plot3Impl shared body.
    // pcolor — real implementation registered earlier in install()
    // (graphics.image.pcolor + compat.pcolor). The duplicate noop
    // here used to register `compat.pcolor` a second time, which the
    // engine's registerFunction rejects, throwing during install and
    // dropping the renderer into fallback mode.
    // xline(x [, lineSpec]) / yline(y [, lineSpec]) — reference lines
    // extending across the visible viewport. Each call emits a
    // dataset with type='xline' / 'yline'; the adapter routes to a
    // viewport-spanning line layer that the renderer redraws on
    // every pan / zoom (range scan ignores these so the auto-range
    // doesn't blow up to ±∞).
    auto refLineImpl = [](const char *type) {
        return [type](Span<const Value> args, size_t nargout,
                      Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            if (args.empty() || !args[0].numel()) {
                outs[0] = Value::empty();
                return;
            }
            auto &fm = ctx.engine->figureManager();
            fm.prepareForPlot();
            // Vector form: emit one dataset per requested position.
            const size_t n = args[0].numel();
            std::string lineSpec;
            for (size_t i = 1; i < args.size(); ++i) {
                if (args[i].isChar()) { lineSpec = args[i].toString(); break; }
            }
            for (size_t i = 0; i < n; ++i) {
                DatasetInfo ds;
                ds.type = type;
                std::ostringstream xs, ys;
                xs << '[' << args[0].elemAsDouble(i) << ']';
                ys << '[' << 0 << ']';   // sentinel y; renderer ignores
                ds.xJson = xs.str();
                ds.yJson = ys.str();
                if (!lineSpec.empty()) ds.style = lineSpec;
                fm.pushDataset(std::move(ds));
            }
            fm.emitModified();
            outs[0] = Value::empty();
        };
    };
    reg("line", "xline", refLineImpl("xline"));
    reg("line", "yline", refLineImpl("yline"));
    // camlight(['left'|'right'|'headlight']) — adds a directional
    // light positioned relative to the camera. Default: headlight
    // (light from camera).
    reg("surface", "camlight",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            auto &fm = ctx.engine->figureManager();
            std::string pos = "headlight";
            if (!args.empty() && args[0].isChar()) {
                std::string s = args[0].toString();
                for (auto &c : s) c = (char)std::tolower((unsigned char)c);
                if (s == "left" || s == "right" || s == "headlight") pos = s;
            }
            fm.currentAxes().camlightPos = pos;
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // lighting('flat'|'gouraud'|'phong'|'none') — material shading
    // model. Default 'gouraud' (smooth Lambert), 'flat' uses per-face
    // normals, 'phong' adds specular highlights, 'none' removes
    // shading entirely.
    reg("surface", "lighting",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            auto &fm = ctx.engine->figureManager();
            std::string m = "gouraud";
            if (!args.empty() && args[0].isChar()) {
                std::string s = args[0].toString();
                for (auto &c : s) c = (char)std::tolower((unsigned char)c);
                if (s == "flat" || s == "gouraud" || s == "phong" || s == "none")
                    m = s;
            }
            fm.currentAxes().lightingMode = m;
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // material('shiny'|'metal'|'dull') — preset specular response
    // (only meaningful with lighting='phong').
    reg("surface", "material",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            auto &fm = ctx.engine->figureManager();
            std::string m;
            if (!args.empty() && args[0].isChar()) {
                std::string s = args[0].toString();
                for (auto &c : s) c = (char)std::tolower((unsigned char)c);
                if (s == "shiny" || s == "metal" || s == "dull") m = s;
            }
            fm.currentAxes().materialPreset = m;
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });

    // surfl(Z[, X, Y]) — surf + auto camlight + lighting gouraud.
    // Implemented as a thin wrapper that calls compat.surf followed by
    // setting camlightPos = 'headlight' and lightingMode = 'gouraud'
    // on the resulting axes.
    reg("surface", "surfl",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            (void)nargout;
            const ExternalFunc *cf = ctx.engine->findExternal("surf", ctx.env);
            if (!cf) { outs[0] = Value::empty(); return; }
            std::array<Value, 1> outBuf;
            (*cf)(args, 0, Span<Value>(outBuf.data(), 1), ctx);
            auto &fm = ctx.engine->figureManager();
            fm.currentAxes().camlightPos = "headlight";
            fm.currentAxes().lightingMode = "gouraud";
            fm.currentAxes().materialPreset = "shiny";
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value::empty();
        });
}

} // namespace numkit
