// numkit/graphics — contour.cpp
//
// graphics.contour.* builders (contour / contourf), carved out of plots.cpp. Core-free bodies (GraphicsContext); shared
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

void buildContourPlots(std::vector<PlotEntry> &table)
{
    using namespace detail;
    auto reg = [&](const char *sub, const char *name, GraphicsFn fn) {
        table.push_back(PlotEntry{sub, name, /*core=*/false, std::move(fn)});
    };

    // ── Contour — marching squares over Z(R, C) ────────────────────────
    // contour(Z) / contour(Z, n) / contour(Z, levels)
    // contour(X, Y, Z[, n|levels])
    // We don't reuse the imagesc heatmap path — contour produces 1-D
    // line layers instead of a 2-D raster. Each level becomes its own
    // DatasetInfo with type='line' and an inline color (HSL→RGB ramp
    // through the z-extent), with NaN separators between segments so
    // the existing line renderer can draw them as one path.
    auto contourImpl = [](Span<const Value> args, size_t nargout,
                          Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        auto &fm = gc.fm;
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
        if (!Z_arg) { outs[0] = Value(); return; }

        const size_t R = Z_arg->dims().rows();
        const size_t C = Z_arg->dims().cols();
        if (R < 2 || C < 2) { outs[0] = Value(); return; }

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
        outs[0] = Value();
    };
    reg("contour", "contour", contourImpl);
    // ────────────────────────────────────────────────────────────────
    // contourf — filled bands between consecutive levels.
    // Strategy: for each level L from highest to lowest, draw the
    // closed polygon "Z >= L" with a colour from the colormap at that
    // level. Because levels are drawn in descending order each layer
    // overdraws a smaller region of the previous one, producing the
    // classic MATLAB filled-contour banded effect.
    // Per cell with values (v_TL, v_TR, v_BR, v_BL) and the four
    // corner-points (TL, TR, BR, BL), we compute a 4-bit bitmask of
    // "corner is inside (z >= L)" and look up the polygon vertices
    // from a 16-entry table. Saddle codes (5, 10) are split into two
    // disjoint triangles — visually equivalent to MATLAB's
    // disambiguation in our usage (the renderer fills the union).
    // ────────────────────────────────────────────────────────────────
    auto contourfImpl = [](Span<const Value> args, size_t nargout,
                           Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        auto &fm = gc.fm;
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
        if (!Z_arg) { outs[0] = Value(); return; }

        const size_t R = Z_arg->dims().rows();
        const size_t C = Z_arg->dims().cols();
        if (R < 2 || C < 2) { outs[0] = Value(); return; }

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
        outs[0] = Value();
    };
    reg("contour", "contourf", contourfImpl);
}

}  // namespace numkit
