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
#include <cstdio>
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
            DatasetInfo ds;
            ds.type = "bar";
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
            fm.pushDataset(std::move(ds));
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
            if (fm.currentAxes().axisMode.empty()) {
                fm.currentAxes().axisMode = "ij";
            }
            fm.emitModified();
            outs[0] = Value::empty();
    };  // heatmapImpl

    using namespace std::placeholders;
    reg("image", "imagesc", std::bind(heatmapImpl, "imagesc", _1, _2, _3, _4));
    reg("image", "pcolor",  std::bind(heatmapImpl, "pcolor",  _1, _2, _3, _4));

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
    // contourf — same body for now (filled regions deferred). MATLAB
    // users accept that we emit lines instead of filled bands; the
    // shape of the data is preserved.
    reg("contour", "contourf", contourImpl);

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

    reg("layout", "axes", noop_ret1);
    reg("layout", "gca", noop_ret1);
    reg("layout", "gcf", noop_ret1);
    reg("layout", "cla", noop);

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
    reg("line", "xline", noop);
    reg("line", "yline", noop);
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
