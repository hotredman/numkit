// toolboxes/graphics/src/library.cpp
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

#include <numkit/graphics/graphics_context.hpp>

#include "plots/plot_internal.hpp"

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

// Builds the full graphics plotting table. Core-free: every body takes the
// GraphicsContext (fm / mr / callBuiltin / callHandle), never the Engine. The
// bundle-side installer (library.cpp) wraps each entry and registers it.
void buildPlotTable(std::vector<PlotEntry> &table)
{
    using namespace detail;

    // ── Collect a graphics.<sub>.<name> (+ compat.<name>) entry ──
    auto reg = [&](const char *sub, const char *name, GraphicsFn fn) {
        table.push_back(PlotEntry{sub, name, /*core=*/false, std::move(fn)});
    };

    // ── core=true: ALSO bare-name into core (figure / close / hold …), for
    // session/workspace-style commands on par with `clear` / `who` — reachable
    // without any `import`. Use sparingly — see NAMESPACE_DESIGN.md §7.
    auto regCore = [&](const char *sub, const char *name, GraphicsFn fn) {
        table.push_back(PlotEntry{sub, name, /*core=*/true, std::move(fn)});
    };

    // ================================================================
    // Helper lambdas
    // ================================================================

    // (pure JSON/parse helpers moved to plots/plot_internal.hpp — numkit::detail)

    // ================================================================
    // Figure management — graphics.layout
    // ================================================================

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

    // ================================================================
    // Plot types — graphics.line / graphics.bar / graphics.image
    // ================================================================

    reg("line", "plot",
        [](Span<const Value> args, size_t nargout,
                                          Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "line";
            size_t nvStart = parsePlotXYStyle(args, ds);
            parsePlotArgs(args, nvStart, ds);
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });

    reg("bar", "bar",
        [](Span<const Value> args, size_t nargout,
                                   Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
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
                outs[0] = Value();
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
            outs[0] = Value();
        });

    // plot3(x, y, z) / scatter3(x, y, z) — 3-D series. The renderer
    // does a cabinet-projection to 2-D (z extends into upper-right at
    // 30°, scaled 0.5). Real 3-D camera is B3 territory; this gets
    // the data on screen so users can write the script and inspect
    // values today. Both reuse the line / scatter render modes after
    // adapter projection.
    auto plot3Impl = [](
                         const char *typeName,
                         Span<const Value> args, size_t nargout,
                         Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        if (args.size() < 3) {
            outs[0] = Value();
            return;
        }
        auto &fm = gc.fm;
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
        outs[0] = Value();
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
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (args.size() < 3) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
            fm.prepareForPlot();
            const auto &X = args[0];
            const auto &Y = args[1];
            const auto &Z = args[2];
            const size_t N = std::min({X.numel(), Y.numel(), Z.numel()});
            if (N == 0) { outs[0] = Value(); return; }

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
            outs[0] = Value();
        });

    // quiver(x, y, u, v) — vector field as N arrows starting at
    // (x[i], y[i]) and pointing in direction (u[i], v[i]). MATLAB
    // accepts matrices; for first cut we expect flat vectors with
    // matching length. quiver(x, y, u, v, scale) optionally scales
    // arrow lengths (default 1, packed into ds.style as "scale=N").
    reg("line", "quiver",
        [](Span<const Value> args, size_t nargout,
                    Span<Value> outs, GraphicsContext &gc) {
            if (args.size() < 4) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
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
            outs[0] = Value();
        });

    buildPolarPlots(table);
    // feather(U, V) — arrows on the x-axis. Each arrow starts at
    // (i, 0) and points to (i + U_i, V_i). Same scale=1 contract as
    // compass.
    reg("line", "feather",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (args.size() < 2) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
            fm.prepareForPlot();
            const auto &U = args[0];
            const auto &V = args[1];
            const size_t N = std::min(U.numel(), V.numel());
            if (N == 0) { outs[0] = Value(); return; }
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
            outs[0] = Value();
        });

    // ────────── Statistical chart wrappers ───────────────────────────
    // These all reduce to 1-2 existing dataset types (scatter / line /
    // bar / stairs) so the renderer doesn't need any new code.

    // cdfplot(x) / ecdf(x) — empirical cumulative distribution. Sorts
    // x ascending and renders a right-continuous step function from
    // 0 to 1. NaN inputs are dropped.
    auto cdfImpl = [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        if (args.empty() || args[0].numel() == 0) { outs[0] = Value(); return; }
        const auto &X = args[0];
        std::vector<double> xs;
        xs.reserve(X.numel());
        for (size_t i = 0; i < X.numel(); ++i) {
            const double v = X.doubleData()[i];
            if (std::isfinite(v)) xs.push_back(v);
        }
        if (xs.empty()) { outs[0] = Value(); return; }
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

        auto &fm = gc.fm;
        fm.prepareForPlot();
        DatasetInfo ds;
        ds.type = "stairs";
        ds.xJson = sx.str();
        ds.yJson = sy.str();
        ds.style = "color=#1f77b4";
        fm.pushDataset(std::move(ds));
        fm.emitModified();
        outs[0] = Value();
    };
    reg("bar", "cdfplot", cdfImpl);
    // `ecdf` is already provided by toolboxes/stats as a computational
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
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) { outs[0] = Value(); return; }
            std::vector<double> xs;
            for (size_t i = 0; i < args[0].numel(); ++i) {
                const double v = args[0].doubleData()[i];
                if (std::isfinite(v)) xs.push_back(v);
            }
            if (xs.size() < 2) { outs[0] = Value(); return; }
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

            auto &fm = gc.fm;
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
            outs[0] = Value();
        });

    // pareto(Y) — bars in descending Y order + a cumulative-percent
    // line on the same axes. Useful for "80/20" / quality-control
    // visualisations. We emit two datasets:
    //   1. type=bar, X = 1..N (rank), Y sorted descending
    //   2. type=line, same X, Y = 100 * cumsum(Y) / sum(Y)
    reg("bar", "pareto",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) { outs[0] = Value(); return; }
            std::vector<double> ys;
            for (size_t i = 0; i < args[0].numel(); ++i) {
                const double v = args[0].doubleData()[i];
                if (std::isfinite(v)) ys.push_back(v);
            }
            if (ys.empty()) { outs[0] = Value(); return; }
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

            auto &fm = gc.fm;
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
            outs[0] = Value();
        });

    // histfit(x[, nbins]) — histogram of x with a Gaussian fit overlay.
    // Default 10 bins. The fit uses sample mean/std; PDF is scaled by
    // (N * binWidth) so the curve is visually comparable to the bar
    // counts.
    reg("bar", "histfit",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) { outs[0] = Value(); return; }
            std::vector<double> xs;
            for (size_t i = 0; i < args[0].numel(); ++i) {
                const double v = args[0].doubleData()[i];
                if (std::isfinite(v)) xs.push_back(v);
            }
            if (xs.size() < 2) { outs[0] = Value(); return; }
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

            auto &fm = gc.fm;
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
            outs[0] = Value();
        });

    // gscatter(x, y, g) — scatter coloured by group label. Each unique
    // value of g becomes its own scatter dataset (so it picks up a
    // distinct color from the renderer's palette).
    reg("bar", "gscatter",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.size() < 3) { outs[0] = Value(); return; }
            const auto &X = args[0];
            const auto &Y = args[1];
            const auto &G = args[2];
            const size_t N = std::min({X.numel(), Y.numel(), G.numel()});
            if (N == 0) { outs[0] = Value(); return; }

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

            auto &fm = gc.fm;
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
            outs[0] = Value();
        });

    // spy(M) — sparsity pattern. Renders a marker at every (col, row)
    // where M is non-zero (and finite). Mirrors MATLAB convention of
    // axis ij so the matrix sits like the printed form (row 1 at top).
    reg("bar", "spy",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
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
            outs[0] = Value();
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
        [](Span<const Value> args, size_t nargout,
                                   Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
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
            if (!yData) { outs[0] = Value(); return; }

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
                outs[0] = Value();
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
            outs[0] = Value();
        });

    // barh — horizontal bar chart, mirror of bar(). MATLAB convention:
    //   barh(y)    — y is vector of bar lengths, vertical positions = 1:N
    //   barh(x, y) — x is vertical positions (categories), y = lengths
    // We store xJson = positions (rendered along the Y axis) and
    // yJson = lengths (along X axis); the 'barh' mode in the renderer
    // swaps the axis roles compared to 'bar'.
    reg("bar", "barh",
        [](Span<const Value> args, size_t nargout,
                                   Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
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
            outs[0] = Value();
        });

    reg("bar", "scatter",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (args.size() < 2) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "scatter";
            ds.xJson = vecToJson(args[0]);
            ds.yJson = vecToJson(args[1]);
            // scatter(x, y, ..., 'filled') → filled markers; MATLAB's default is
            // open (the renderer draws an outline unless the style says filled).
            for (size_t k = 2; k < args.size(); ++k) {
                if (args[k].isChar()) {
                    std::string s = args[k].toString();
                    for (auto &c : s) c = static_cast<char>(std::tolower(c));
                    if (s == "filled") { ds.style = "filled=1"; break; }
                }
            }
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });

    reg("bar", "hist",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto *mr = gc.mr;
            if (args.empty()) {
                outs[0] = Value();
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
            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "bar";
            ds.xJson = vecToJson(centers);
            ds.yJson = vecToJson(counts);
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });

    // Shared body for any heatmap-like builtin (imagesc / pcolor).
    // The data-emission path is identical; only the `type` field
    // differs (renderer uses it to pick cell-centre vs cell-vertex
    // alignment). Body kept as a captured lambda + std::bind to avoid
    // duplicating ~200 lines of quantization logic.
    auto heatmapImpl = [](
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
    };  // heatmapImpl

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

    // histogram2(X, Y) — 2-D histogram. Bins (X, Y) into an nx×ny grid
    // and renders the count matrix as an imagesc-style heatmap. Param
    // forms supported (positional, like MATLAB's classic call):
    //   histogram2(X, Y)              — 10×10 bins over data extent
    //   histogram2(X, Y, n)           — n×n
    //   histogram2(X, Y, [nx ny])     — explicit grid
    //   histogram2(X, Y, nx, ny)      — explicit grid (separate args)
    // Name-Value form (NumBins, BinEdges, …) is on the BACKLOG.
    reg("bar", "histogram2",
        [heatmapImpl](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto *mr = gc.mr;
            if (args.size() < 2 || args[0].numel() == 0 || args[1].numel() == 0) {
                outs[0] = Value();
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
            // Delegate to heatmapImpl. axisIj=false — histogram2 stays on
            // axis xy by default (Y up), unlike imagesc which auto-flips.
            heatmapImpl("imagesc", false,
                        Span<const Value>(proxied.data(), proxied.size()),
                        nargout, outs, gc);
        });

    buildContourPlots(table);
    buildSurfacePlots(table);
    // quiver3(x, y, z, u, v, w[, scale]) — 3-D vector field. Each (x,
    // y, z, u, v, w) row becomes one arrow from (x, y, z) to
    // (x + s·u, y + s·v, z + s·w). Default scale = 1.
    reg("line", "quiver3",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.size() < 6) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
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
            outs[0] = Value();
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
                        Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        if (args.size() < 2) { outs[0] = Value(); return; }
        const auto &X = args[0];
        const auto &Y = args[1];
        const size_t R = X.dims().rows();
        const size_t C = std::max<size_t>(1, X.dims().cols());
        if (R == 0) { outs[0] = Value(); return; }
        if (Y.dims().rows() != R || std::max<size_t>(1, Y.dims().cols()) != C) {
            outs[0] = Value();
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

        auto &fm = gc.fm;
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
        outs[0] = Value();
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
                      GraphicsContext &gc, bool tilt3d) {
        (void)nargout;
        if (args.empty() || args[0].numel() == 0) {
            outs[0] = Value();
            return;
        }
        const auto &X = args[0];
        const size_t N = X.numel();
        double total = 0;
        for (size_t i = 0; i < N; ++i) {
            const double v = X.doubleData()[i];
            if (std::isfinite(v) && v > 0) total += v;
        }
        if (total <= 0) { outs[0] = Value(); return; }

        const Value *expl = (args.size() >= 2) ? &args[1] : nullptr;
        static const char *kPalette[] = {
            "#1f77b4", "#ff7f0e", "#2ca02c", "#d62728",
            "#9467bd", "#8c564b", "#e377c2", "#7f7f7f",
            "#bcbd22", "#17becf",
        };

        const double TAU = 2 * 3.14159265358979323846;
        const int arcSamples = 24;          // vertices per wedge arc
        const double tiltY = tilt3d ? 0.4 : 1.0;   // pie3 squashes Y

        auto &fm = gc.fm;
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
        outs[0] = Value();
    };
    reg("bar", "pie",  [pieImpl](Span<const Value> a, size_t n, Span<Value> o, GraphicsContext &gc) {
        pieImpl(a, n, o, gc, false);
    });
    reg("bar", "pie3", [pieImpl](Span<const Value> a, size_t n, Span<Value> o, GraphicsContext &gc) {
        pieImpl(a, n, o, gc, true);
    });

    // boxplot(X) / boxchart(X) — Tukey box-and-whisker plot.
    // X as vector → one box at x=1.
    // X as matrix → one box per column at x = 1..C.
    // Per box we emit:
    //   • polygon for the IQR rectangle (Q1..Q3)
    //   • line for the median (horizontal across the box)
    //   • line dataset for the two whisker stems + the two caps
    //   • scatter dataset for outliers (beyond ±1.5·IQR)
    auto boxImpl = [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        if (args.empty() || args[0].numel() == 0) {
            outs[0] = Value();
            return;
        }
        const auto &M = args[0];
        const size_t R = std::max<size_t>(1, M.dims().rows());
        const size_t C = std::max<size_t>(1, M.dims().cols());
        // Treat 1×N or N×1 as a single column; matrix → C columns.
        const bool single = (R == 1 || C == 1);
        const size_t nBoxes = single ? 1 : C;
        const size_t nPerBox = single ? M.numel() : R;

        auto &fm = gc.fm;
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
        outs[0] = Value();
    };
    reg("bar", "boxplot",  boxImpl);
    reg("bar", "boxchart", boxImpl);

    // violinplot(X) — Gaussian-KDE shape + slim box + median dot.
    // Layout mirrors boxplot: vector → one violin at x=1, matrix
    // → one violin per column at x = 1..C. KDE bandwidth uses
    // Silverman's rule (1.06 · σ · N^(-1/5)).
    reg("bar", "violinplot",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) {
                outs[0] = Value();
                return;
            }
            const auto &M = args[0];
            const size_t R = std::max<size_t>(1, M.dims().rows());
            const size_t Cmat = std::max<size_t>(1, M.dims().cols());
            const bool single = (R == 1 || Cmat == 1);
            const size_t nViolins = single ? 1 : Cmat;
            const size_t nPerColumn = single ? M.numel() : R;

            auto &fm = gc.fm;
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
            outs[0] = Value();
        });

    // bar3(Z) — 3-D bars. Emits raw 3-D coords so the WebGL renderer
    // can build cuboids with real depth + lighting; no more cabinet
    // pre-projection. Wire format: type='bar3' with the Z-matrix as
    // nested rows (same shape as surf), plus implicit (x, y) coords =
    // (1..C, 1..R).
    reg("bar", "bar3",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty()) { outs[0] = Value(); return; }
            const auto &Z = args[0];
            const size_t R = Z.dims().rows();
            const size_t C = Z.dims().cols();
            if (R == 0 || C == 0) { outs[0] = Value(); return; }

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

            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "bar3";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.zJson = zs.str();
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });

    // fill3(X, Y, Z[, C]) — emits raw 3-D vertex arrays. Each column
    // of (X, Y, Z) is one polygon (one polygon for vector inputs).
    // The WebGL renderer builds a triangle fan per polygon under
    // perspective.
    reg("bar", "fill3",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.size() < 3) { outs[0] = Value(); return; }
            const auto &X = args[0], &Y = args[1], &Z = args[2];
            const size_t R = X.dims().rows();
            const size_t C = std::max<size_t>(1, X.dims().cols());
            if (R == 0) { outs[0] = Value(); return; }
            if (Y.dims().rows() != R || Z.dims().rows() != R) {
                outs[0] = Value(); return;
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

            auto &fm = gc.fm;
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
            outs[0] = Value();
        });

    // ── Function-based plots (fplot / fcontour / fsurf / fmesh) ─────
    // Eval the user's function-handle on a sampled grid in C++ and
    // route the resulting matrices through the existing 2-D / 3-D
    // pipelines. Engine::callFunctionHandle works on both backends
    // (TW + VM) so the same body services either scripts.
    auto fplotImpl = [](Span<const Value> args, size_t nargout,
                        Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        if (args.empty() || !args[0].isFuncHandle()) {
            outs[0] = Value();
            return;
        }
        const Value &fh = args[0];
        double a = -5, b = 5;
        if (args.size() >= 2 && args[1].numel() >= 2) {
            a = args[1].doubleData()[0];
            b = args[1].doubleData()[1];
        }
        const int N = 200;
        auto *mr = gc.mr;

        std::ostringstream xs, ys;
        xs << '['; ys << '[';
        for (int i = 0; i < N; ++i) {
            const double x = a + (b - a) * i / (double)(N - 1);
            Value xv = Value::scalar(x, mr);
            std::array<Value, 1> argv{ xv };
            Value res;
            try {
                res = gc.callHandle(fh,
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

        auto &fm = gc.fm;
        fm.prepareForPlot();
        DatasetInfo ds;
        ds.type = "line";
        ds.xJson = xs.str();
        ds.yJson = ys.str();
        ds.style = "color=#1f77b4";
        fm.pushDataset(std::move(ds));
        fm.emitModified();
        outs[0] = Value();
    };
    reg("line", "fplot", fplotImpl);

    // fcontour / fsurf / fmesh — sample f(x, y) on a grid then route
    // through the existing contour / surf paths. We stitch a Z matrix
    // value via Value::matrix and proxy the call.
    auto sampleGrid = [](const Value &fh, double xa, double xb,
                          double ya, double yb, int N,
                          GraphicsContext &gc) {
        auto *mr = gc.mr;
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
                    Value res = gc.callHandle(fh,
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
                     Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty() || !args[0].isFuncHandle()) {
                outs[0] = Value();
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
            auto *mr = gc.mr;
            Value Z = sampleGrid(args[0], xa, xb, ya, yb, N, gc);
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
            if (!gc.callBuiltin("contour", Span<const Value>(proxied.data(), 3), 0,
                                Span<Value>(outBuf.data(), 1))) { outs[0] = Value(); return; }
            outs[0] = Value();
        });

    auto fSurfMeshImpl = [sampleGrid](Span<const Value> args, size_t nargout,
                                      Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        if (args.empty() || !args[0].isFuncHandle()) {
            outs[0] = Value();
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
        auto *mr = gc.mr;
        Value Z = sampleGrid(args[0], xa, xb, ya, yb, N, gc);
        Value Xv = Value::matrix(1, (size_t)N, ValueType::DOUBLE, mr);
        Value Yv = Value::matrix(1, (size_t)N, ValueType::DOUBLE, mr);
        for (int j = 0; j < N; ++j)
            Xv.doubleDataMut()[j] = xa + (xb - xa) * j / (double)(N - 1);
        for (int i = 0; i < N; ++i)
            Yv.doubleDataMut()[i] = ya + (yb - ya) * i / (double)(N - 1);
        std::array<Value, 3> proxied{ Xv, Yv, Z };
        std::array<Value, 1> outBuf;
        if (!gc.callBuiltin("surf", Span<const Value>(proxied.data(), 3), 0,
                            Span<Value>(outBuf.data(), 1))) { outs[0] = Value(); return; }
        outs[0] = Value();
    };
    reg("line", "fsurf", fSurfMeshImpl);
    reg("line", "fmesh", fSurfMeshImpl);

    // fplot3(funx, funy, funz [, [tmin tmax]]) — parametric 3-D curve.
    // Mirror of fplot but with 3 function handles evaluated against a
    // shared parameter t; emits a `plot3` dataset (xJson + yJson + zJson).
    reg("line", "fplot3",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.size() < 3
                || !args[0].isFuncHandle()
                || !args[1].isFuncHandle()
                || !args[2].isFuncHandle()) {
                outs[0] = Value();
                return;
            }
            const Value &fx = args[0];
            const Value &fy = args[1];
            const Value &fz = args[2];
            double a = -5, b = 5;
            if (args.size() >= 4 && args[3].numel() >= 2) {
                a = args[3].doubleData()[0];
                b = args[3].doubleData()[1];
            }
            const int N = 200;
            auto *mr = gc.mr;

            std::ostringstream xs, ys, zs;
            xs << '['; ys << '['; zs << '[';
            for (int i = 0; i < N; ++i) {
                const double t = a + (b - a) * i / (double)(N - 1);
                Value tv = Value::scalar(t, mr);
                std::array<Value, 1> argv{ tv };
                double x = std::nan(""), y = std::nan(""), z = std::nan("");
                try {
                    x = gc.callHandle(fx,
                        Span<const Value>(argv.data(), 1)).toScalar();
                    y = gc.callHandle(fy,
                        Span<const Value>(argv.data(), 1)).toScalar();
                    z = gc.callHandle(fz,
                        Span<const Value>(argv.data(), 1)).toScalar();
                } catch (...) { /* point becomes NaN — JSON null */ }
                if (i) { xs << ','; ys << ','; zs << ','; }
                if (std::isfinite(x)) xs << x; else xs << "null";
                if (std::isfinite(y)) ys << y; else ys << "null";
                if (std::isfinite(z)) zs << z; else zs << "null";
            }
            xs << ']'; ys << ']'; zs << ']';

            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "plot3";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.zJson = zs.str();
            ds.style = "color=#1f77b4";
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });

    // ── Streamlines — RK4 integration over a 2-D vector field ────────
    //   streamline(X, Y, U, V, sx, sy)    — explicit seed points
    //   streamslice(X, Y, U, V)           — auto 5×5 seed grid
    // Uniform grid is assumed (linspace-style X / Y). Bilinear interp
    // for (U, V) at integration points; integration stops when the
    // particle leaves the grid, hits a NaN cell, or stalls (|F| < eps).
    auto streamImpl = [](bool autoSlice,
                         Span<const Value> args, size_t nargout,
                         Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        if (args.size() < 4) { outs[0] = Value(); return; }
        const auto &Xv = args[0];
        const auto &Yv = args[1];
        const auto &Uv = args[2];
        const auto &Vv = args[3];
        const size_t Cx = Xv.numel();
        const size_t Ry = Yv.numel();
        if (Cx < 2 || Ry < 2) { outs[0] = Value(); return; }
        if (Uv.dims().rows() != Ry || Uv.dims().cols() != Cx) {
            outs[0] = Value(); return;
        }
        if (Vv.dims().rows() != Ry || Vv.dims().cols() != Cx) {
            outs[0] = Value(); return;
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
            if (args.size() < 6) { outs[0] = Value(); return; }
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

        auto &fm = gc.fm;
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
        outs[0] = Value();
    };
    reg("line", "streamline",
        [streamImpl](Span<const Value> args, size_t nargout,
                     Span<Value> outs, GraphicsContext &gc) {
            streamImpl(false, args, nargout, outs, gc);
        });
    reg("line", "streamslice",
        [streamImpl](Span<const Value> args, size_t nargout,
                     Span<Value> outs, GraphicsContext &gc) {
            streamImpl(true, args, nargout, outs, gc);
        });

    reg("line", "stem",
        [](Span<const Value> args, size_t nargout,
                                          Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "stem";
            size_t nvStart = parsePlotXYStyle(args, ds);
            parsePlotArgs(args, nvStart, ds);
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });

    reg("line", "stairs",
        [](Span<const Value> args, size_t nargout,
                                          Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "stairs";
            size_t nvStart = parsePlotXYStyle(args, ds);
            parsePlotArgs(args, nvStart, ds);
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });

    // errorbar(y, e)                — x = 1:N, symmetric e
    // errorbar(x, y, e)             — symmetric e
    // errorbar(x, y, neg, pos)      — asymmetric error bounds
    // errorbar(x, y, e, 'spec')     — symmetric, with line spec
    // errorbar(x, y, neg, pos, 'spec')
    // The dataset's xJson/yJson hold the centre points; eJson (sym) or
    // eNegJson + ePosJson (asym) hold the magnitudes. The renderer
    // draws vertical bars from y-eNeg to y+ePos with caps at each end.
    reg("line", "errorbar",
        [](Span<const Value> args, size_t nargout,
                                                  Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
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
            outs[0] = Value();
        });

    // ── Log-scale plot types — graphics.line ────────────────────────
    auto registerLogPlot = [&reg](
                                const char *name, const std::string &xscale,
                                const std::string &yscale) {
        reg("line", name,
            [xscale, yscale](
                Span<const Value> args, size_t nargout, Span<Value> outs,
                GraphicsContext &gc) {
                if (args.empty()) {
                    outs[0] = Value();
                    return;
                }
                auto &fm = gc.fm;
                fm.prepareForPlot();
                fm.currentAxes().xscale = xscale;
                fm.currentAxes().yscale = yscale;
                DatasetInfo ds;
                ds.type = "line";
                size_t nvStart = parsePlotXYStyle(args, ds);
                parsePlotArgs(args, nvStart, ds);
                fm.pushDataset(std::move(ds));
                fm.emitModified();
                outs[0] = Value();
            });
    };
    registerLogPlot("semilogx", "log", "linear");
    registerLogPlot("semilogy", "linear", "log");
    registerLogPlot("loglog", "log", "log");

    // ================================================================
    // Axes labels, limits, legend — graphics.layout
    // ================================================================

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

    // ================================================================
    // GUI no-ops (not yet implemented)
    // ================================================================

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

    // ────────────────────────────────────────────────────────────────
    // geoplot / geoscatter / geobubble — geographic plots without a
    // basemap. The basemap-tile path (Web Mercator + WMS fetch) is
    // BACKLOG; for v1 these route through plot/scatter with
    // (X = lon, Y = lat) so user scripts that target geographic
    // axes still produce the right scatter / line shape.
    // Forms supported (matching MATLAB):
    //   geoplot(lat, lon)
    //   geoplot(lat, lon, lineSpec)
    //   geoscatter(lat, lon[, sizes[, color]])
    //   geobubble(lat, lon, sizes)   — sizes drive marker radius
    // ────────────────────────────────────────────────────────────────
    auto geoForward = [](const char *target, Span<const Value> args,
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
    };
    reg("line", "geoplot",
        [geoForward](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            geoForward("plot", a, o, gc);
        });
    reg("bar", "geoscatter",
        [geoForward](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            geoForward("scatter", a, o, gc);
        });
    reg("bar", "geobubble",
        [geoForward](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            // bubblechart routing — scatter accepts sizes as its 3rd arg.
            geoForward("scatter", a, o, gc);
        });

    // ────────────────────────────────────────────────────────────────
    // bubblechart / bubblechart3 / swarmchart — variants of scatter
    // / scatter3 that emphasise size-modulated markers. We delegate
    // to the underlying scatter family; the size vector flows through
    // unchanged and the renderer applies it as marker radius.
    // ────────────────────────────────────────────────────────────────
    auto delegateTo = [](const char *target, Span<const Value> args,
                         Span<Value> outs, GraphicsContext &gc) {
        std::array<Value, 1> outBuf;
        if (!gc.callBuiltin(target, args, 0, Span<Value>(outBuf.data(), 1))) { outs[0] = Value(); return; }
        outs[0] = Value();
    };
    reg("bar", "bubblechart",
        [delegateTo](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            delegateTo("scatter", a, o, gc);
        });
    reg("bar", "bubblechart3",
        [delegateTo](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            delegateTo("scatter3", a, o, gc);
        });
    // swarmchart(x, y[, sizes[, color]]) — scatter with X-axis jitter
    // applied per unique categorical X. For each cluster of points
    // sharing the same X coordinate, points are spread along X within
    // a small interval; the spread amount scales with the cluster's
    // local rank (so the layout resembles a violin plot's swarm).
    reg("bar", "swarmchart",
        [](Span<const Value> args, size_t nargout,
           Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.size() < 2) { outs[0] = Value(); return; }
            const auto &xv = args[0];
            const auto &yv = args[1];
            const size_t n = std::min(xv.numel(), yv.numel());
            auto *mr = gc.mr;
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
            gc.callBuiltin("scatter", Span<const Value>(proxied.data(), proxied.size()), 0,
                           Span<Value>(outBuf.data(), 1));
            outs[0] = Value();
        });
    reg("bar", "swarmchart3",
        [delegateTo](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            delegateTo("scatter3", a, o, gc);
        });

    // comet(x, y) / comet3(x, y, z) — animated trail. Routes to plot
    // with a `cometAnim=1` hint in the style string; the IDE's
    // CompositePlot picks up the flag and animates the polyline
    // progressively via requestAnimationFrame (the final-state
    // figure is the full curve).
    reg("line", "comet",
        [delegateTo](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            delegateTo("plot", a, o, gc);
            // Stamp the just-pushed dataset with the animation hint.
            auto &fm = gc.fm;
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
        [delegateTo](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            delegateTo("plot3", a, o, gc);
            auto &fm = gc.fm;
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
    // triplot(TRI, X, Y [, LineSpec, ...]) — plot a triangulation.
    // TRI is M×3 (column-major) with 1-based vertex indices into X/Y.
    // Emits ONE `line` dataset with null-separated triangle loops:
    //   a → b → c → a → null → a' → b' → c' → a' → null …
    // so the entire triangulation is a single series in the figure
    // (one legend entry, one colour) instead of M independent series.
    // This matches MATLAB's triplot semantics — the canonical way to
    // visualise `delaunay(x, y)` output without exploding the series
    // count.
    // Optional 4th positional arg = LineSpec ("b-", "k--", etc). After
    // that, standard plot N-V pairs (LineWidth / MarkerSize) flow
    // through parsePlotArgs.
    // ────────────────────────────────────────────────────────────────
    reg("line", "triplot",
        [](Span<const Value> args, size_t /*nargout*/,
                        Span<Value> outs, GraphicsContext &gc) {
            if (args.size() < 3) { outs[0] = Value(); return; }
            const auto &TRI = args[0];
            const auto &xv  = args[1];
            const auto &yv  = args[2];
            const std::size_t M = TRI.dims().rows();
            const std::size_t N = xv.numel();
            if (yv.numel() != N || TRI.dims().cols() != 3 || M == 0) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
            fm.prepareForPlot();

            // Walk TRI column-major: row i, column k → T[k*M + i] (1-based).
            const double *T = TRI.doubleData();
            std::ostringstream xs, ys;
            xs << '['; ys << '[';
            bool first = true;
            for (std::size_t i = 0; i < M; ++i) {
                const std::size_t a = static_cast<std::size_t>(T[0 * M + i]) - 1;
                const std::size_t b = static_cast<std::size_t>(T[1 * M + i]) - 1;
                const std::size_t c = static_cast<std::size_t>(T[2 * M + i]) - 1;
                if (a >= N || b >= N || c >= N) continue;
                if (!first) { xs << ",null,"; ys << ",null,"; }
                first = false;
                xs << xv.elemAsDouble(a) << ',' << xv.elemAsDouble(b) << ','
                   << xv.elemAsDouble(c) << ',' << xv.elemAsDouble(a);
                ys << yv.elemAsDouble(a) << ',' << yv.elemAsDouble(b) << ','
                   << yv.elemAsDouble(c) << ',' << yv.elemAsDouble(a);
            }
            xs << ']'; ys << ']';

            DatasetInfo ds;
            ds.type = "line";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            std::size_t nvStart = 3;
            if (args.size() >= 4 && args[3].isChar()) {
                ds.style = args[3].toString();
                nvStart = 4;
            }
            parsePlotArgs(args, nvStart, ds);
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });

    // ────────────────────────────────────────────────────────────────
    // voronoi(x, y) — Voronoi diagram via Delaunay dual.
    // Algorithm:
    //   1. Compute brute-force Delaunay triangulation of the cloud
    //      (same logic as delaunay_reg in toolboxes/builtin).
    //   2. For each triangle, compute its circumcenter.
    //   3. For each pair of triangles sharing an edge, draw a line
    //      segment connecting their circumcenters — that's a Voronoi
    //      cell boundary edge.
    //   4. Emit as a single `line` dataset with null separators
    //      between segments, plus scatter markers at the input
    //      points.
    // Cells touching the convex hull don't have a finite second
    // endpoint (the cell is unbounded); v1 just omits those edges.
    // Properly-extended infinite rays are BACKLOG.
    // ────────────────────────────────────────────────────────────────
    reg("line", "voronoi",
        [delegateTo](Span<const Value> args, size_t nargout,
                     Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.size() < 2) { outs[0] = Value(); return; }
            const auto &xv = args[0];
            const auto &yv = args[1];
            const size_t n = xv.numel();
            if (yv.numel() != n || n < 3) { outs[0] = Value(); return; }
            std::vector<double> X(n), Y(n);
            for (size_t i = 0; i < n; ++i) {
                X[i] = xv.elemAsDouble(i);
                Y[i] = yv.elemAsDouble(i);
            }
            // Same Delaunay loop as toolboxes/builtin (kept local to avoid
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
            // Compute the input-points extent up-front — we need it
            // both for clipping unbounded ray endpoints and (later) for
            // setting xlim/ylim on the axes.
            double xMin = X[0], xMax = X[0], yMin = Y[0], yMax = Y[0];
            for (size_t i = 1; i < n; ++i) {
                if (X[i] < xMin) xMin = X[i];
                if (X[i] > xMax) xMax = X[i];
                if (Y[i] < yMin) yMin = Y[i];
                if (Y[i] > yMax) yMax = Y[i];
            }
            // 25% margin gives boundary cells visible "wedge" room.
            // Smaller margin (10%) made the rays look chopped at data
            // edge; larger gives the MATLAB-style bounded-but-airy view.
            const double xPad = std::max(0.1, (xMax - xMin) * 0.25);
            const double yPad = std::max(0.1, (yMax - yMin) * 0.25);
            const double clipXLo = xMin - xPad, clipXHi = xMax + xPad;
            const double clipYLo = yMin - yPad, clipYHi = yMax + yPad;

            // Build edge-to-triangles map. Each Delaunay edge is shared
            // by exactly 1 triangle (= convex-hull edge → unbounded
            // Voronoi cell, draw a ray) or 2 triangles (= interior
            // edge, draw the finite segment between their CCs).
            std::map<std::pair<size_t, size_t>, std::vector<size_t>> edge2tri;
            for (size_t t = 0; t < tris.size(); ++t) {
                const auto &T = tris[t];
                for (int e = 0; e < 3; ++e) {
                    size_t u = T[e], v = T[(e + 1) % 3];
                    if (u > v) std::swap(u, v);
                    edge2tri[{u, v}].push_back(t);
                }
            }
            // We DON'T pre-clip emitted segments to the data bbox: rays
            // need to extend out to the visible axis frame so unbounded
            // cells look terminated, not chopped short. xlim/ylim below
            // pin the axis and the IDE's SVG clipPath does the actual
            // boundary clipping per pixel.
            std::ostringstream xs, ys;
            xs << '['; ys << '[';
            bool first = true;
            const auto emit = [&](double xa, double ya, double xb, double yb) {
                if (!first) { xs << ",null,"; ys << ",null,"; }
                first = false;
                xs << xa << ',' << xb;
                ys << ya << ',' << yb;
            };

            for (auto &kv : edge2tri) {
                const size_t u = kv.first.first, v = kv.first.second;
                const auto &neighbors = kv.second;
                if (neighbors.size() == 2) {
                    // Interior edge — bounded segment between two CCs.
                    const auto &A = ccs[neighbors[0]];
                    const auto &B = ccs[neighbors[1]];
                    emit(A.x, A.y, B.x, B.y);
                } else if (neighbors.size() == 1) {
                    // Hull edge — unbounded ray from the CC, perpendicular
                    // to the edge, going AWAY from the third triangle vertex.
                    const size_t t = neighbors[0];
                    const auto &T = tris[t];
                    const size_t third = (T[0] != u && T[0] != v) ? T[0]
                                       : (T[1] != u && T[1] != v) ? T[1] : T[2];
                    const double ex = X[v] - X[u], ey = Y[v] - Y[u];
                    // Perpendicular candidate (one of two normals).
                    double nx = -ey, ny = ex;
                    // Make it point AWAY from the third vertex.
                    const double midX = 0.5 * (X[u] + X[v]);
                    const double midY = 0.5 * (Y[u] + Y[v]);
                    const double tx = X[third] - midX;
                    const double ty = Y[third] - midY;
                    if (nx * tx + ny * ty > 0) { nx = -nx; ny = -ny; }
                    // Extend the ray FAR — clipSeg will trim to the bbox.
                    const double far = (clipXHi - clipXLo) + (clipYHi - clipYLo);
                    const double nlen = std::sqrt(nx * nx + ny * ny);
                    if (nlen < 1e-15) continue;
                    const double endX = ccs[t].x + (nx / nlen) * far;
                    const double endY = ccs[t].y + (ny / nlen) * far;
                    emit(ccs[t].x, ccs[t].y, endX, endY);
                }
            }
            xs << ']'; ys << ']';

            auto &fm = gc.fm;
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
            {
                const bool wasHold = fm.currentAxes().holdOn;
                fm.currentAxes().holdOn = true;
                gc.callBuiltin("scatter", Span<const Value>(ptArgs.data(), 2), 0,
                               Span<Value>(outBuf.data(), 1));
                fm.currentAxes().holdOn = wasHold;
            }
            // Clamp axis to the data extent (computed above as
            // clipX/Y Lo/Hi). Same range we used for ray clipping —
            // both come from the input-points bbox plus 10% margin.
            std::ostringstream xlim, ylim;
            xlim << '[' << clipXLo << ',' << clipXHi << ']';
            ylim << '[' << clipYLo << ',' << clipYHi << ']';
            fm.currentAxes().xlimJson = xlim.str();
            fm.currentAxes().ylimJson = ylim.str();
            // scatter() above already called emitModified, clearing the
            // modified flag. Re-flag so the JSON re-emits with our xlim.
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });

    // heatmap(C) — table-style heatmap. Same data layout as imagesc:
    // 2-D matrix of values mapped through a colormap. Optional row/
    // column label strings ('RowNames' / 'ColumnNames') are accepted
    // but currently flow through as labels only — full table-style
    // heatmap with cell-text overlays + row/col headers is BACKLOG.
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
        [delegateTo, pushCellText](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
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
                       Span<const Value>(proxied.data(), proxied.size()), o, gc);
            if (!cmap.empty()) {
                gc.fm.currentAxes().colormapName = cmap;
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
                    auto &fm = gc.fm;
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
            gc.fm.current().modified = true;
            gc.fm.emitModified();
        });

    // confusionchart(C [, classNames]) — confusion matrix heatmap
    // with cell-value labels. Same as heatmap(C) but adds explicit
    // axis labels and (when classNames given) categorical tick
    // labels on both axes.
    reg("bar", "confusionchart",
        [](Span<const Value> args, size_t, Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) { outs[0] = Value(); return; }
            // Forward to heatmap (cellLabel default on, no Colormap).
            std::array<Value, 1> outBuf;
            std::array<Value, 1> proxied{ args[0] };
            if (!gc.callBuiltin("heatmap", Span<const Value>(proxied.data(), 1), 0,
                                Span<Value>(outBuf.data(), 1))) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
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
                    Value elem = cn.elemAt(i, gc.mr);
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
            outs[0] = Value();
        });

    // parallelplot — parallel-coordinates plot. v1 routes each row of
    // the input matrix as one line plot across N axis indices; the
    // dedicated multi-axis "parallel" rendering is BACKLOG.
    reg("line", "parallelplot",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty()) { outs[0] = Value(); return; }
            const auto &T = args[0];
            const size_t R = T.dims().rows();
            const size_t C = T.dims().cols();
            if (R == 0 || C == 0) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
            fm.prepareForPlot();
            // Hold on so all rows pile in one figure.
            const bool wasHold = fm.currentAxes().holdOn;
            fm.currentAxes().holdOn = true;
            auto *mr = gc.mr;
            auto xv = Value::matrix(1, C, ValueType::DOUBLE, mr);
            for (size_t c = 0; c < C; ++c) xv.doubleDataMut()[c] = (double)(c + 1);
            for (size_t r = 0; r < R; ++r) {
                auto yv = Value::matrix(1, C, ValueType::DOUBLE, mr);
                for (size_t c = 0; c < C; ++c)
                    yv.doubleDataMut()[c] = T.elemAsDouble(c * R + r);
                std::array<Value, 2> proxied{ xv, yv };
                std::array<Value, 1> outBuf;
                gc.callBuiltin("plot", Span<const Value>(proxied.data(), 2), 0,
                               Span<Value>(outBuf.data(), 1));
            }
            fm.currentAxes().holdOn = wasHold;
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
                      Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty() || !args[0].numel()) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
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
            outs[0] = Value();
        };
    };
    reg("line", "xline", refLineImpl("xline"));
    reg("line", "yline", refLineImpl("yline"));
}

} // namespace numkit
