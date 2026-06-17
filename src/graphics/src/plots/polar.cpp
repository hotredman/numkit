// numkit/graphics — polar.cpp
//
// graphics.polar.* builders (polarplot / polarscatter / polarhistogram / rlim / theta* / r*), carved out of plots.cpp. Core-free bodies (GraphicsContext); shared
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

void buildPolarPlots(std::vector<PlotEntry> &table)
{
    using namespace detail;
    auto reg = [&](const char *sub, const char *name, GraphicsFn fn) {
        table.push_back(PlotEntry{sub, name, /*core=*/false, std::move(fn)});
    };

    // compass(U, V) — vector arrows from the origin on a POLAR
    // coordinate system. Each (U_i, V_i) becomes an arrow with
    // tail at origin and head at (theta_i, rho_i) where
    // theta = atan2(V, U) and rho = hypot(U, V). MATLAB-equivalent.
    //   compass(Z)        — Z complex; (U, V) = (real(Z), imag(Z))
    //   compass(U, V)     — explicit pair
    //   compass(..., spec) — optional LineSpec string
    // Wire format: type="compass" dataset on a polar axes — the
    // renderer (PolarPlot.jsx) treats xJson as theta, yJson as rho.
    reg("polar", "compass",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
            fm.prepareForPlot();
            fm.currentAxes().polar = true;

            // Build polar (theta, rho) pairs from (U, V).
            std::ostringstream tx, ry;
            tx << '['; ry << '[';
            const auto emit = [&](double u, double v, bool first) {
                if (!first) { tx << ','; ry << ','; }
                tx << std::atan2(v, u);
                ry << std::hypot(u, v);
            };
            size_t N = 0;
            size_t nvStart = 1;
            const Value &A = args[0];
            if (args.size() == 1 && A.isComplex()) {
                N = A.numel();
                for (size_t i = 0; i < N; ++i) {
                    const auto z = A.complexData()[i];
                    emit(z.real(), z.imag(), i == 0);
                }
            } else if (args.size() >= 2 && args[1].numel() >= A.numel()) {
                N = A.numel();
                for (size_t i = 0; i < N; ++i)
                    emit(A.doubleData()[i], args[1].doubleData()[i], i == 0);
                nvStart = 2;
            } else {
                outs[0] = Value();
                return;
            }
            tx << ']'; ry << ']';

            DatasetInfo ds;
            ds.type  = "compass";   // polar-renderer dispatch key
            ds.xJson = tx.str();
            ds.yJson = ry.str();
            if (args.size() > nvStart && args[nvStart].isChar()) {
                ds.style = args[nvStart].toString();
            }
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });
    // ── Polar — graphics.polar ───────────────────────────────────────
    reg("polar", "polarplot",
        [](Span<const Value> args, size_t nargout,
                                   Span<Value> outs, GraphicsContext &gc) {
            if (args.size() < 2) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
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
            outs[0] = Value();
        });
    // polarscatter(theta, rho) — markers at each (θ, ρ) on the polar
    // axes. Same wire format as polarplot but type='scatter' so the
    // PolarPlot renderer draws circles instead of polylines.
    reg("polar", "polarscatter",
        [](Span<const Value> args, size_t nargout,
                                   Span<Value> outs, GraphicsContext &gc) {
            if (args.size() < 2) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
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
            outs[0] = Value();
        });
    // Shared helper for the two angular-histogram builtins. Bins
    // theta values into `nbins` sectors of width 2π/nbins over
    // [0, 2π), wraps negative/out-of-range inputs, and emits the
    // bin centres + counts as two JSON arrays — `tx` (theta) and
    // `ty` (count). Used by polarhistogram (type="bar") and the
    // legacy rose (type="rose"); the only difference is the
    // dataset `type` tag and the default bin count.
    auto computeAngularHistogram = [](const Value &theta, int nbins,
                                      std::string &txOut, std::string &tyOut) {
        const double TAU = 2 * 3.14159265358979323846;
        const double bw  = TAU / nbins;
        std::vector<double> counts(nbins, 0.0);
        const size_t N = theta.numel();
        for (size_t i = 0; i < N; ++i) {
            double t = theta.doubleData()[i];
            if (!std::isfinite(t)) continue;
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
        txOut = tx.str();
        tyOut = ty.str();
    };
    // polarhistogram(theta[, nbins]) — bins θ values into nbins angular
    // sectors over [0, 2π) and emits a polar bar dataset where each
    // bin centre carries its count. PolarPlot renders the bars as
    // wedges from the origin.
    reg("polar", "polarhistogram",
        [computeAngularHistogram](Span<const Value> args, size_t nargout,
                                  Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) { outs[0] = Value(); return; }
            int nbins = (args.size() >= 2 && args[1].numel() == 1)
                        ? std::max(1, (int)args[1].toScalar())
                        : 36;   // 10° bins by default
            std::string tx, ty;
            computeAngularHistogram(args[0], nbins, tx, ty);
            auto &fm = gc.fm;
            fm.prepareForPlot();
            fm.currentAxes().polar = true;
            DatasetInfo ds;
            ds.type = "bar";
            ds.xJson = std::move(tx);
            ds.yJson = std::move(ty);
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });
    // polarbubblechart(theta, rho, [sz], [c]) — scatter on polar
    // axes with per-point marker size + optional per-point colour.
    //   sz : scalar (broadcast) | vector (per-point), points^2
    //   c  : one of —
    //          • RGB row [r g b]   (single colour for all points)
    //          • RGB matrix N-by-3 (per-point RGB triplets)
    //          • vector length N   (colormap data — index into
    //            the active colormap; renderer maps to RGB)
    // Wire format: type="bubble", size column in sizeJson, colour
    // (if any) in colorJson. NEITHER lands in zJson — that's
    // reserved for actual z-coordinates / matrices (imagesc etc.).
    reg("polar", "polarbubblechart",
        [](Span<const Value> args, size_t nargout,
                                   Span<Value> outs, GraphicsContext &gc) {
            if (args.size() < 2) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
            fm.prepareForPlot();
            fm.currentAxes().polar = true;
            DatasetInfo ds;
            ds.type  = "bubble";
            ds.xJson = vecToJson(args[0]);   // theta
            ds.yJson = vecToJson(args[1]);   // rho
            // Size column.
            ds.sizeJson = (args.size() >= 3) ? vecToJson(args[2]) : "[36]";
            // Optional colour column. RGB-matrix shape (N-by-3) is
            // emitted as a nested JSON array so the renderer can
            // distinguish "one colour for all" (1-by-3) from
            // "per-point" (N-by-3 or N-by-1 colormap index).
            if (args.size() >= 4 && !args[3].isChar()) {
                const Value &c = args[3];
                const size_t R = c.dims().rows();
                const size_t C = c.dims().cols();
                if (R > 0 && C == 3) {
                    // RGB matrix — emit as [[r,g,b], [r,g,b], ...].
                    // MATLAB stores column-major, so row i of an
                    // R×3 matrix is at linear indices i, i+R, i+2R.
                    std::ostringstream cs;
                    cs << '[';
                    for (size_t i = 0; i < R; ++i) {
                        if (i) cs << ',';
                        cs << '['
                           << c.doubleData()[i + 0 * R] << ','
                           << c.doubleData()[i + 1 * R] << ','
                           << c.doubleData()[i + 2 * R] << ']';
                    }
                    cs << ']';
                    ds.colorJson = cs.str();
                } else {
                    // Scalar / vector → colormap index data.
                    ds.colorJson = vecToJson(c);
                }
            }
            // LineSpec etc. as a trailing string argument.
            parsePlotArgs(args, (args.size() >= 4 && !args[3].isChar()) ? 4 : 3, ds);
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });
    // rose(theta[, nbins]) — angular histogram, the classic MATLAB
    // function (deprecated in favour of polarhistogram but kept for
    // legacy code). Same bin counting as polarhistogram (shared
    // helper); tagged type="rose" so a future renderer can give it
    // the classic wedges-from-origin look. Default nbins=20 matches
    // MATLAB (polarhistogram defaults to 36).
    reg("polar", "rose",
        [computeAngularHistogram](Span<const Value> args, size_t nargout,
                                  Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) { outs[0] = Value(); return; }
            int nbins = (args.size() >= 2 && args[1].numel() == 1)
                        ? std::max(1, (int)args[1].toScalar())
                        : 20;
            std::string tx, ty;
            computeAngularHistogram(args[0], nbins, tx, ty);
            auto &fm = gc.fm;
            fm.prepareForPlot();
            fm.currentAxes().polar = true;
            DatasetInfo ds;
            ds.type  = "rose";
            ds.xJson = std::move(tx);
            ds.yJson = std::move(ty);
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });
    // ── Polar-specific settings — graphics.polar ─────────────────────
    reg("polar", "rlim",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty() && args[0].numel() >= 2) {
                auto &fm = gc.fm;
                fm.currentAxes().rlimJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    reg("polar", "thetalim",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty() && args[0].numel() >= 2) {
                auto &fm = gc.fm;
                fm.currentAxes().thetalimJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    reg("polar", "thetadir",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty() && args[0].isChar()) {
                auto &fm = gc.fm;
                fm.currentAxes().thetaDir = args[0].toString();
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    reg("polar", "thetazero",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty() && args[0].isChar()) {
                auto &fm = gc.fm;
                fm.currentAxes().thetaZeroLocation = args[0].toString();
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    // MATLAB calls the property ThetaZeroLocation; the convenience
    // function we have is `thetazero`, but accepting the longer name
    // as an alias keeps copy-pasted MATLAB scripts working.
    reg("polar", "thetazerolocation",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (!args.empty() && args[0].isChar()) {
                auto &fm = gc.fm;
                fm.currentAxes().thetaZeroLocation = args[0].toString();
                fm.current().modified = true;
                fm.emitModified();
            }
            outs[0] = Value();
        });
    // thetaticks(degrees) — set theta tick positions on the polar
    // axes. Argument is a vector of DEGREES (MATLAB convention).
    // Without args (or empty arg) the user clears custom ticks and
    // the renderer falls back to its default 30°-spaced grid.
    reg("polar", "thetaticks",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto &fm = gc.fm;
            fm.currentAxes().thetaticksJson =
                (args.empty() || args[0].numel() == 0) ? "" : vecToJson(args[0]);
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    // rticks(values) — set radial tick positions in r-axis units.
    // Empty arg = clear (auto). Default rticks are picked by
    // niceStep in the renderer.
    reg("polar", "rticks",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto &fm = gc.fm;
            fm.currentAxes().rticksJson =
                (args.empty() || args[0].numel() == 0) ? "" : vecToJson(args[0]);
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    // Helper: build a JSON array of strings from a Value that's
    // either a cell of chars or a single char (one label). Used by
    // thetaticklabels / rticklabels.
    auto strArrToJson = [](const Value &v) -> std::string {
        std::ostringstream os;
        os << '[';
        auto escape = [&](const std::string &s) {
            os << '"';
            for (char c : s) {
                if (c == '"' || c == '\\') os << '\\' << c;
                else if (c == '\n')        os << "\\n";
                else                       os << c;
            }
            os << '"';
        };
        if (v.isCell()) {
            const size_t N = v.numel();
            for (size_t i = 0; i < N; ++i) {
                if (i) os << ',';
                const Value &el = v.cellAt(i);
                escape(el.isChar() ? el.toString() : std::string{});
            }
        } else if (v.isChar()) {
            // Single label — wrap in 1-element array.
            escape(v.toString());
        }
        os << ']';
        return os.str();
    };
    // thetaticklabels(labels) — set label text for theta ticks.
    // `labels` is a cell array of char vectors (or a single char
    // for one tick). Renderer falls back to numeric degrees when
    // the labels array length doesn't match the tick count.
    reg("polar", "thetaticklabels",
        [strArrToJson](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto &fm = gc.fm;
            fm.currentAxes().thetaticklabelsJson =
                args.empty() ? "" : strArrToJson(args[0]);
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
    reg("polar", "rticklabels",
        [strArrToJson](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto &fm = gc.fm;
            fm.currentAxes().rticklabelsJson =
                args.empty() ? "" : strArrToJson(args[0]);
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
}

}  // namespace numkit
