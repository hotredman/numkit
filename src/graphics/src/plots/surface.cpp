// numkit/graphics — surface.cpp
//
// graphics.surface.* builders (surf / mesh / slice / isosurface / coneplot / streamtube / waterfall / lighting), carved out of plots.cpp. Core-free bodies (GraphicsContext); shared
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

void buildSurfacePlots(std::vector<PlotEntry> &table)
{
    using namespace detail;
    auto reg = [&](const char *sub, const char *name, GraphicsFn fn) {
        table.push_back(PlotEntry{sub, name, /*core=*/false, std::move(fn)});
    };

    // ── Surf / mesh — wireframe quad mesh under cabinet projection ──
    // surf(Z) / surf(X, Y, Z); mesh(...) shares the same body. Real
    // face shading + lighting is deferred; for now we emit the wire
    // skeleton (rows + cols) as plot3-style line segments and let the
    // existing cabinet projection in the JS adapter render it.
    auto surfImpl = [](const char *typeName,
                       Span<const Value> args, size_t nargout,
                       Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        auto &fm = gc.fm;
        fm.prepareForPlot();

        const Value *Z_arg = nullptr;
        const Value *X_arg = nullptr;
        const Value *Y_arg = nullptr;
        if (args.size() == 1) {
            Z_arg = &args[0];
        } else if (args.size() >= 3) {
            X_arg = &args[0]; Y_arg = &args[1]; Z_arg = &args[2];
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
            outs[0] = Value();
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
        outs[0] = Value();
    };
    {
        using namespace std::placeholders;
        reg("surface", "surf", std::bind(surfImpl, "surf", _1, _2, _3, _4));
        reg("surface", "mesh", std::bind(surfImpl, "mesh", _1, _2, _3, _4));
    }
    // ────────────────────────────────────────────────────────────────
    // slice — axis-aligned cross sections of a 3-D scalar volume.
    // Forms supported:
    //   slice(V, sx, sy, sz)           — V is M×N×P
    //   slice(X, Y, Z, V, sx, sy, sz)  — explicit grid coords
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
                        Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        auto &fm = gc.fm;
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
            outs[0] = Value();
            return;
        }
        if (!V || V->dims().ndims() != 3) {
            outs[0] = Value();
            return;
        }

        const size_t M = V->dims().rows();
        const size_t N = V->dims().cols();
        const size_t P = V->dims().pages();
        if (M < 2 || N < 2 || P < 2) {
            outs[0] = Value();
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
        outs[0] = Value();
    };
    reg("surface", "slice", sliceImpl);
    // ────────────────────────────────────────────────────────────────
    // isosurface(V, isovalue) — marching cubes.
    // Forms supported:
    //   isosurface(V, iso)
    //   isosurface(X, Y, Z, V, iso)
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
                             Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        auto &fm = gc.fm;
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
            outs[0] = Value();
            return;
        }

        const size_t M = V->dims().rows();
        const size_t N = V->dims().cols();
        const size_t P = V->dims().pages();
        if (M < 2 || N < 2 || P < 2) { outs[0] = Value(); return; }

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
        outs[0] = Value();
    };
    reg("surface", "isosurface", isosurfaceImpl);
    // ────────────────────────────────────────────────────────────────
    // coneplot — cone-headed arrows over a 3-D vector field.
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
    // Each cone is a 6-sided pyramid (apex + 6-vertex base ring) →
    // 6 side triangles + 4 cap triangles = 10 triangles per cone,
    // emitted into a single fill3 dataset (null-separated triangles).
    // ────────────────────────────────────────────────────────────────
    auto coneplotImpl = [](Span<const Value> args, size_t nargout,
                           Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        auto &fm = gc.fm;
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
            outs[0] = Value();
            return;
        }
        if (!U || !Vv || !W) { outs[0] = Value(); return; }
        const auto &dims = U->dims();
        if (dims.ndims() != 3) { outs[0] = Value(); return; }
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
        if (cones.empty()) { outs[0] = Value(); return; }

        // Auto-scale: largest vector magnitude in the field divided
        // by 0.4 × the smallest grid spacing → cones don't overlap.
        double maxMag = 0;
        for (const auto &c : cones) {
            const double m = std::sqrt(c.u * c.u + c.v * c.v + c.w * c.w);
            if (m > maxMag) maxMag = m;
        }
        if (maxMag <= 0) { outs[0] = Value(); return; }
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
        outs[0] = Value();
    };
    reg("surface", "coneplot", coneplotImpl);
    // ────────────────────────────────────────────────────────────────
    // streamtube — tubes wrapped around streamlines through a 3-D
    // vector field. Forms supported:
    //   streamtube(X, Y, Z, U, V, W, sx, sy, sz)
    //   streamtube(U, V, W, sx, sy, sz)         — implicit grid
    // Per seed point, integrate the field with fixed-step Euler (RK1)
    // forward. Tube generated with an N-vertex ring around each
    // streamline sample; radii proportional to local |V| with a hard
    // cap of the grid spacing. Output: one fill3 dataset per
    // streamline, triangle list (quads split into two triangles).
    // ────────────────────────────────────────────────────────────────
    auto streamtubeImpl = [](Span<const Value> args, size_t nargout,
                             Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        auto &fm = gc.fm;
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
            outs[0] = Value();
            return;
        }
        if (!U || U->dims().ndims() != 3) {
            outs[0] = Value();
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
        outs[0] = Value();
    };
    reg("surface", "streamtube", streamtubeImpl);
    // contour3(Z[, n|levels]) — contour lines on the surface defined
    // by Z. Same algorithm as 2-D contour but each segment carries the
    // Z value of the level so it can be drawn at the surface height.
    // Wire format: type='contour3' with X, Y vectors, Z-matrix, plus
    // a separate `levels` style key.
    reg("surface", "contour3",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty()) { outs[0] = Value(); return; }
            const Value *Z_arg = nullptr;
            const Value *levels_arg = nullptr;
            if (args.size() == 1) Z_arg = &args[0];
            else if (args.size() >= 2) {
                Z_arg = &args[0];
                levels_arg = &args[1];
            }
            if (!Z_arg) { outs[0] = Value(); return; }
            const size_t R = Z_arg->dims().rows();
            const size_t C = Z_arg->dims().cols();
            if (R < 2 || C < 2) { outs[0] = Value(); return; }

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

            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "contour3";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.zJson = zs.str();
            ds.style = sty.str();
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });
    // surfc(Z) / meshc(Z) — surf/mesh + contour3 in a single figure.
    // Implemented as wrappers that invoke compat.surf (or compat.mesh)
    // followed by compat.contour3. hold on between calls so both
    // datasets land on the same axes.
    auto surfMeshContour = [](const char *base,
                              Span<const Value> args, size_t nargout,
                              Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        std::array<Value, 1> tmp;
        if (!gc.callBuiltin(base, args, 0, Span<Value>(tmp.data(), 1))) { outs[0] = Value(); return; }
        // Hold on so contour3 doesn't clear the axes.
        gc.fm.currentAxes().holdOn = true;
        gc.callBuiltin("contour3", args, 0, Span<Value>(tmp.data(), 1));
        gc.fm.currentAxes().holdOn = false;
        outs[0] = Value();
    };
    reg("surface", "surfc", [surfMeshContour](Span<const Value> a, size_t n, Span<Value> o, GraphicsContext &gc) {
        surfMeshContour("surf", a, n, o, gc);
    });
    reg("surface", "meshc", [surfMeshContour](Span<const Value> a, size_t n, Span<Value> o, GraphicsContext &gc) {
        surfMeshContour("mesh", a, n, o, gc);
    });
    // waterfall(Z) — row-by-row 3-D ribbons. Emits the same Z-matrix
    // wire format as surf/bar3; the WebGL renderer builds per-row
    // ribbons (row Z values down to baseline z=0).
    reg("surface", "waterfall",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty()) { outs[0] = Value(); return; }
            const auto &Z = args[0];
            const size_t R = Z.dims().rows();
            const size_t C = Z.dims().cols();
            if (R < 1 || C < 2) { outs[0] = Value(); return; }

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
            ds.type = "waterfall";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.zJson = zs.str();
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });
    // camlight(['left'|'right'|'headlight']) — adds a directional
    // light positioned relative to the camera. Default: headlight
    // (light from camera).
    reg("surface", "camlight",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            auto &fm = gc.fm;
            std::string pos = "headlight";
            if (!args.empty() && args[0].isChar()) {
                std::string s = args[0].toString();
                for (auto &c : s) c = (char)std::tolower((unsigned char)c);
                if (s == "left" || s == "right" || s == "headlight") pos = s;
            }
            fm.currentAxes().camlightPos = pos;
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    // lighting('flat'|'gouraud'|'phong'|'none') — material shading
    // model. Default 'gouraud' (smooth Lambert), 'flat' uses per-face
    // normals, 'phong' adds specular highlights, 'none' removes
    // shading entirely.
    reg("surface", "lighting",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            auto &fm = gc.fm;
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
            outs[0] = Value();
        });
    // material('shiny'|'metal'|'dull') — preset specular response
    // (only meaningful with lighting='phong').
    reg("surface", "material",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            auto &fm = gc.fm;
            std::string m;
            if (!args.empty() && args[0].isChar()) {
                std::string s = args[0].toString();
                for (auto &c : s) c = (char)std::tolower((unsigned char)c);
                if (s == "shiny" || s == "metal" || s == "dull") m = s;
            }
            fm.currentAxes().materialPreset = m;
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    // surfl(Z[, X, Y]) — surf + auto camlight + lighting gouraud.
    // Implemented as a thin wrapper that calls compat.surf followed by
    // setting camlightPos = 'headlight' and lightingMode = 'gouraud'
    // on the resulting axes.
    reg("surface", "surfl",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            std::array<Value, 1> outBuf;
            if (!gc.callBuiltin("surf", args, 0, Span<Value>(outBuf.data(), 1))) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
            fm.currentAxes().camlightPos = "headlight";
            fm.currentAxes().lightingMode = "gouraud";
            fm.currentAxes().materialPreset = "shiny";
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
}

}  // namespace numkit
