// numkit/graphics — line.cpp
//
// graphics.line.* builders (plot / plot3 / stem / stairs / area / errorbar / semilog* / fplot / comet / geoplot / xline), carved out of plots.cpp. Core-free bodies (GraphicsContext); shared
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

void buildLinePlots(std::vector<PlotEntry> &table)
{
    using namespace detail;
    auto reg = [&](const char *sub, const char *name, GraphicsFn fn) {
        table.push_back(PlotEntry{sub, name, /*core=*/false, std::move(fn)});
    };

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
    reg("line", "geoplot",
        [](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            geoForward("plot", a, o, gc);
        });
    // comet(x, y) / comet3(x, y, z) — animated trail. Routes to plot
    // with a `cometAnim=1` hint in the style string; the IDE's
    // CompositePlot picks up the flag and animates the polyline
    // progressively via requestAnimationFrame (the final-state
    // figure is the full curve).
    reg("line", "comet",
        [](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
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
        [](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
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
        [](Span<const Value> args, size_t nargout,
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

}  // namespace numkit
