/// @file figure_render.hpp
/// @ingroup group_graphics
///
/// Engine-side figure renderer — the software path behind
/// saveas/exportgraphics/print (portion 32). Produces a pixel-space
/// DISPLAY LIST from a FigureState, plus two backends:
///   • renderFigureSvg  — vector SVG (text as <text>)
///   • renderFigureRgb  — rasterised RGB buffer (built-in 5×7 bitmap
///     font, Bresenham lines, scanline polygon fill)
///
/// Deliberately STL-only and Value-free (same contract as the rest of
/// the figure layer); the bundle's export_reg.cpp turns the RGB buffer
/// into PNG bytes via the image toolbox and writes through the fs.
///
/// Scope (portion 32): vector chart types — line, stairs, stem, scatter,
/// bar, barh, area, errorbar — on linear/log axes with grid, limits,
/// tick formats, dir reverse, subplot tiling, legend, sgtitle.
/// NOT yet rendered: 3-D surfaces/contours, imagesc/heatmap (colormap
/// port), polar, text overlays, yyaxis right-scale, annotations —
/// tracked in bugs/opened/graphics/handle-surface-gaps.md sibling
/// export-gaps file.
#pragma once

#include <numkit/figure/figure_manager.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace numkit {
namespace figrender {

// ── Colour helpers ───────────────────────────────────────────────────
struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
    Rgb() = default;
    Rgb(std::uint8_t rr, std::uint8_t gg, std::uint8_t bb)
        : r(rr), g(gg), b(bb) {}
    static Rgb fromHex(const std::string &hex)
    {
        if (hex.size() >= 7 && hex[0] == '#') {
            auto h = [&](size_t i) {
                return static_cast<std::uint8_t>(std::strtoul(hex.substr(i, 2).c_str(), nullptr, 16));
            };
            return Rgb(h(1), h(3), h(5));
        }
        return Rgb(0, 0, 0);
    }
    std::string hex() const
    {
        char buf[8];
        std::snprintf(buf, sizeof buf, "#%02x%02x%02x", r, g, b);
        return buf;
    }
};

// Series palette — the SAME hexes the IDE renderer's KIND_PALETTE uses,
// so saveas output matches the on-screen figure.
inline const std::vector<Rgb> &palette()
{
    static const std::vector<Rgb> p = {
        Rgb(0x7f, 0xd9, 0x9a), Rgb(0x5f, 0xb3, 0xd4), Rgb(0xe9, 0xb8, 0x70),
        Rgb(0x9b, 0x8c, 0xf2), Rgb(0xe2, 0x6a, 0x6a), Rgb(0xd4, 0xa5, 0xe6),
        Rgb(0xf2, 0xa3, 0x7e), Rgb(0x6f, 0xcf, 0xbf),
    };
    return p;
}

// MATLAB single-char linespec colours — mirrors the JS STYLE_COLOR map.
inline bool charSpecColor(char c, Rgb &out)
{
    switch (c) {
    case 'r': out = Rgb(0xf0, 0x70, 0x70); return true;
    case 'g': out = Rgb(0x6e, 0xe7, 0xa0); return true;
    case 'b': out = Rgb(0x60, 0xd0, 0xf0); return true;
    case 'k': out = Rgb(0x28, 0x28, 0x30); return true;
    case 'm': out = Rgb(0xe0, 0x70, 0xc0); return true;
    case 'c': out = Rgb(0x60, 0xd0, 0xf0); return true;
    case 'y': out = Rgb(0xe8, 0xd0, 0x60); return true;
    case 'w': out = Rgb(0xff, 0xff, 0xff); return true;
    default:  return false;
    }
}

// Parsed style string (both wire dialects: "r--o" and the kv extras).
struct StyleParts {
    Rgb color{0x7f, 0xd9, 0x9a};
    bool hasColor = false;   // explicit colour (char or kv)
    std::string lineStyle;   // "-", "--", ":", "-.", "" (unset), "none"
    std::string marker;      // "", "o", "+", "*", ".", "x", "s", "d", …
};

inline StyleParts parseStyle(const std::string &s)
{
    StyleParts p;
    if (s.empty())
        return p;
    if (s.find('=') != std::string::npos) {
        for (size_t i = 0; i < s.size();) {
            size_t eq = s.find('=', i);
            if (eq == std::string::npos)
                break;
            size_t semi = s.find(';', eq);
            std::string key = s.substr(i, eq - i);
            std::string val = s.substr(
                eq + 1, (semi == std::string::npos ? s.size() : semi) - eq - 1);
            if (key == "color") {
                p.color = Rgb::fromHex(val);
                p.hasColor = true;
            } else if (key == "lineStyle") {
                p.lineStyle = val;
            } else if (key == "marker") {
                p.marker = val;
            }
            i = semi == std::string::npos ? s.size() : semi + 1;
        }
        return p;
    }
    static const std::string markers = "o+*.xsd^v<>ph";
    size_t i = 0;
    while (i < s.size()) {
        if (p.lineStyle.empty() && s.compare(i, 2, "--") == 0) { p.lineStyle = "--"; i += 2; continue; }
        if (p.lineStyle.empty() && s.compare(i, 2, "-.") == 0)  { p.lineStyle = "-.";  i += 2; continue; }
        char c = s[i];
        if (p.lineStyle.empty() && (c == '-' || c == ':')) { p.lineStyle = std::string(1, c); ++i; continue; }
        if (!p.hasColor) {
            Rgb col;
            if (charSpecColor(c, col)) {
                p.color = col;
                p.hasColor = true;
                ++i;
                continue;
            }
        }
        if (p.marker.empty() && markers.find(c) != std::string::npos) {
            p.marker = std::string(1, c);
            ++i;
            continue;
        }
        ++i;
    }
    return p;
}

// ── Display list ─────────────────────────────────────────────────────
struct Cmd {
    enum Kind { LINE, POLY, TEXT, MARK } kind;
    std::vector<double> xs, ys;   // LINE/POLY pixel points; MARK single
    Rgb color;
    double width = 1.0;           // stroke width / marker size px
    bool fill = false;            // POLY: filled
    bool dash = false;            // LINE: dashed
    std::string text;             // TEXT
    double fontSize = 12;         // TEXT
    double x = 0, y = 0;          // TEXT baseline-left anchor / MARK centre
    bool rot90 = false;           // TEXT: rotate CCW (ylabel)
    bool bold = false;            // TEXT: FontWeight bold
    std::string fontFamily;       // TEXT: font name (SVG honours it;
                                  // the raster backend is monospace)
};

// ── Ticks (ported from ide/src/components/plot/plotTicks.js) ─────────
// MATLAB-exact auto ticks (probe-verified R2025b, portion 34): target
// 10 intervals, ladder breakpoints 1/2/5 — [0,10]→step 1 (11 ticks),
// [0,3]→step 0.5 (7), [0,7]→step 1 (8).
inline std::vector<double> niceTicks(double mn, double mx, int target = 10)
{
    std::vector<double> major;
    double range = mx - mn;
    if (!(range > 0))
        return {mn};
    double rough = range / target;
    double pow = std::pow(10.0, std::floor(std::log10(rough)));
    double norm = rough / pow;
    double step = norm <= 1.0 ? pow
                : norm <= 2.0 ? 2 * pow
                : norm <= 5.0 ? 5 * pow : 10 * pow;
    for (double v = std::ceil(mn / step) * step; v <= mx + step * 1e-6; v += step)
        major.push_back(std::stod(std::to_string(v).substr(0, 12)));
    return major;
}

inline std::vector<double> logTicks(double mn, double mx)
{
    std::vector<double> major;
    if (!(mn > 0) || !(mx > mn))
        return major;
    int lmin = (int)std::floor(std::log10(mn));
    int lmax = (int)std::ceil(std::log10(mx));
    for (int p = lmin; p <= lmax; ++p) {
        double base = std::pow(10.0, p);
        if (base >= mn && base <= mx)
            major.push_back(base);
    }
    return major;
}

// Default tick label: %g-style with trailing-zero trim + exponent
// compaction ("1e+06" → "1e6").
inline std::string tickLabel(double v)
{
    if (v == 0)
        return "0";
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    std::string s = buf;
    size_t e = s.find('e');
    if (e != std::string::npos) {
        std::string mant = s.substr(0, e);
        std::string exp = s.substr(e + 1);
        bool neg = !exp.empty() && exp[0] == '-';
        size_t k = 0;
        while (k < exp.size() && (exp[k] == '+' || exp[k] == '-'))
            ++k;
        while (k + 1 < exp.size() && exp[k] == '0')
            exp.erase(k, 1);
        if (mant.size() > 1 && mant.compare(mant.size() - 1, 1, "0") == 0
            && mant.find('.') != std::string::npos)
            while (mant.back() == '0')
                mant.pop_back();
        if (!mant.empty() && mant.back() == '.')
            mant.pop_back();
        s = mant + "e" + (neg ? "-" : "") + exp.substr(k);
    } else if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0')
            s.pop_back();
        if (!s.empty() && s.back() == '.')
            s.pop_back();
    }
    return s;
}

// ── Per-axes data range ──────────────────────────────────────────────
struct AxisRange {
    double lo = 0, hi = 1;
    bool log = false;
    bool valid() const { return hi > lo; }
    double map(double v, double p0, double p1) const   // data → pixel
    {
        double t;
        if (log) {
            double a = std::log10(lo), b = std::log10(hi);
            t = (std::log10(v) - a) / (b - a);
        } else {
            t = (v - lo) / (hi - lo);
        }
        return p0 + t * (p1 - p0);
    }
};

inline void dataRange(const std::vector<double> &v, double &lo, double &hi)
{
    lo = +INFINITY;
    hi = -INFINITY;
    for (double x : v)
        if (std::isfinite(x)) {
            lo = std::min(lo, x);
            hi = std::max(hi, x);
        }
}

inline std::vector<double> parseVec(const std::string &json)
{
    return figdetail::parseFlatDoubles(json);
}

// ["a","b",…] tick-label json → strings.
inline std::vector<std::string> parseTickLabelVec(const std::string &json)
{
    std::vector<std::string> labels;
    size_t i = 0;
    while (i < json.size()) {
        size_t q1 = json.find('"', i);
        if (q1 == std::string::npos)
            break;
        size_t q2 = json.find('"', q1 + 1);
        if (q2 == std::string::npos)
            break;
        labels.push_back(json.substr(q1 + 1, q2 - q1 - 1));
        i = q2 + 1;
    }
    return labels;
}

// ── Display-list builder ─────────────────────────────────────────────
struct Rect { double x0, y0, x1, y1; };   // pixel, y DOWN

inline void addLine(std::vector<Cmd> &out, std::vector<double> xs,
                    std::vector<double> ys, Rgb c, double w, bool dash)
{
    Cmd cmd;
    cmd.kind = Cmd::LINE;
    cmd.xs = std::move(xs);
    cmd.ys = std::move(ys);
    cmd.color = c;
    cmd.width = w;
    cmd.dash = dash;
    out.push_back(std::move(cmd));
}

inline void addText(std::vector<Cmd> &out, const std::string &s, double x,
                    double y, double size, Rgb c, bool rot90 = false,
                    bool bold = false, const std::string &family = "")
{
    if (s.empty())
        return;
    Cmd cmd;
    cmd.kind = Cmd::TEXT;
    cmd.text = s;
    cmd.x = x;
    cmd.y = y;
    cmd.fontSize = size;
    cmd.color = c;
    cmd.rot90 = rot90;
    cmd.bold = bold;
    cmd.fontFamily = family;
    out.push_back(std::move(cmd));
}

inline void addMarker(std::vector<Cmd> &out, double x, double y, double size,
                      Rgb c, const std::string &kind)
{
    Cmd cmd;
    cmd.kind = Cmd::MARK;
    cmd.x = x;
    cmd.y = y;
    cmd.width = size;
    cmd.color = c;
    cmd.text = kind;   // marker glyph selector
    cmd.fill = true;
    out.push_back(std::move(cmd));
}

inline void addPoly(std::vector<Cmd> &out, std::vector<double> xs,
                    std::vector<double> ys, Rgb c, bool fill)
{
    if (xs.size() < 2)
        return;
    Cmd cmd;
    cmd.kind = Cmd::POLY;
    cmd.xs = std::move(xs);
    cmd.ys = std::move(ys);
    cmd.color = c;
    cmd.fill = fill;
    cmd.width = 1;
    out.push_back(std::move(cmd));
}

// Render ONE axes into its pixel rect. `seriesIdxBase` continues the
// palette cycle across subplot cells (MATLAB restarts per axes; keep
// simple: per-axes reset).
inline void renderAxes(const FigureState &fig, const AxesState &ax, Rect box,
                       std::vector<Cmd> &out)
{
    const Rgb kAxisCol(0x40, 0x40, 0x44);
    const Rgb kGridCol(0xd8, 0xd8, 0xdc);
    const Rgb kTextCol(0x28, 0x28, 0x30);

    // Margin: room for tick labels + axis labels.
    double ml = 64, mr = 14, mt = 20, mb = 48;
    if (!ax.title.empty())
        mt += 18;
    Rect plot{box.x0 + ml, box.y0 + mt, box.x1 - mr, box.y1 - mb};

    // ── Collect series data → ranges ────────────────────────────────
    auto applyLim = [](const std::string &json, double &lo, double &hi) {
        if (json.empty())
            return;
        auto v = figdetail::parseFlatDoubles(json);
        if (v.size() == 2) {
            lo = v[0];
            hi = v[1];
        }
    };
    double xlo = +INFINITY, xhi = -INFINITY, ylo = +INFINITY, yhi = -INFINITY;
    std::vector<std::vector<double>> xs(ax.datasets.size()), ys(ax.datasets.size());
    for (size_t i = 0; i < ax.datasets.size(); ++i) {
        const DatasetInfo &ds = ax.datasets[i];
        if (!ds.visible)
            continue;
        if (ds.type == "imagesc" || ds.type == "image-rgb" || ds.type == "text")
            continue;   // deferred: raster/polar/text layers
        xs[i] = parseVec(ds.xJson);
        ys[i] = parseVec(ds.yJson);
        if (xs[i].empty()) {
            xs[i].resize(ys[i].size());
            for (size_t k = 0; k < ys[i].size(); ++k)
                xs[i][k] = (double)(k + 1);
        }
        double a, b;
        dataRange(xs[i], a, b);
        xlo = std::min(xlo, a);
        xhi = std::max(xhi, b);
        dataRange(ys[i], a, b);
        ylo = std::min(ylo, a);
        yhi = std::max(yhi, a * 0 + b);
        // errorbar extents
        auto ext = [&](const std::string &j, bool pos) {
            auto e = parseVec(j);
            if (e.empty())
                return;
            for (size_t k = 0; k < e.size() && k < ys[i].size(); ++k) {
                ylo = std::min(ylo, ys[i][k] - (pos ? 0 : e[k]));
                yhi = std::max(yhi, ys[i][k] + (pos ? e[k] : 0));
            }
        };
        ext(ds.eJson, true);
        ext(ds.eNegJson, false);
        ext(ds.ePosJson, true);
        if (ds.type == "stem") {
            ylo = std::min(ylo, 0.0);
            yhi = std::max(yhi, 0.0);
        }
    }
    if (!(xhi > xlo)) {
        xlo = 0;
        xhi = xhi > xlo ? xhi : xlo + 1;
    }
    if (!(yhi > ylo)) {
        double mid = ylo;
        ylo = mid - 0.5;
        yhi = mid + 0.5;
    }
    applyLim(ax.xlimJson, xlo, xhi);
    applyLim(ax.ylimJson, ylo, yhi);
    // 5% padding on non-explicit ranges (MATLAB-style breathing room).
    auto pad = [](double &lo, double &hi, const std::string &lim) {
        if (!lim.empty())
            return;
        double d = (hi - lo) * 0.05;
        lo -= d;
        hi += d;
    };
    pad(xlo, xhi, ax.xlimJson);
    pad(ylo, yhi, ax.ylimJson);
    if (ax.xscale == "log") {
        xlo = std::max(xlo, 1e-12);
        if (!(xhi > xlo))
            xhi = xlo * 100;
    }
    if (ax.yscale == "log") {
        ylo = std::max(ylo, 1e-12);
        if (!(yhi > ylo))
            yhi = ylo * 100;
    }

    AxisRange XR{xlo, xhi, ax.xscale == "log"};
    AxisRange YR{ylo, yhi, ax.yscale == "log"};
    double px0 = plot.x0, px1 = plot.x1;
    double py0 = plot.y1, py1 = plot.y0;   // y DOWN: data-up = pixel-up
    if (ax.xDir == "reverse")
        std::swap(px0, px1);
    if (ax.yDir == "reverse")
        std::swap(py0, py1);
    auto X = [&](double v) { return XR.map(v, px0, px1); };
    auto Y = [&](double v) { return YR.map(v, py0, py1); };

    // ── Grid (under the data) ───────────────────────────────────────
    if (ax.gridMajor && ax.gridUserTouched) {
        auto gx = XR.log ? logTicks(xlo, xhi) : niceTicks(xlo, xhi);
        auto gy = YR.log ? logTicks(ylo, yhi) : niceTicks(ylo, yhi);
        for (double v : gx)
            addLine(out, {X(v), X(v)}, {plot.y0, plot.y1}, kGridCol, 1, false);
        for (double v : gy)
            addLine(out, {plot.x0, plot.x1}, {Y(v), Y(v)}, kGridCol, 1, false);
    }

    // ── Series ──────────────────────────────────────────────────────
    size_t seriesIdx = 0;
    for (size_t i = 0; i < ax.datasets.size(); ++i) {
        const DatasetInfo &ds = ax.datasets[i];
        if (!ds.visible || xs[i].empty())
            continue;
        StyleParts st = parseStyle(ds.style);
        if (!st.hasColor)
            st.color = palette()[seriesIdx % palette().size()];
        ++seriesIdx;
        Rgb col = st.color;
        double lw = ds.lineWidth > 0 ? ds.lineWidth : (st.lineStyle == "-" || st.lineStyle.empty() ? 1.4 : 1.4);
        double ms = ds.markerSize > 0 ? ds.markerSize * 1.4 : 6.0;
        bool dashed = st.lineStyle == "--" || st.lineStyle == ":" || st.lineStyle == "-.";
        const size_t n = std::min(xs[i].size(), ys[i].size());
        const std::string &t = ds.type;

        if (t == "line" || t == "comet" || t == "animatedline") {
            std::vector<double> px, py;
            for (size_t k = 0; k < n; ++k)
                if (std::isfinite(xs[i][k]) && std::isfinite(ys[i][k])) {
                    px.push_back(X(xs[i][k]));
                    py.push_back(Y(ys[i][k]));
                }
            if (st.lineStyle != "none" && px.size() >= 2)
                addLine(out, px, py, col, lw, dashed);
            if (!st.marker.empty())
                for (size_t k = 0; k < px.size(); ++k)
                    addMarker(out, px[k], py[k], ms, col, st.marker);
        } else if (t == "stairs") {
            std::vector<double> px, py;
            for (size_t k = 0; k < n; ++k) {
                if (k && px.size()) {
                    px.push_back(X(xs[i][k]));
                    py.push_back(py.back());
                }
                px.push_back(X(xs[i][k]));
                py.push_back(Y(ys[i][k]));
            }
            if (px.size() >= 2)
                addLine(out, px, py, col, lw, dashed);
            if (!st.marker.empty())
                for (size_t k = 0; k < n; ++k)
                    addMarker(out, X(xs[i][k]), Y(ys[i][k]), ms, col, st.marker);
        } else if (t == "stem") {
            double base = 0;
            if (base >= YR.lo && base <= YR.hi)
                for (size_t k = 0; k < n; ++k)
                    addLine(out, {X(xs[i][k]), X(xs[i][k])},
                            {Y(base), Y(ys[i][k])}, col, lw, dashed);
            for (size_t k = 0; k < n; ++k)
                addMarker(out, X(xs[i][k]), Y(ys[i][k]), ms,
                          col, st.marker.empty() ? "o" : st.marker);
        } else if (t == "scatter" || t == "bubble") {
            std::vector<double> sz;
            if (!ds.sizeJson.empty())
                sz = parseVec(ds.sizeJson);
            for (size_t k = 0; k < n; ++k)
                addMarker(out, X(xs[i][k]), Y(ys[i][k]),
                          sz.empty() ? ms : std::max(3.0, std::sqrt(sz[k] / 3.14) * 1.6),
                          col, "o");
        } else if (t == "bar" || t == "histogram") {
            // Grouped vertical bars, width 0.8 of slot.
            const int groups = (int)n;
            const int series = 1;   // per-dataset grouping (adjacent bar
                                    // datasets share slots below)
            for (size_t k = 0; k < n; ++k) {
                double slot = groups > 1 ? (xhi - xlo) / (groups - 1) : 1;
                double w = std::min(slot * 0.8, (plot.x1 - plot.x0) * 0.05) / series;
                double x0p = X(xs[i][k]) - w / 2;
                double yBase = Y(std::min(std::max(0.0, YR.lo), YR.hi));
                double yTop = Y(ys[i][k]);
                addPoly(out, {x0p, x0p + w, x0p + w, x0p},
                        {yBase, yBase, yTop, yTop}, col, true);
            }
        } else if (t == "area") {
            std::vector<double> px, py;
            double yBase = Y(std::min(std::max(0.0, YR.lo), YR.hi));
            px.push_back(X(xs[i].front()));
            py.push_back(yBase);
            for (size_t k = 0; k < n; ++k) {
                px.push_back(X(xs[i][k]));
                py.push_back(Y(ys[i][k]));
            }
            px.push_back(X(xs[i][n - 1]));
            py.push_back(yBase);
            addPoly(out, px, py, col, true);
        } else if (t == "errorbar") {
            auto sym = parseVec(ds.eJson);
            auto eneg = parseVec(ds.eNegJson);
            auto epos = parseVec(ds.ePosJson);
            for (size_t k = 0; k < n; ++k) {
                double loV = ys[i][k], hiV = ys[i][k];
                if (k < sym.size()) {
                    loV -= sym[k];
                    hiV += sym[k];
                }
                if (k < eneg.size())
                    loV = ys[i][k] - eneg[k];
                if (k < epos.size())
                    hiV = ys[i][k] + epos[k];
                double pxk = X(xs[i][k]);
                addLine(out, {pxk, pxk}, {Y(loV), Y(hiV)}, col, 1, false);
                double cap = 4;
                addLine(out, {pxk - cap, pxk + cap}, {Y(loV), Y(loV)}, col, 1, false);
                addLine(out, {pxk - cap, pxk + cap}, {Y(hiV), Y(hiV)}, col, 1, false);
            }
            if (!st.marker.empty())
                for (size_t k = 0; k < n; ++k)
                    addMarker(out, X(xs[i][k]), Y(ys[i][k]), ms, col, st.marker);
        }
        // Unhandled types (quiver/feather/pie/…) silently skipped — see
        // the portion-32 export-gaps bug.
    }

    // ── Box + ticks + labels (portion 34: custom ticks/labels, fonts,
    // frame width — all MATLAB-probe-verified) ─────────────────────
    const double boxW = std::max(0.75, ax.lineWidth);
    addLine(out, {plot.x0, plot.x1, plot.x1, plot.x0, plot.x0},
            {plot.y0, plot.y0, plot.y1, plot.y1, plot.y0}, kAxisCol, boxW,
            false);
    auto tx = !ax.xTicksJson.empty()
                  ? parseVec(ax.xTicksJson)
                  : (XR.log ? logTicks(xlo, xhi) : niceTicks(xlo, xhi));
    auto ty = !ax.yTicksJson.empty()
                  ? parseVec(ax.yTicksJson)
                  : (YR.log ? logTicks(ylo, yhi) : niceTicks(ylo, yhi));
    std::vector<std::string> txLab, tyLab;
    if (!ax.xTickLabelsJson.empty())
        txLab = parseTickLabelVec(ax.xTickLabelsJson);
    if (!ax.yTickLabelsJson.empty())
        tyLab = parseTickLabelVec(ax.yTickLabelsJson);
    const double tickFS = ax.fontSize;
    const double labelFS = ax.fontSize * 1.1;
    const bool fBold = ax.fontWeight == "bold";
    const std::string &fam = ax.fontName;
    for (size_t k = 0; k < tx.size(); ++k) {
        const double v = tx[k];
        double p = X(v);
        addLine(out, {p, p}, {plot.y1, plot.y1 + 4}, kAxisCol, boxW, false);
        std::string lab = k < txLab.size()
                              ? txLab[k]
                              : tickLabel(v);
        if (k >= txLab.size() && !ax.xTickFormat.empty()
            && ax.xTickFormat.size() > 1 && ax.xTickFormat[0] == '%') {
            char b[40];
            std::snprintf(b, sizeof b, ax.xTickFormat.c_str(), v);
            lab = b;
        }
        addText(out, lab, p - 3.0 * lab.size(), plot.y1 + 8, tickFS,
                kTextCol, false, fBold, fam);
    }
    for (size_t k = 0; k < ty.size(); ++k) {
        const double v = ty[k];
        double p = Y(v);
        addLine(out, {plot.x0 - 4, plot.x0}, {p, p}, kAxisCol, boxW, false);
        std::string lab = k < tyLab.size()
                              ? tyLab[k]
                              : tickLabel(v);
        if (k >= tyLab.size() && !ax.yTickFormat.empty()
            && ax.yTickFormat.size() > 1 && ax.yTickFormat[0] == '%') {
            char b[40];
            std::snprintf(b, sizeof b, ax.yTickFormat.c_str(), v);
            lab = b;
        }
        double w = 6.0 * lab.size();
        addText(out, lab, plot.x0 - 8 - w, p + 4, tickFS, kTextCol, false,
                fBold, fam);
    }
    if (!ax.title.empty())
        addText(out, ax.title, (plot.x0 + plot.x1) / 2 - 3.2 * ax.title.size(),
                plot.y0 - 18, labelFS + 2, kTextCol, false, fBold, fam);
    if (!ax.xlabel.empty())
        addText(out, ax.xlabel, (plot.x0 + plot.x1) / 2 - 3.2 * ax.xlabel.size(),
                plot.y1 + 34, labelFS, kTextCol, false, fBold, fam);
    if (!ax.ylabel.empty())
        addText(out, ax.ylabel, plot.x0 - 46,
                (plot.y0 + plot.y1) / 2 + 3.2 * ax.ylabel.size(), labelFS,
                kTextCol, /*rot90=*/true, fBold, fam);

    // ── Legend ──────────────────────────────────────────────────────
    if (!ax.legendLabels.empty() && !ax.legendHidden) {
        double rowH = 16, pad2 = 6;
        double lw2 = 22, tw = 0;
        for (auto &s2 : ax.legendLabels)
            tw = std::max(tw, 6.0 * s2.size());
        double wLegend = pad2 + lw2 + 6 + tw + pad2;
        double hLegend = pad2 + rowH * ax.legendLabels.size() + pad2;
        double lx = plot.x1 - 10 - wLegend, ly = plot.y0 + 10;
        if (ax.legendLocation.find("south") != std::string::npos)
            ly = plot.y1 - 10 - hLegend;
        if (ax.legendLocation.find("west") != std::string::npos)
            lx = plot.x0 + 10;
        if (ax.legendLocation.find("outside") != std::string::npos)
            lx = plot.x1 + 8;
        addPoly(out, {lx, lx + wLegend, lx + wLegend, lx},
                {ly, ly, ly + hLegend, ly + hLegend}, kAxisCol, false);
        for (size_t k = 0; k < ax.legendLabels.size(); ++k) {
            double ry = ly + pad2 + rowH * k + rowH / 2;
            Rgb c = palette()[k % palette().size()];
            for (size_t d = 0; d < ax.datasets.size(); ++d) {
                if (ax.datasets[d].label == ax.legendLabels[k]) {
                    StyleParts sp = parseStyle(ax.datasets[d].style);
                    if (sp.hasColor)
                        c = sp.color;
                    break;
                }
            }
            addLine(out, {lx + pad2, lx + pad2 + lw2}, {ry, ry}, c, 1.6, false);
            addText(out, ax.legendLabels[k], lx + pad2 + lw2 + 6, ry + 4, 11,
                    kTextCol);
        }
    }
}

// Full figure → display list. Subplot grid tiling; single axes uses the
// whole canvas.
inline std::vector<Cmd> renderFigureDisplay(const FigureState &fig, int W,
                                            int H)
{
    std::vector<Cmd> out;
    int rows = std::max(1, fig.subplotRows);
    int cols = std::max(1, fig.subplotCols);
    // Order axes by subplotIndex (1-based reading order) when gridded.
    std::vector<const AxesState *> ordered;
    for (int p = 1; p <= rows * cols; ++p)
        for (const auto &ax : fig.axes)
            if (ax.subplotIndex == p) {
                ordered.push_back(&ax);
                break;
            }
    if (ordered.empty())
        for (const auto &ax : fig.axes)
            ordered.push_back(&ax);
    for (size_t i = 0; i < ordered.size(); ++i) {
        int r = rows > 1 || cols > 1 ? (int)i / cols : 0;
        int c = rows > 1 || cols > 1 ? (int)i % cols : 0;
        Rect box;
        box.x0 = 10 + c * (W - 20.0) / cols + 6;
        box.x1 = 10 + (c + 1) * (W - 20.0) / cols - 6;
        box.y0 = 10 + r * (H - 20.0) / rows + 6;
        box.y1 = 10 + (r + 1) * (H - 20.0) / rows - 6;
        if (fig.subplotRows == 0) {
            box = Rect{10, 10, (double)W - 10, (double)H - 10};
        }
        if (!fig.superTitle.empty() && r == 0 && c == (cols - 1) / 2) {
            addText(out, fig.superTitle,
                    W / 2.0 - 3.4 * fig.superTitle.size(), 6, 13,
                    Rgb(0x28, 0x28, 0x30));
            box.y0 += 10;
        }
        renderAxes(fig, *ordered[i], box, out);
    }
    return out;
}

// ── SVG backend ─────────────────────────────────────────────────────
inline std::string renderFigureSvg(const FigureState &fig, int W, int H)
{
    std::vector<Cmd> cmds = renderFigureDisplay(fig, W, H);
    std::string s = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"";
    s += std::to_string(W) + "\" height=\"" + std::to_string(H)
       + "\" viewBox=\"0 0 " + std::to_string(W) + " " + std::to_string(H)
       + "\">\n<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
    auto esc = [](const std::string &t) {
        std::string o;
        for (char ch : t) {
            if (ch == '&')
                o += "&amp;";
            else if (ch == '<')
                o += "&lt;";
            else if (ch == '>')
                o += "&gt;";
            else
                o += ch;
        }
        return o;
    };
    for (const Cmd &c : cmds) {
        if (c.kind == Cmd::TEXT) {
            s += "<text x=\"" + std::to_string(c.x) + "\" y=\""
               + std::to_string(c.y) + "\" font-size=\""
               + std::to_string(c.fontSize)
               + "\" font-family=\""
               + (c.fontFamily.empty() ? std::string("monospace")
                                       : c.fontFamily)
               + "\"" + std::string(c.bold ? " font-weight=\"bold\"" : "")
               + " fill=\"" + c.color.hex() + "\"";
            if (c.rot90)
                s += " transform=\"rotate(-90 " + std::to_string(c.x) + " "
                   + std::to_string(c.y) + ")\"";
            s += ">" + esc(c.text) + "</text>\n";
        } else if (c.kind == Cmd::MARK) {
            double r = c.width / 2;
            if (c.text == "s" || c.text == "d") {
                s += "<rect x=\"" + std::to_string(c.x - r) + "\" y=\""
                   + std::to_string(c.y - r) + "\" width=\""
                   + std::to_string(c.width) + "\" height=\""
                   + std::to_string(c.width) + "\" fill=\""
                   + (c.text == "d" ? "none" : c.color.hex()) + "\" stroke=\""
                   + c.color.hex() + "\"/>\n";
            } else if (c.text == "+" || c.text == "x") {
                s += "<path d=\"M" + std::to_string(c.x - r) + " "
                   + std::to_string(c.y) + " L" + std::to_string(c.x + r) + " "
                   + std::to_string(c.y) + " M" + std::to_string(c.x) + " "
                   + std::to_string(c.y - r) + " L" + std::to_string(c.x)
                   + " " + std::to_string(c.y + r)
                   + (c.text == "x" ? " M" + std::to_string(c.x - r) + " "
                                        + std::to_string(c.y - r) + " L"
                                        + std::to_string(c.x + r) + " "
                                        + std::to_string(c.y + r) + " M"
                                        + std::to_string(c.x + r) + " "
                                        + std::to_string(c.y - r) + " L"
                                        + std::to_string(c.x - r) + " "
                                        + std::to_string(c.y + r)
                                    : "")
                   + "\" stroke=\"" + c.color.hex() + "\" stroke-width=\"1.2\" fill=\"none\"/>\n";
            } else {   // o . * ^ v < > p h → circle
                s += "<circle cx=\"" + std::to_string(c.x) + "\" cy=\""
                   + std::to_string(c.y) + "\" r=\"" + std::to_string(r)
                   + "\" fill=\"none\" stroke=\"" + c.color.hex()
                   + "\" stroke-width=\"1.4\"/>\n";
            }
        } else {
            // LINE / POLY → <polyline>/<polygon>
            std::string pts;
            for (size_t i = 0; i < c.xs.size(); ++i) {
                if (i)
                    pts += " ";
                char b[48];
                std::snprintf(b, sizeof b, "%.1f,%.1f", c.xs[i], c.ys[i]);
                pts += b;
            }
            if (c.kind == Cmd::POLY && c.fill) {
                s += "<polygon points=\"" + pts + "\" fill=\"" + c.color.hex()
                   + "\" stroke=\"none\"/>\n";
            } else {
                s += "<polyline points=\"" + pts + "\" fill=\"none\" stroke=\""
                   + c.color.hex() + "\" stroke-width=\""
                   + std::to_string(c.width) + "\"";
                if (c.dash)
                    s += " stroke-dasharray=\"7 5\"";
                s += "/>\n";
            }
        }
    }
    s += "</svg>\n";
    return s;
}

// ── RGB raster backend ───────────────────────────────────────────────
// Classic public-domain 5×7 bitmap font (column-encoded, LSB = top row),
// ASCII 0x20..0x7E.
inline const std::uint8_t *font5x7(char c)
{
    // Canonical public-domain 5x7 font (Adafruit glcdfont, chars 0x20..0x7E),
    // column-encoded, LSB = top row. Unsized so a dropped row fails to
    // compile instead of silently shifting every later glyph.
    static const std::uint8_t F[][5] = {
        {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
        {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
        {0x36,0x49,0x56,0x20,0x50},{0x00,0x08,0x07,0x03,0x00},{0x00,0x1C,0x22,0x41,0x00},
        {0x00,0x41,0x22,0x1C,0x00},{0x2A,0x1C,0x7F,0x1C,0x2A},{0x08,0x08,0x3E,0x08,0x08},
        {0x00,0x80,0x70,0x30,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x00,0x60,0x60,0x00},
        {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
        {0x72,0x49,0x49,0x49,0x46},{0x21,0x41,0x49,0x4D,0x33},{0x18,0x14,0x12,0x7F,0x10},
        {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x31},{0x41,0x21,0x11,0x09,0x07},
        {0x36,0x49,0x49,0x49,0x36},{0x46,0x49,0x49,0x29,0x1E},{0x00,0x00,0x14,0x00,0x00},
        {0x00,0x40,0x34,0x00,0x00},{0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},
        {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x59,0x09,0x06},{0x3E,0x41,0x5D,0x59,0x4E},
        {0x7C,0x12,0x11,0x12,0x7C},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
        {0x7F,0x41,0x41,0x41,0x3E},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
        {0x3E,0x41,0x41,0x51,0x73},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
        {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
        {0x7F,0x02,0x1C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
        {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
        {0x26,0x49,0x49,0x49,0x32},{0x03,0x01,0x7F,0x01,0x03},{0x3F,0x40,0x40,0x40,0x3F},
        {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
        {0x03,0x04,0x78,0x04,0x03},{0x61,0x59,0x49,0x4D,0x43},{0x00,0x7F,0x41,0x41,0x41},
        {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x41,0x7F},{0x04,0x02,0x01,0x02,0x04},
        {0x40,0x40,0x40,0x40,0x40},{0x00,0x03,0x07,0x08,0x00},{0x20,0x54,0x54,0x78,0x40},
        {0x7F,0x28,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x28},{0x38,0x44,0x44,0x28,0x7F},
        {0x38,0x54,0x54,0x54,0x18},{0x00,0x08,0x7E,0x09,0x02},{0x18,0xA4,0xA4,0x9C,0x78},
        {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x40,0x3D,0x00},
        {0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x78,0x04,0x78},
        {0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0xFC,0x18,0x24,0x24,0x18},
        {0x18,0x24,0x24,0x18,0xFC},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x24},
        {0x04,0x04,0x3F,0x44,0x24},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
        {0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x4C,0x90,0x90,0x90,0x7C},
        {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x77,0x00,0x00},
        {0x00,0x41,0x36,0x08,0x00},{0x02,0x01,0x02,0x04,0x02}
    };
    static_assert(sizeof F / sizeof F[0] == 95,
                  "font5x7 must cover exactly ASCII 0x20..0x7E");
    if (c < 0x20 || c > 0x7E)
        c = '?';
    return F[c - 0x20];
}

class Raster
{
public:
    Raster(int w, int h) : w_(w), h_(h), px_(w * h * 3, 255) {}

    void px(int x, int y, Rgb c)
    {
        if (x < 0 || y < 0 || x >= w_ || y >= h_)
            return;
        size_t o = (size_t)(y * w_ + x) * 3;
        px_[o] = c.r;
        px_[o + 1] = c.g;
        px_[o + 2] = c.b;
    }

    // Thick-ish Bresenham with optional dashing (7 on / 5 off).
    void line(double x0, double y0, double x1, double y1, Rgb c, double wd,
              bool dash)
    {
        double len = std::hypot(x1 - x0, y1 - y0);
        if (len < 1e-9)
            return;
        int steps = (int)std::ceil(len * 2);
        double dashOn = 7, dashOff = 5;
        double run = 0;
        int side = (int)std::max(1.0, std::round(wd));
        for (int i = 0; i <= steps; ++i) {
            double t = (double)i / steps;
            double x = x0 + (x1 - x0) * t;
            double y = y0 + (y1 - y0) * t;
            if (dash) {
                double m = std::fmod(run, dashOn + dashOff);
                run += len / steps;
                if (m > dashOn)
                    continue;
            }
            for (int dx = 0; dx < side; ++dx)
                for (int dy = 0; dy < side; ++dy)
                    px((int)x + dx - side / 2, (int)y + dy - side / 2, c);
        }
    }

    void polygon(const std::vector<double> &xs, const std::vector<double> &ys,
                 Rgb c, bool fill)
    {
        if (xs.size() < 2)
            return;
        if (fill) {
            // Scanline fill (even-odd).
            double ymn = ys[0], ymx = ys[0];
            for (double y : ys) {
                ymn = std::min(ymn, y);
                ymx = std::max(ymx, y);
            }
            for (int y = (int)std::floor(ymn); y <= (int)std::ceil(ymx); ++y) {
                std::vector<double> xi;
                for (size_t i = 0; i < xs.size(); ++i) {
                    size_t j = (i + 1) % xs.size();
                    double ya = ys[i], yb = ys[j];
                    if ((y + 0.5 >= ya && y + 0.5 < yb)
                        || (y + 0.5 >= yb && y + 0.5 < ya))
                        xi.push_back(xs[i] + (y + 0.5 - ya) / (yb - ya)
                                          * (xs[j] - xs[i]));
                }
                std::sort(xi.begin(), xi.end());
                for (size_t k = 0; k + 1 < xi.size(); k += 2)
                    for (int x = (int)std::ceil(xi[k]); x <= (int)xi[k + 1]; ++x)
                        px(x, y, c);
            }
        } else {
            for (size_t i = 0; i + 1 < xs.size(); ++i)
                line(xs[i], ys[i], xs[i + 1], ys[i + 1], c, 1, false);
            line(xs.back(), ys.back(), xs.front(), ys.front(), c, 1, false);
        }
    }

    void marker(double x, double y, double size, Rgb c, const std::string &k)
    {
        int r = (int)std::max(2.0, size / 2);
        if (k == "s") {
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx)
                    px((int)x + dx, (int)y + dy, c);
        } else if (k == "+" || k == "x") {
            for (int d = -r; d <= r; ++d) {
                px((int)x + d, (int)y, c);
                px((int)x, (int)y + d, c);
                if (k == "x") {
                    px((int)x + d, (int)y + d, c);
                    px((int)x + d, (int)y - d, c);
                }
            }
        } else {   // o . * ^ v < > p h → outline circle (+ rays for *)
            for (int a = 0; a < 360; a += 2) {
                double rad = a * 3.14159265358979 / 180.0;
                px((int)std::lround(x + r * std::cos(rad)),
                   (int)std::lround(y + r * std::sin(rad)), c);
            }
            if (k == "*")
                for (int d = -r - 1; d <= r + 1; ++d) {
                    px((int)x + d, (int)y, c);
                    px((int)x, (int)y + d, c);
                    px((int)x + d, (int)y + d, c);
                    px((int)x + d, (int)y - d, c);
                }
        }
    }

    // Monospace text, scale = fontSize / 7 (glyph 5×7 + 1 col spacing).
    // rot90 renders the run bottom-to-top (ylabel).
    void text(double x, double y, const std::string &s, double fontSize, Rgb c,
              bool rot90)
    {
        int scale = std::max(1, (int)std::lround(fontSize / 7.0));
        if (!rot90) {
            double cx = x;
            for (char ch : s) {
                const std::uint8_t *g = font5x7(ch);
                for (int col = 0; col < 5; ++col)
                    for (int row = 0; row < 7; ++row)
                        if (g[col] & (1 << row))
                            for (int sx = 0; sx < scale; ++sx)
                                for (int sy = 0; sy < scale; ++sy)
                                    px((int)cx + col * scale + sx,
                                       (int)y + row * scale + sy, c);
                cx += 6 * scale;
            }
        } else {
            // Glyph columns run UPWARD from (x,y); within a glyph rows
            // go left→right after rotation.
            double cy = y;
            for (auto it = s.rbegin(); it != s.rend(); ++it) {
                const std::uint8_t *g = font5x7(*it);
                for (int row = 0; row < 7; ++row)
                    for (int col = 0; col < 5; ++col)
                        if (g[col] & (1 << row))
                            for (int sx = 0; sx < scale; ++sx)
                                for (int sy = 0; sy < scale; ++sy)
                                    px((int)x + (6 - row) * scale + sx,
                                       (int)cy - (5 - col) * scale - sy, c);
                cy -= 6 * scale;
            }
        }
    }

    const std::vector<std::uint8_t> &bytes() const { return px_; }
    int width() const { return w_; }
    int height() const { return h_; }

private:
    int w_, h_;
    std::vector<std::uint8_t> px_;
};

// Render to a top-down RGB buffer (H×W×3 bytes).
inline std::vector<std::uint8_t> renderFigureRgb(const FigureState &fig,
                                                 int W, int H)
{
    Raster r(W, H);
    for (const Cmd &c : renderFigureDisplay(fig, W, H)) {
        switch (c.kind) {
        case Cmd::TEXT:
            r.text(c.x, c.y, c.text, c.fontSize, c.color, c.rot90);
            if (c.bold)   // fake bold: second strike one pixel over
                r.text(c.x + 1, c.y, c.text, c.fontSize, c.color, c.rot90);
            break;
        case Cmd::MARK:
            r.marker(c.x, c.y, c.width, c.color, c.text);
            break;
        case Cmd::POLY:
            r.polygon(c.xs, c.ys, c.color, c.fill);
            break;
        case Cmd::LINE:
            for (size_t i = 0; i + 1 < c.xs.size(); ++i)
                r.line(c.xs[i], c.ys[i], c.xs[i + 1], c.ys[i + 1], c.color,
                       c.width, c.dash);
            break;
        }
    }
    return r.bytes();
}

} // namespace figrender
} // namespace numkit
