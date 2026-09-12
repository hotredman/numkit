// src/graphics/src/library.cpp
//
// Engine-coupled installer for the graphics plotting service — the SOLE
// graphics TU that includes <numkit/core>. It is registration glue only: it
// asks plots.cpp (core-free) for the table of GraphicsFn plotting bodies, then
// wraps each in a CallContext→GraphicsContext adapter and registers it under
//   graphics.<sub>.<name>   +   compat.<name>   (+ bare core name when core=true)
//
// The adapter is the one place graphics' two runtime escape hatches are bound
// to the live Engine: callBuiltin (forward to another registered plot builtin
// by name — surfc→surf, fcontour→contour, geoplot→plot, …) and callHandle
// (evaluate a user @(x) function handle for fplot/fsurf sampling). Everything
// else a plotting body needs flows through the figure session (FigureManager)
// and a scratch arena, both core-free.
//
// This mirrors the toolbox convention: compute is core-free, the install hub is
// the lone Engine-coupled file (here it also hosts the generic adapter, since
// graphics has one uniform adapter rather than per-function _reg bridges).
//
// ── Graphics handles (bugs/closed/graphics/plot-family-no-return-value.md) ──
// This TU is also the handle ACCESSOR layer:
//   • the adapter post-hook binds handle objects for chart builtins
//     (h = plot(...)) and for figure/gca/gcf/axes;
//   • handle-aware set/get/isgraphics/ishandle/isvalid are registered here
//     (delete lives in general_reg.cpp — one core name, type-dispatched);
//   • one BuiltinClass per MATLAB class name routes dot-syntax
//     (h.LineWidth, h.LineWidth = 2) through the FigureManager registry.

#include <numkit/graphics/library.hpp>
#include <numkit/graphics/graphics_context.hpp>
#include <numkit/graphics/graphics_handle.hpp>

#include <numkit/core/engine.hpp>  // Engine, CallContext, ExternalFunc, Span, Value
#include <numkit/figure/figure_manager.hpp>
#include <numkit/figure/figure_render.hpp>
#include <numkit/value/object.hpp>

#include <cstddef>
#include <cstdio>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace numkit {

// export_reg.cpp — saveas/exportgraphics/print (portion 32).
void registerFigureExport(Engine &engine);

namespace {

constexpr const char *kFigureClass = "matlab.ui.Figure";
constexpr const char *kAxesClass   = "matlab.graphics.axis.Axes";
constexpr const char *kTextClass   = "matlab.graphics.primitive.Text";
constexpr const char *kLegendClass = "matlab.graphics.illustration.Legend";
constexpr const char *kRootClass   = "matlab.ui.Root";

// Builtin name → Text slot for the title/label family (adapter wrap).
const std::string &textSlotOf(const std::string &fnName)
{
    static const std::map<std::string, std::string> m = {
        {"title", "title"}, {"subtitle", "subtitle"}, {"sgtitle", "sgtitle"},
        {"xlabel", "xlabel"}, {"ylabel", "ylabel"}, {"zlabel", "zlabel"},
    };
    static const std::string kEmpty;
    auto it = m.find(fnName);
    return it == m.end() ? kEmpty : it->second;
}

// Axes PROPERTY name → Text slot (get(gca,'Title') family — capitalized).
const std::string &textSlotOfProp(const std::string &prop)
{
    static const std::map<std::string, std::string> m = {
        {"Title", "title"}, {"Subtitle", "subtitle"},
        {"XLabel", "xlabel"}, {"YLabel", "ylabel"}, {"ZLabel", "zlabel"},
    };
    static const std::string kEmpty;
    auto it = m.find(prop);
    return it == m.end() ? kEmpty : it->second;
}

// Which AxesState field a Text slot reads/writes ("sgtitle" → the
// FigureState instead). ylabel under yyaxis-right routes to ylabel2.
std::string *textFieldOf(FigureManager &fm, FigureManager::HandleRecord &rec)
{
    if (rec.slot == "sgtitle") {
        FigureState *fig = fm.figureOf(rec.figureId);
        return fig ? &fig->superTitle : nullptr;
    }
    AxesState *ax = fm.axesOf(rec);
    if (!ax)
        return nullptr;
    if (rec.slot == "title")    return &ax->title;
    if (rec.slot == "subtitle") return &ax->subtitle;
    if (rec.slot == "xlabel")   return &ax->xlabel;
    if (rec.slot == "ylabel") {
        if (ax->yyEnabled && ax->activeYside == "right")
            return &ax->ylabel2;
        return &ax->ylabel;
    }
    if (rec.slot == "zlabel")   return &ax->zlabel;
    return nullptr;
}

// Builtin name → (type, MATLAB class name)
const std::unordered_map<std::string, std::pair<std::string, std::string>> &
handleTypes()
{
    static const std::unordered_map<std::string,
        std::pair<std::string, std::string>> m = {
        {"plot",        {"line",    "matlab.graphics.chart.primitive.Line"}},
        {"plot3",       {"line",    "matlab.graphics.chart.primitive.Line"}},
        {"scatter",     {"scatter", "matlab.graphics.chart.scatter.Scatter"}},
        {"scatter3",    {"scatter", "matlab.graphics.chart.scatter.Scatter"}},
        {"stem",        {"stem",    "matlab.graphics.chart.primitive.Stem"}},
        {"stem3",       {"stem",    "matlab.graphics.chart.primitive.Stem"}},
        {"stairs",      {"stairs",  "matlab.graphics.chart.primitive.Stair"}},
        {"bar",         {"bar",     "matlab.graphics.chart.primitive.Bar"}},
        {"barh",        {"bar",     "matlab.graphics.chart.primitive.Bar"}},
        {"errorbar",    {"errorbar","matlab.graphics.chart.primitive.ErrorBar"}},
        {"area",        {"area",    "matlab.graphics.chart.primitive.Area"}},
        {"pie",         {"pie",     "matlab.graphics.chart.primitive.Pie"}},
        {"compass",     {"compass", "matlab.graphics.chart.primitive.Compass"}},
        {"polarplot",   {"line",    "matlab.graphics.chart.primitive.Line"}},
        {"surf",        {"surface", "matlab.graphics.chart.surface.Surface"}},
        {"mesh",        {"surface", "matlab.graphics.chart.surface.Surface"}},
        {"waterfall",   {"surface", "matlab.graphics.chart.surface.Waterfall"}},
        {"contour",     {"contour", "matlab.graphics.chart.contour.Contour"}},
        {"contour3",    {"contour", "matlab.graphics.chart.contour.Contour"}},
        {"contourf",    {"contour", "matlab.graphics.chart.contour.Contour"}},
        {"imagesc",     {"image",   "matlab.graphics.image.Image"}},
        {"image",       {"image",   "matlab.graphics.image.Image"}},
        {"quiver",      {"quiver",  "matlab.graphics.chart.primitive.Quiver"}},
        {"feather",     {"feather", "matlab.graphics.chart.primitive.Feather"}},
        {"histogram",   {"histogram","matlab.graphics.chart.histogram.Histogram"}},
        {"fplot",       {"line",    "matlab.graphics.chart.primitive.Line"}},
    };
    return m;
}

// ── Property encoding ────────────────────────────────────────────────
// rec->props maps name → string form (figure layer is Value-free).
// Numerics use %.17g so decodeProp's stod round-trips bit-exactly
// (std::to_string's 6 digits would silently truncate user values).
std::string formatPropDouble(double d)
{
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.17g", d);
    return buf;
}

std::string encodeProp(const Value &v)
{
    if (v.isChar() || v.isString())
        return v.toString();
    if (v.isScalar())
        return formatPropDouble(v.toScalar());
    if (v.isLogical() && v.numel() == 1)
        return v.toBool() ? "1" : "0";
    if (v.isNumeric() && v.numel() >= 2) {
        std::string s = "[";
        for (size_t i = 0; i < v.numel(); ++i) {
            if (i) s += ",";
            s += formatPropDouble(v.elemAsDouble(i));
        }
        return s + "]";
    }
    return "";
}

Value decodeProp(const std::string &s, std::pmr::memory_resource *mr)
{
    if (!s.empty() && s[0] == '[') {
        std::vector<double> vals;
        std::string num;
        for (size_t i = 1; i < s.size(); ++i) {
            if (s[i] == ',' || s[i] == ']') {
                if (!num.empty()) {
                    try { vals.push_back(std::stod(num)); } catch (...) {}
                    num.clear();
                }
            } else num += s[i];
        }
        if (vals.size() == 1)
            return Value::scalar(vals[0], mr);
        if (!vals.empty()) {
            auto r = Value::matrix(1, vals.size(), ValueType::DOUBLE, mr);
            for (size_t i = 0; i < vals.size(); ++i)
                r.doubleDataMut()[i] = vals[i];
            return r;
        }
    }
    try { return Value::scalar(std::stod(s), mr); } catch (...) {}
    return Value::fromString(s, mr);
}

// ── Chart property defaults (MATLAB R2025b documented values) ────────
// Introspection reports MATLAB's defaults even where the numkit renderer
// draws its own palette (get before set); once the user sets a property,
// both get() and the render use the user value.
Value chartDefaultProp(const std::string &type, const std::string &name,
                       std::pmr::memory_resource *mr)
{
    auto str = [&](const char *s) { return Value::fromString(s, mr); };
    auto num = [&](double d) { return Value::scalar(d, mr); };
    auto rgb = [&](double r, double g, double b) {
        auto v = Value::matrix(1, 3, ValueType::DOUBLE, mr);
        v.doubleDataMut()[0] = r; v.doubleDataMut()[1] = g; v.doubleDataMut()[2] = b;
        return v;
    };
    if (name == "Type")       return str(type.c_str());
    if (name == "Visible")    return str("on");
    if (name == "LineWidth")  return num(0.5);
    if (name == "MarkerSize") return num(6.0);
    if (name == "Marker")     return str(type == "scatter" ? "o" : "none");
    if (name == "LineStyle")  return str(type == "scatter" ? "none" : "-");
    if (name == "DisplayName")return str("");
    if (name == "Color") {
        if (type == "scatter") return rgb(0.2902, 0.0627, 0.5294);
        return rgb(0, 0.4470, 0.7410);
    }
    if (name == "SizeData" && type == "scatter") return num(36.0);
    if (name == "BaseValue" && (type == "stem" || type == "area" || type == "bar"))
        return num(0.0);
    if (name == "BarWidth" && type == "bar") return num(0.8);
    if (name == "Children")   return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    return Value();   // unknown → caller decides (empty ⇒ error/blank)
}

// ── Style-string rebuild (render-effect for LineStyle/Marker/Color) ──
// ds.style speaks two dialects (renderer parseLineSpec): the compact
// MATLAB linespec ("r--o") and the kv extras ("color=#hex;lineStyle=--").
// A property set rewrites the string into the kv form, preserving the
// components the user did not change.
struct StyleParts { std::string colorHex, lineStyle, marker; };

StyleParts parseStyle(const std::string &s)
{
    StyleParts p;
    if (s.empty()) return p;
    if (s.find('=') != std::string::npos) {
        for (size_t i = 0; i < s.size();) {
            size_t eq = s.find('=', i);
            if (eq == std::string::npos) break;
            size_t semi = s.find(';', eq);
            std::string key = s.substr(i, eq - i);
            std::string val = s.substr(eq + 1,
                (semi == std::string::npos ? s.size() : semi) - eq - 1);
            if (key == "color") p.colorHex = val;
            else if (key == "lineStyle") p.lineStyle = val;
            else if (key == "marker") p.marker = val;
            i = semi == std::string::npos ? s.size() : semi + 1;
        }
        return p;
    }
    // Compact dialect — mirrors the renderer's STYLE_COLOR map so a
    // rebuilt style keeps the same visual colour the plot was drawn with.
    static const std::map<char, const char *> kCharColor = {
        {'r', "#f07070"}, {'g', "#6ee7a0"}, {'b', "#60d0f0"},
        {'k', "#d4d4f0"}, {'m', "#e070c0"}, {'c', "#60d0f0"},
        {'y', "#e8d060"}, {'w', "#ffffff"},
    };
    size_t i = 0;
    while (i < s.size()) {
        if (p.lineStyle.empty() && s.compare(i, 2, "--") == 0) { p.lineStyle = "--"; i += 2; continue; }
        if (p.lineStyle.empty() && s.compare(i, 2, "-.") == 0)  { p.lineStyle = "-.";  i += 2; continue; }
        char c = s[i];
        if (p.lineStyle.empty() && (c == '-' || c == ':')) { p.lineStyle = std::string(1, c); ++i; continue; }
        if (p.colorHex.empty()) {
            auto hit = kCharColor.find(c);
            if (hit != kCharColor.end()) { p.colorHex = hit->second; ++i; continue; }
        }
        if (p.marker.empty() && std::string("o+*.xsd^v<>ph").find(c) != std::string::npos) {
            p.marker = std::string(1, c); ++i; continue;
        }
        ++i;
    }
    return p;
}

std::string rgbToHex(const Value &v)
{
    auto clamp255 = [](double d) {
        int i = static_cast<int>(d * 255.0 + 0.5);
        if (i < 0) i = 0;
        if (i > 255) i = 255;
        return i;
    };
    char buf[8];
    std::snprintf(buf, sizeof buf, "#%02x%02x%02x",
                  clamp255(v.numel() > 0 ? v.elemAsDouble(0) : 0),
                  clamp255(v.numel() > 1 ? v.elemAsDouble(1) : 0),
                  clamp255(v.numel() > 2 ? v.elemAsDouble(2) : 0));
    return buf;
}

// ── Render-effect: apply one property to the backing DatasetInfo ─────
void applyChartProp(FigureManager &fm, FigureManager::HandleRecord *rec,
                    const std::string &name, const Value &val)
{
    DatasetInfo *ds = fm.datasetOf(*rec);
    if (!ds)
        return;
    bool touched = true;
    if (name == "LineWidth" && val.isNumeric() && val.numel() == 1) {
        double w = val.toScalar();
        ds->lineWidth = w > 0 ? w : 0.0;   // 0/negative = default
    } else if (name == "MarkerSize" && val.isNumeric() && val.numel() == 1) {
        double m = val.toScalar();
        ds->markerSize = m > 0 ? m : 0.0;
    } else if (name == "LineStyle" || name == "Marker" || name == "Color") {
        StyleParts p = parseStyle(ds->style);
        if (name == "LineStyle") p.lineStyle = val.toString();
        else if (name == "Marker") p.marker = val.toString();
        else p.colorHex = rgbToHex(val);
        std::string out;
        auto append = [&](const char *k, const std::string &v) {
            if (v.empty() || v == "none") return;
            if (!out.empty()) out += ";";
            out += std::string(k) + "=" + v;
        };
        append("color", p.colorHex);
        append("lineStyle", p.lineStyle);
        append("marker", p.marker);
        ds->style = out;   // empty ⇒ renderer defaults (palette line, no marker)
    } else if (name == "Visible") {
        ds->visible = !(val.isChar() || val.isString()) || val.toString() != "off";
    } else if (name == "DisplayName") {
        ds->label = val.isChar() || val.isString() ? val.toString() : "";
    } else if ((name == "XData" || name == "YData") && val.isNumeric()) {
        // Replace the series. Huge series: drop the downsample state and
        // re-decide via maybeDownsampleSeries (rare path, correctness first).
        std::vector<double> xs(val.numel());
        for (size_t i = 0; i < val.numel(); ++i)
            xs[i] = val.elemAsDouble(i);
        ds->xRaw.clear(); ds->yRaw.clear(); ds->seriesPyramid.clear();
        ds->seriesDownsampled = false; ds->seriesN = 0;
        (name == "XData" ? ds->xJson : ds->yJson) =
            figdetail::serializeFlatDoubles(xs);
        if (ds->xJson.empty()) ds->xJson = "[]";
        if (ds->yJson.empty()) ds->yJson = "[]";
        fm.maybeDownsampleSeries(*ds);
    } else {
        touched = false;   // stored in rec->props only
    }
    if (touched)
        fm.markFigureModified(rec->figureId);
}

// Auto tick positions for get(gca,'XTick') in 'auto' mode — the SAME
// niceTicks the renderer uses, over the explicit xlim if set, else the
// data range (portion 34; MATLAB returns the drawn ticks).
std::vector<double> axesAutoTicks(FigureManager &fm,
                                  FigureManager::HandleRecord *rec, char axis)
{
    AxesState *ax = fm.axesOf(*rec);
    if (!ax)
        return {};
    const bool log = (axis == 'x' ? ax->xscale : ax->yscale) == "log";
    double lo = 0, hi = 1;
    const std::string &lim = axis == 'x' ? ax->xlimJson : ax->ylimJson;
    if (!lim.empty()) {
        auto v = figdetail::parseFlatDoubles(lim);
        if (v.size() == 2) {
            lo = v[0];
            hi = v[1];
        }
    } else {
        lo = +INFINITY;
        hi = -INFINITY;
        for (const auto &ds : ax->datasets) {
            if (!ds.visible)
                continue;
            const std::string &j = axis == 'x' ? ds.xJson : ds.yJson;
            for (double v : figdetail::parseFlatDoubles(j))
                if (std::isfinite(v)) {
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
        }
        if (!(hi > lo)) {
            lo = 0;
            hi = 1;
        }
        double d = (hi - lo) * 0.05;
        lo -= d;
        hi += d;
    }
    if (log) {
        if (!(lo > 0) || !(hi > lo))
            return {};
        return figrender::logTicks(lo, hi);
    }
    return figrender::niceTicks(lo, hi);
}

// MATLAB R2025b (probe-verified): setting the AXES font RESETS even an
// explicitly-set label-Text font — the texts fall back to the axes
// values (FontSize ×1.1). Drop their stored font props.
void resetAxesTextFonts(FigureManager &fm, FigureManager::HandleRecord *rec)
{
    for (auto &[id, r] : fm.handles())
        if (!r.deleted && r.type == "text" && r.figureId == rec->figureId
            && r.axesIndex == rec->axesIndex) {
            r.props.erase("FontSize");
            r.props.erase("FontWeight");
            r.props.erase("FontName");
        }
}

// tick vector Value ← json "[a,b,c]"
Value decodeTickVec(const std::string &json, std::pmr::memory_resource *mr)
{
    auto v = figdetail::parseFlatDoubles(json);
    if (v.empty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto out = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < v.size(); ++i)
        out.doubleDataMut()[i] = v[i];
    return out;
}

// ["a","b"] json → 1×N cell of char.
Value decodeTickLabels(const std::string &json, std::pmr::memory_resource *mr)
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
    if (labels.empty())
        return Value::cell(0, 0, mr);
    Value c = Value::cell(1, labels.size(), mr);
    for (size_t k = 0; k < labels.size(); ++k)
        c.cellAt(k) = Value::fromString(labels[k], mr);
    return c;
}

// cell/string-array/char → ["a","b"] json.
std::string encodeTickLabels(const Value &v)
{
    std::string out = "[";
    for (size_t i = 0; i < v.numel(); ++i) {
        if (i)
            out += ",";
        out += "\"" + v.cellAt(i).toString() + "\"";
    }
    return out + "]";
}

// ── Axes / figure property mapping (render-effect) ───────────────────
bool applyAxesProp(FigureManager &fm, FigureManager::HandleRecord *rec,
                   const std::string &name, const Value &val)
{
    AxesState *ax = fm.axesOf(*rec);
    if (!ax)
        return false;
    std::vector<double> v(val.isNumeric() ? val.numel() : 0);
    for (size_t i = 0; i < v.size(); ++i)
        v[i] = val.elemAsDouble(i);
    auto limJson = [](const std::vector<double> &w) {
        if (w.size() != 2) return std::string();
        return "[" + formatPropDouble(w[0]) + "," + formatPropDouble(w[1]) + "]";
    };
    bool touched = true;
    if      (name == "XLim")  ax->xlimJson  = limJson(v);
    else if (name == "YLim")  ax->ylimJson  = limJson(v);
    else if (name == "ZLim")  ax->zlimJson  = limJson(v);
    else if (name == "XScale") ax->xscale = val.toString();
    else if (name == "YScale") ax->yscale = val.toString();
    else if (name == "XGrid") ax->gridMajor = val.toString() == "on";
    else if (name == "YGrid") ax->gridMajor = val.toString() == "on";
    else if (name == "XLabel") ax->xlabel = val.toString();
    else if (name == "YLabel") ax->ylabel = val.toString();
    else if (name == "ZLabel") ax->zlabel = val.toString();
    else if (name == "Title")  ax->title  = val.toString();
    // ── Portion 34 (MATLAB R2025b probe-verified semantics) ──
    else if (name == "FontSize" && val.isNumeric() && v.size() == 1) {
        ax->fontSize = v[0];
        resetAxesTextFonts(fm, rec);   // label Texts follow the axes font
    }
    else if (name == "FontWeight") {
        ax->fontWeight = val.toString();
        resetAxesTextFonts(fm, rec);
    }
    else if (name == "FontName") {
        ax->fontName = val.toString();
        resetAxesTextFonts(fm, rec);
    }
    else if (name == "LineWidth" && val.isNumeric() && v.size() == 1)
        ax->lineWidth = v[0];   // axes frame; series LineWidth is per-chart
    else if ((name == "XTick" || name == "YTick" || name == "ZTick")
             && (val.isNumeric() || val.isEmpty())) {
        std::ostringstream os;
        os << '[';
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) os << ',';
            os << formatPropDouble(v[i]);
        }
        os << ']';
        (name == "XTick" ? ax->xTicksJson
         : name == "YTick" ? ax->yTicksJson : ax->zTicksJson) = os.str();
        (name == "XTick" ? ax->xTicksAuto
         : name == "YTick" ? ax->yTicksAuto : ax->zTicksAuto) = false;
    }
    else if ((name == "XTickLabel" || name == "YTickLabel"
              || name == "ZTickLabel")
             && (val.isCell() || val.isChar() || val.isString())) {
        std::string json = val.isCell() ? encodeTickLabels(val)
                                        : "[\"" + val.toString() + "\"]";
        (name == "XTickLabel" ? ax->xTickLabelsJson
         : name == "YTickLabel" ? ax->yTickLabelsJson
                                : ax->zTickLabelsJson) = json;
    }
    else if (name == "XTickMode" || name == "YTickMode"
             || name == "ZTickMode") {
        bool auto_ = val.toString() == "auto";
        if (name == "XTickMode") { ax->xTicksAuto = auto_; if (auto_) ax->xTicksJson.clear(); }
        if (name == "YTickMode") { ax->yTicksAuto = auto_; if (auto_) ax->yTicksJson.clear(); }
        if (name == "ZTickMode") { ax->zTicksAuto = auto_; if (auto_) ax->zTicksJson.clear(); }
    }
    else if (name == "XTickLabelMode" || name == "YTickLabelMode"
             || name == "ZTickLabelMode") {
        bool auto_ = val.toString() == "auto";
        if (name == "XTickLabelMode" && auto_) ax->xTickLabelsJson.clear();
        if (name == "YTickLabelMode" && auto_) ax->yTickLabelsJson.clear();
        if (name == "ZTickLabelMode" && auto_) ax->zTickLabelsJson.clear();
    }
    else if (name == "CLim" && v.size() == 2)
        ax->climJson = limJson(v);
    else touched = false;
    if (touched)
        fm.markFigureModified(rec->figureId);
    return touched;
}

Value axesGetProp(FigureManager &fm, FigureManager::HandleRecord *rec,
                  const std::string &name, std::pmr::memory_resource *mr)
{
    auto str = [&](const char *s) { return Value::fromString(s, mr); };
    auto num = [&](double d) { return Value::scalar(d, mr); };
    if (name == "Type") return str("axes");
    AxesState *ax = fm.axesOf(*rec);
    if (!ax)
        return Value();
    if (name == "XLim" && !ax->xlimJson.empty()) return decodeProp(ax->xlimJson, mr);
    if (name == "YLim" && !ax->ylimJson.empty()) return decodeProp(ax->ylimJson, mr);
    if (name == "ZLim" && !ax->zlimJson.empty()) return decodeProp(ax->zlimJson, mr);
    if (name == "XScale") return str(ax->xscale.c_str());
    if (name == "YScale") return str(ax->yscale.c_str());
    if (name == "XLabel") return str(ax->xlabel.c_str());
    if (name == "YLabel") return str(ax->ylabel.c_str());
    if (name == "ZLabel") return str(ax->zlabel.c_str());
    if (name == "Title")  return str(ax->title.c_str());
    // ── Portion 34 live reads (MATLAB R2025b probe-verified) ──
    if (name == "FontSize")   return num(ax->fontSize);
    if (name == "FontWeight") return str(ax->fontWeight.c_str());
    if (name == "FontName")   return str(ax->fontName.c_str());
    if (name == "LineWidth")  return num(ax->lineWidth);
    if (name == "XTick") {
        if (!ax->xTicksAuto)
            return decodeTickVec(ax->xTicksJson, mr);
        auto v = axesAutoTicks(fm, rec, 'x');
        if (v.empty())
            return Value::matrix(0, 0, ValueType::DOUBLE, mr);
        auto out = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
        for (size_t i = 0; i < v.size(); ++i)
            out.doubleDataMut()[i] = v[i];
        return out;
    }
    if (name == "YTick") {
        if (!ax->yTicksAuto)
            return decodeTickVec(ax->yTicksJson, mr);
        auto v = axesAutoTicks(fm, rec, 'y');
        if (v.empty())
            return Value::matrix(0, 0, ValueType::DOUBLE, mr);
        auto out = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
        for (size_t i = 0; i < v.size(); ++i)
            out.doubleDataMut()[i] = v[i];
        return out;
    }
    if (name == "XTickMode")
        return str(ax->xTicksAuto ? "auto" : "manual");
    if (name == "YTickMode")
        return str(ax->yTicksAuto ? "auto" : "manual");
    if (name == "XTickLabelMode")
        return str(ax->xTickLabelsJson.empty() ? "auto" : "manual");
    if (name == "YTickLabelMode")
        return str(ax->yTickLabelsJson.empty() ? "auto" : "manual");
    if (!ax->xTickLabelsJson.empty() && name == "XTickLabel")
        return decodeTickLabels(ax->xTickLabelsJson, mr);
    if (!ax->yTickLabelsJson.empty() && name == "YTickLabel")
        return decodeTickLabels(ax->yTickLabelsJson, mr);
    if (name == "CLim") {
        if (!ax->climJson.empty())
            return decodeProp(ax->climJson, mr);
        // auto: imagesc cmin/cmax if present, else MATLAB's [0 1].
        for (const auto &ds : ax->datasets)
            if (!ds.zQuantized.empty()) {
                auto c = Value::matrix(1, 2, ValueType::DOUBLE, mr);
                c.doubleDataMut()[0] = ds.cminOrig;
                c.doubleDataMut()[1] = ds.cmaxOrig;
                return c;
            }
        auto c = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        c.doubleDataMut()[0] = 0;
        c.doubleDataMut()[1] = 1;
        return c;
    }
    if (name == "CLimMode")
        return str(ax->climJson.empty() ? "auto" : "manual");
    if (name == "YDir")
        return str(ax->yDir.empty() ? "normal" : ax->yDir.c_str());
    if (name == "XDir")
        return str(ax->xDir.empty() ? "normal" : ax->xDir.c_str());
    return Value();
}

// XData/YData for chart records read from the live dataset (xRaw holds
// the full series when it was downsampled; xJson only the preview).
Value chartDataProp(FigureManager &fm, FigureManager::HandleRecord *rec,
                    const std::string &name, std::pmr::memory_resource *mr)
{
    if (name != "XData" && name != "YData")
        return Value();
    DatasetInfo *ds = fm.datasetOf(*rec);
    if (!ds)
        return Value();
    const std::vector<double> &raw = (name == "XData") ? ds->xRaw : ds->yRaw;
    std::vector<double> vals;
    if (!raw.empty()) {
        vals = raw;
    } else {
        vals = figdetail::parseFlatDoubles(
            (name == "XData") ? ds->xJson : ds->yJson);
    }
    if (vals.empty())
        return Value();
    auto v = Value::matrix(1, vals.size(), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < vals.size(); ++i)
        v.doubleDataMut()[i] = vals[i];
    return v;
}

// ── Handle record lookup helpers (create-or-reuse) ───────────────────
int handleIdOfRecord(FigureManager &fm, FigureManager::HandleRecord *rec)
{
    for (auto &[id, r] : fm.handles())
        if (&r == rec)
            return id;
    return -1;
}

FigureManager::HandleRecord *findRecordBy(FigureManager &fm, const char *type,
                                          int figureId, size_t axesIndex)
{
    for (auto &[id, rec] : fm.handles())
        if (!rec.deleted && rec.type == type && rec.figureId == figureId
            && rec.axesIndex == axesIndex)
            return &rec;
    return nullptr;
}

Value makeAxesHandle(FigureManager &fm, std::pmr::memory_resource *mr)
{
    FigureState &fig = fm.current();
    const int fid = fm.currentFigureId();
    const size_t aidx = static_cast<size_t>(fig.currentAxes);
    if (auto *rec = findRecordBy(fm, "axes", fid, aidx))
        return makeGraphicsHandleValue(kAxesClass, handleIdOfRecord(fm, rec), mr);
    const int id = fm.createHandle("axes", kAxesClass);
    return makeGraphicsHandleValue(kAxesClass, id, mr);
}

Value makeFigureHandleFor(FigureManager &fm, int figureId,
                          std::pmr::memory_resource *mr)
{
    if (auto *rec = findRecordBy(fm, "figure", figureId, 0))
        return makeGraphicsHandleValue(kFigureClass, handleIdOfRecord(fm, rec), mr);
    // createHandle stamps the CURRENT figure — switch, create, switch back.
    const int saved = fm.currentFigureId();
    if (saved != figureId)
        fm.setFigure(figureId);
    const int id = fm.createHandle("figure", kFigureClass);
    if (saved != figureId)
        fm.setFigure(saved);
    return makeGraphicsHandleValue(kFigureClass, id, mr);
}

Value makeFigureHandle(FigureManager &fm, std::pmr::memory_resource *mr)
{
    fm.current();   // ensure the state exists
    return makeFigureHandleFor(fm, fm.currentFigureId(), mr);
}

// Axes handle at an EXPLICIT index (gcf.Children addresses non-current
// subplot cells too).
Value makeAxesHandleFor(FigureManager &fm, int figureId, size_t axesIndex,
                        std::pmr::memory_resource *mr)
{
    if (auto *rec = findRecordBy(fm, "axes", figureId, axesIndex))
        return makeGraphicsHandleValue(kAxesClass, handleIdOfRecord(fm, rec), mr);
    const int id = fm.createHandleAt(figureId, axesIndex, "axes", kAxesClass);
    return makeGraphicsHandleValue(kAxesClass, id, mr);
}

// Text handle bound to a label slot ("sgtitle" is figure-scoped: kNoAxes
// so scoped axes sweeps keep it alive while clf/close invalidate it).
Value makeTextHandleFor(FigureManager &fm, int figureId, size_t axesIndex,
                        const std::string &slot, std::pmr::memory_resource *mr)
{
    for (auto &[id, rec] : fm.handles())
        if (!rec.deleted && rec.type == "text" && rec.figureId == figureId
            && rec.slot == slot && rec.axesIndex == axesIndex)
            return makeGraphicsHandleValue(kTextClass, id, mr);
    const int id = fm.createHandleAt(figureId, axesIndex, "text", kTextClass,
                                     slot);
    return makeGraphicsHandleValue(kTextClass, id, mr);
}

Value makeLegendHandleFor(FigureManager &fm, int figureId, size_t axesIndex,
                          std::pmr::memory_resource *mr)
{
    if (auto *rec = findRecordBy(fm, "legend", figureId, axesIndex))
        return makeGraphicsHandleValue(kLegendClass, handleIdOfRecord(fm, rec), mr);
    const int id = fm.createHandleAt(figureId, axesIndex, "legend", kLegendClass);
    return makeGraphicsHandleValue(kLegendClass, id, mr);
}

// Root (groot) — ONE global record (figureId 0 never matches a real
// figure, so figure sweeps leave it alone).
Value makeRootHandle(FigureManager &fm, std::pmr::memory_resource *mr)
{
    for (auto &[id, rec] : fm.handles())
        if (!rec.deleted && rec.type == "root")
            return makeGraphicsHandleValue(kRootClass, id, mr);
    const int id = fm.createHandleAt(0, FigureManager::HandleRecord::kNoAxes,
                                     "root", kRootClass);
    return makeGraphicsHandleValue(kRootClass, id, mr);
}

// Array of handles (gcf.Children / groot.Children).
Value makeHandleArray(const std::string &className,
                      const std::vector<int> &ids,
                      std::pmr::memory_resource *mr)
{
    if (ids.empty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    std::vector<std::shared_ptr<ObjectState>> states;
    states.reserve(ids.size());
    for (int id : ids) {
        auto payload = std::make_shared<GraphicsHandlePayload>();
        payload->handleId = id;
        auto st = std::make_shared<ObjectState>(mr);
        st->native = std::move(payload);
        states.push_back(std::move(st));
    }
    Dims d{1, ids.size()};
    return Value::objectArray(className, d, std::move(states),
                              /*isHandle=*/true, mr);
}

// ── Unified record property access ───────────────────────────────────
// ONE body shared by get(h,'Prop') / set(h,'Prop',v) AND the dot-syntax
// h.Prop / h.Prop = v (BuiltinClass propGet/propSet). Live state fields
// (Text.String, Legend.Location/Box/Visible, chart XData) read from the
// figure state; everything user-set lives in rec->props; documented
// MATLAB defaults fill the rest.
Value recordGetProp(FigureManager &fm, FigureManager::HandleRecord *rec,
                    const std::string &name, std::pmr::memory_resource *mr,
                    bool strict = false)
{
    auto str = [&](const char *s) { return Value::fromString(s, mr); };
    auto num = [&](double d) { return Value::scalar(d, mr); };
    auto rgb = [&](double r, double g, double b) {
        auto v = Value::matrix(1, 3, ValueType::DOUBLE, mr);
        v.doubleDataMut()[0] = r; v.doubleDataMut()[1] = g; v.doubleDataMut()[2] = b;
        return v;
    };
    if (rec->type == "root") {
        if (name == "Type")   return str("root");
        if (name == "Children") {
            std::vector<int> ids;
            for (const auto &[fid, fig] : fm.figures()) {
                (void)fig;
                int hid = -1;
                for (auto &[id, r] : fm.handles())
                    if (!r.deleted && r.type == "figure" && r.figureId == fid) {
                        hid = id;
                        break;
                    }
                if (hid < 0)
                    hid = fm.createHandleAt(fid, 0, "figure", kFigureClass);
                ids.push_back(hid);
            }
            return makeHandleArray(kFigureClass, ids, mr);
        }
        auto it = rec->props.find(name);
        return it != rec->props.end() ? decodeProp(it->second, mr) : Value();
    }
    if (rec->type == "text") {
        // Live String from the label slot (the render source of truth).
        if (name == "String") {
            std::string *f = textFieldOf(fm, *rec);
            return str(f ? f->c_str() : "");
        }
        if (name == "Type")       return str("text");
        if (name == "Parent") {
            // sgtitle's parent is the Figure; other slots the Axes.
            if (rec->slot == "sgtitle")
                return makeFigureHandleFor(fm, rec->figureId, mr);
            return makeAxesHandleFor(fm, rec->figureId, rec->axesIndex, mr);
        }
        auto it = rec->props.find(name);
        if (it != rec->props.end())
            return decodeProp(it->second, mr);
        // MATLAB R2025b (probe-verified): label Texts inherit the AXES
        // font — FontSize at 1.1× — unless the axes font was set (which
        // RESETS any explicit Text font, see resetAxesTextFonts).
        AxesState *ax = rec->slot == "sgtitle" ? nullptr
                                               : fm.axesOf(*rec);
        if (name == "FontSize")
            return num(ax ? ax->fontSize * 1.1 : 11.0);
        if (name == "FontWeight")
            return str(ax ? ax->fontWeight.c_str() : "normal");
        if (name == "FontName")
            return str(ax ? ax->fontName.c_str() : "Helvetica");
        if (name == "Visible")    return str("on");
        if (name == "Color")      return rgb(0.15, 0.15, 0.15);
        return Value();
    }
    if (rec->type == "legend") {
        AxesState *ax = fm.axesOf(*rec);
        if (name == "String") {
            size_t n = ax ? ax->legendLabels.size() : 0;
            if (!n)
                return Value::cell(0, 0, mr);
            Value c = Value::cell(1, n, mr);
            for (size_t i = 0; i < n; ++i)
                c.cellAt(i) = Value::fromString(ax->legendLabels[i], mr);
            return c;
        }
        if (name == "Type") return str("legend");
        if (name == "Parent")
            return makeAxesHandleFor(fm, rec->figureId, rec->axesIndex, mr);
        // Live render state first — a stored prop would shadow later
        // command-form changes (`legend show` after set(hL,'Visible','off')).
        if (name == "Location")
            return str(ax && !ax->legendLocation.empty()
                           ? ax->legendLocation.c_str() : "northeast");
        if (name == "Box")  return str(!ax || ax->legendBoxOn ? "on" : "off");
        if (name == "Visible")
            return str(!ax || !ax->legendHidden ? "on" : "off");
        auto it = rec->props.find(name);
        if (it != rec->props.end())
            return decodeProp(it->second, mr);
        return Value();
    }
    if (rec->type == "figure") {
        if (name == "Type")   return str("figure");
        if (name == "Number") return num(rec->figureId);
        if (name == "Parent") return makeRootHandle(fm, mr);
        if (name == "CurrentAxes")
            return makeAxesHandle(fm, mr);
        if (name == "Children") {
            FigureState *fig = fm.figureOf(rec->figureId);
            const size_t n = fig ? fig->axes.size() : 0;
            std::vector<int> ids;
            for (size_t k = 0; k < n; ++k) {
                int hid = -1;
                for (auto &[id, r] : fm.handles())
                    if (!r.deleted && r.type == "axes"
                        && r.figureId == rec->figureId && r.axesIndex == k) {
                        hid = id;
                        break;
                    }
                if (hid < 0)
                    hid = fm.createHandleAt(rec->figureId, k, "axes", kAxesClass);
                ids.push_back(hid);
            }
            return makeHandleArray(kAxesClass, ids, mr);
        }
        auto it = rec->props.find(name);
        return it != rec->props.end()
                   ? decodeProp(it->second, mr)
                   : (strict ? Value() : Value::fromString("", mr));
    }
    if (rec->type == "axes") {
        if (name == "Parent")
            return makeFigureHandleFor(fm, rec->figureId, mr);
        // Label slots return Text handles (MATLAB), not strings.
        const std::string &slot = textSlotOfProp(name);
        if (!slot.empty())
            return makeTextHandleFor(fm, rec->figureId, rec->axesIndex, slot, mr);
        Value v = axesGetProp(fm, rec, name, mr);
        if (!v.isEmpty())
            return v;
        auto it = rec->props.find(name);
        return it != rec->props.end()
                   ? decodeProp(it->second, mr)
                   : (strict ? Value() : Value::fromString("", mr));
    }
    // Chart records.
    if (name == "Parent")
        return makeAxesHandleFor(fm, rec->figureId, rec->axesIndex, mr);
    auto it = rec->props.find(name);
    if (it != rec->props.end())
        return decodeProp(it->second, mr);
    {
        Value v = chartDataProp(fm, rec, name, mr);
        if (!v.isEmpty())
            return v;
    }
    // An EXPLICIT linespec from the plot call (plot(x,y,'r--o')) is
    // user intent and reads back MATLAB-exact — unlike the soft series
    // palette the renderer draws. Colors map to MATLAB's table
    // ('r'==[1 0 0]), kv hex to /255.
    if (name == "Color" || name == "LineStyle" || name == "Marker") {
        DatasetInfo *ds = fm.datasetOf(*rec);
        if (ds && !ds->style.empty()) {
            const std::string &s = ds->style;
            auto rgb = [&](double r, double g, double b) {
                auto v = Value::matrix(1, 3, ValueType::DOUBLE, mr);
                v.doubleDataMut()[0] = r;
                v.doubleDataMut()[1] = g;
                v.doubleDataMut()[2] = b;
                return v;
            };
            auto strv = [&](const char *t) {
                return Value::fromString(t, mr);
            };
            if (name == "LineStyle") {
                if (s.find("--") != std::string::npos)
                    return strv("--");
                if (s.find("-.") != std::string::npos)
                    return strv("-.");
                if (s.find(':') != std::string::npos)
                    return strv(":");
            } else if (name == "Marker") {
                static const char *kMarkers = "o+*.xsd^v<>ph";
                for (char mc : s)
                    if (mc && std::strchr(kMarkers, mc))
                        return strv(std::string(1, mc).c_str());
            } else {
                struct ColEntry { char c; double rgb[3]; };
                static const ColEntry kCol[] = {
                    {'r', {1, 0, 0}},       {'g', {0, 0.5, 0}},
                    {'b', {0, 0, 1}},       {'k', {0, 0, 0}},
                    {'w', {1, 1, 1}},       {'y', {1, 1, 0}},
                    {'m', {1, 0, 1}},       {'c', {0, 1, 1}},
                };
                size_t hp = s.find("color=#");
                if (hp != std::string::npos) {
                    unsigned R = 0, G = 0, B = 0;
                    if (std::sscanf(s.c_str() + hp + 7, "%02x%02x%02x", &R,
                                    &G, &B)
                        == 3)
                        return rgb(R / 255.0, G / 255.0, B / 255.0);
                } else {
                    // kv color= segment or a bare linespec letter; the
                    // single-letter form must not collide with markers
                    // first (handled above), so scan for a colour char.
                    size_t cp = s.find("color=");
                    std::string cand;
                    if (cp != std::string::npos) {
                        size_t e = s.find(';', cp);
                        cand = s.substr(cp + 6,
                                        e == std::string::npos
                                            ? std::string::npos
                                            : e - cp - 6);
                    } else if (s.size() == 1) {
                        cand = s;
                    } else {
                        for (char ch : s)
                            if (std::strchr("rgbcymwkv", ch)) {
                                cand = std::string(1, ch);
                                break;
                            }
                    }
                    if (cand.size() == 1) {
                        for (const auto &cc : kCol)
                            if (cand[0] == cc.c)
                                return rgb(cc.rgb[0], cc.rgb[1],
                                           cc.rgb[2]);
                    }
                    if (cand.size() > 1 && cand[0] == '#'
                        && cand.size() >= 7) {
                        unsigned R = 0, G = 0, B = 0;
                        if (std::sscanf(cand.c_str() + 1, "%02x%02x%02x",
                                        &R, &G, &B)
                            == 3)
                            return rgb(R / 255.0, G / 255.0, B / 255.0);
                    }
                }
            }
        }
    }
    Value d = chartDefaultProp(rec->type, name, mr);
    if (!d.isEmpty())
        return d;
    return strict ? Value() : Value::fromString("", mr);
}

// Returns true when the property is known/accepted (stored at minimum).
bool recordSetProp(FigureManager &fm, FigureManager::HandleRecord *rec,
                   const std::string &name, const Value &val)
{
    rec->props[name] = encodeProp(val);
    if (rec->type == "text") {
        if (name == "String") {
            std::string *f = textFieldOf(fm, *rec);
            if (f) {
                *f = (val.isChar() || val.isString()) ? val.toString() : "";
                fm.markFigureModified(rec->figureId);
            }
        }
        return true;   // FontSize/Color/… store-only (renderer gap filed)
    }
    if (rec->type == "legend") {
        AxesState *ax = fm.axesOf(*rec);
        if (!ax)
            return true;
        if (name == "Location") {
            ax->legendLocation = val.toString();
            fm.markFigureModified(rec->figureId);
        } else if (name == "Box") {
            ax->legendBoxOn = val.toString() != "off";
            fm.markFigureModified(rec->figureId);
        } else if (name == "Visible") {
            ax->legendHidden = val.toString() == "off";
            fm.markFigureModified(rec->figureId);
        } else if (name == "String" && val.isCell()) {
            ax->legendLabels.clear();
            for (size_t i = 0; i < val.numel(); ++i)
                ax->legendLabels.push_back(val.cellAt(i).toString());
            fm.markFigureModified(rec->figureId);
        }
        return true;
    }
    if (rec->type == "root")
        return true;   // root properties are read-only at this layer
    if (rec->type == "axes")
        applyAxesProp(fm, rec, name, val);
    else
        applyChartProp(fm, rec, name, val);
    return true;
}

} // anonymous namespace

// ============================================================
// Handle-aware accessor registration (set/get/isgraphics/ishandle/
// isvalid — delete lives in general_reg.cpp, type-dispatched)
// ============================================================
namespace {

void registerHandleAccessors(Engine &engine)
{
    // set(h, …)
    {
        ExternalFunc fn = [&engine](
                Span<const Value> args, size_t,
                Span<Value> outs, CallContext &) {
            if (args.empty()) { outs[0] = Value(); return; }
            const int id = graphicsHandleIdOf(args[0]);
            if (id < 0) { outs[0] = Value(); return; }
            auto *rec = engine.figureManager().findHandle(id);
            if (!rec)
                throw std::runtime_error("Invalid or deleted object.");
            for (size_t i = 1; i + 1 < args.size(); i += 2) {
                if (!args[i].isChar() && !args[i].isString()) continue;
                recordSetProp(engine.figureManager(), rec,
                              args[i].toString(), args[i + 1]);
            }
            engine.figureManager().emitModified();
            outs[0] = Value();
        };
        engine.registerFunction("", "set", std::move(fn));
    }
    // get(h [, 'Prop']) — without a property name: struct of all
    {
        ExternalFunc fn = [&engine](
                Span<const Value> args, size_t,
                Span<Value> outs, CallContext &) {
            if (args.empty()) { outs[0] = Value(); return; }
            const int id = graphicsHandleIdOf(args[0]);
            if (id < 0) {
                // Not a handle (legacy figure-number contract) → 1.
                outs[0] = Value::scalar(1.0, engine.resource());
                return;
            }
            auto *rec = engine.figureManager().findHandle(id);
            if (!rec)
                throw std::runtime_error("Invalid or deleted object.");
            if (args.size() >= 2 && (args[1].isChar() || args[1].isString())) {
                outs[0] = recordGetProp(engine.figureManager(), rec,
                                        args[1].toString(), engine.resource());
                return;
            }
            // get(h) → struct: stored props ∪ documented defaults.
            auto *mr = engine.resource();
            Value st = Value::structure(mr);
            std::map<std::string, std::string> fields(rec->props);
            static const char *kCommon[] = {
                "Type", "Color", "LineStyle", "LineWidth", "Marker",
                "MarkerSize", "Visible", "Parent", "Children",
            };
            for (const char *p : kCommon) {
                if (fields.count(p)) continue;
                Value d = rec->isChart() ? chartDefaultProp(rec->type, p, mr)
                                         : Value();
                if (!d.isEmpty())
                    fields[p] = encodeProp(d);
            }
            if (rec->type == "figure") {
                fields["Number"] = std::to_string(rec->figureId);
                fields["Type"] = "figure";
            } else if (rec->type == "axes") {
                fields["Type"] = "axes";
                static const char *kLayout[] = {
                    "XLim", "YLim", "XScale", "YScale", "XLabel", "YLabel",
                    "Title",
                };
                for (const char *p : kLayout) {
                    Value v = axesGetProp(engine.figureManager(), rec, p, mr);
                    if (!v.isEmpty())
                        fields[p] = encodeProp(v);
                }
            } else if (rec->type == "text") {
                fields["Type"] = "text";
                Value v = recordGetProp(engine.figureManager(), rec, "String", mr);
                if (!v.isEmpty())
                    fields["String"] = encodeProp(v);
            } else if (rec->type == "legend") {
                fields["Type"] = "legend";
                fields["Location"] = encodeProp(
                    recordGetProp(engine.figureManager(), rec, "Location", mr));
                fields["Visible"] = encodeProp(
                    recordGetProp(engine.figureManager(), rec, "Visible", mr));
            }
            for (const auto &[k, v] : fields)
                st.setFieldAll(k, decodeProp(v, mr));
            outs[0] = std::move(st);
        };
        engine.registerFunction("", "get", std::move(fn));
    }
    // findobj([h][, 'Prop', value, ...] | '-property', Name) — MATLAB
    // R2025b probes: no handle → ROOT scope (every record across
    // figures, registry order); a figure/axes handle scopes to its
    // descendants; 'Type' matches the MATLAB class family
    // (line/axes/figure/text/legend); Name-Value pairs compare via
    // recordGetProp (chars exact, numerics element-wise within 1e-9).
    // Deferred: '-regexp', '-depth', hidden handles.
    {
        ExternalFunc fn = [&engine](
                Span<const Value> args, size_t, Span<Value> outs,
                CallContext &ctx) {
            auto &fm = ctx.engine->figureManager();
            auto *mr = ctx.engine->resource();

            // Scope: an optional leading handle narrows the search.
            int scopeFig = -1;      // -1 = root scope
            int scopeAxes = -1;
            int scopeSelf = -1;      // chart-handle self-scope
            size_t first = 0;
            if (!args.empty()) {
                int hid = graphicsHandleIdOf(args[0]);
                if (hid >= 0) {
                    auto *rec = fm.findHandle(hid);
                    if (rec) {
                        if (rec->type == "figure")
                            scopeFig = rec->figureId;
                        else if (rec->type == "axes") {
                            scopeFig = rec->figureId;
                            scopeAxes = (int)rec->axesIndex;
                        } else {
                            // chart/text/legend handle: MATLAB returns
                            // the handle itself (no children modeled).
                            if (!args.empty() && args.size() > 1
                                && args[1].isChar()) {
                                // has filters — evaluate them on itself
                                scopeFig = rec->figureId;
                                scopeAxes = (int)rec->axesIndex;
                                scopeSelf = hid;
                            } else {
                                outs[0] = args[0];
                                return;
                            }
                        }
                        first = 1;
                    }
                }
            }

            // Filters.
            struct Filt {
                std::string prop;   // "Type" or a property name
                Value want;
                bool isPropTest;    // '-property' Name
            };
            std::vector<Filt> filts;
            for (size_t i = first; i < args.size(); ++i) {
                if (!args[i].isChar())
                    continue;
                std::string key = args[i].toString();
                for (auto &c : key)
                    c = (char)std::tolower((unsigned char)c);
                if (key == "-property" && i + 1 < args.size()
                    && args[i + 1].isChar()) {
                    Filt f;
                    f.isPropTest = true;
                    f.prop = args[++i].toString();
                    filts.push_back(std::move(f));
                } else if (key == "-regexp" || key == "-depth") {
                    // skip their option payload; unsupported, ignored
                    ++i;
                } else if (i + 1 < args.size()) {
                    Filt f;
                    f.isPropTest = false;
                    f.prop = args[i].toString();
                    f.want = args[i + 1];
                    ++i;
                    filts.push_back(std::move(f));
                }
            }

            auto matlabType = [](const std::string &t) {
                if (t == "figure" || t == "axes" || t == "text"
                    || t == "line" || t == "legend" || t == "image")
                    return t;
                // chart records carry dataset-ish types (scatter, bar…)
                return std::string("line");
            };

            std::vector<int> hits;
            for (auto &kv : fm.handles()) {
                auto *rec = &kv.second;
                if (!rec || rec->deleted)
                    continue;
                // MATLAB hides title/label text objects from findobj
                // (hidden handles; probe: findobj('Type','text') == 0
                // next to a title). Slot-bound Texts follow suit;
                // user text() stays findable.
                if (rec->type == "text" && !rec->slot.empty())
                    continue;
                if (scopeFig >= 0 && rec->figureId != scopeFig)
                    continue;
                if (scopeSelf >= 0) {
                    if (kv.first != scopeSelf)
                        continue;
                } else if (scopeAxes >= 0
                           && rec->axesIndex != (size_t)scopeAxes
                           && !(rec->type == "figure")) {
                    continue;
                }
                bool pass = true;
                for (const auto &f : filts) {
                    if (f.isPropTest) {
                        Value v = recordGetProp(fm, rec, f.prop, mr);
                        if (v.isUnset() || v.isEmpty()) {
                            pass = false;
                            break;
                        }
                        continue;
                    }
                    if (f.prop == "Type") {
                        if (!f.want.isChar()
                            || matlabType(rec->type)
                                   != f.want.toString()) {
                            pass = false;
                            break;
                        }
                        continue;
                    }
                    Value v = recordGetProp(fm, rec, f.prop, mr);
                    if (v.isUnset() || v.isEmpty()) {
                        pass = false;
                        break;
                    }
                    // Colour shorthand: MATLAB compares findobj(
                    // 'Color','r') against the RGB triplet [1 0 0].
                    if (f.want.isChar() && !v.isChar()
                        && v.numel() == 3
                        && (f.prop == "Color" || f.prop == "EdgeColor"
                            || f.prop == "FaceColor")) {
                        struct ColEntry { const char *n; double rgb[3]; };
                        static const ColEntry kCol[] = {
                            {"r", {1, 0, 0}},     {"g", {0, 0.5, 0}},
                            {"b", {0, 0, 1}},     {"k", {0, 0, 0}},
                            {"w", {1, 1, 1}},     {"y", {1, 1, 0}},
                            {"m", {1, 0, 1}},     {"c", {0, 1, 1}},
                            {"red", {1, 0, 0}},   {"green", {0, 0.5, 0}},
                            {"blue", {0, 0, 1}},  {"black", {0, 0, 0}},
                            {"white", {1, 1, 1}},
                        };
                        std::string want = f.want.toString();
                        bool matched = false;
                        for (const auto &cc : kCol) {
                            if (want != cc.n)
                                continue;
                            matched = std::abs(v.elemAsDouble(0) - cc.rgb[0]) < 1e-9
                                   && std::abs(v.elemAsDouble(1) - cc.rgb[1]) < 1e-9
                                   && std::abs(v.elemAsDouble(2) - cc.rgb[2]) < 1e-9;
                            break;
                        }
                        if (!matched) {
                            pass = false;
                            break;
                        }
                        continue;
                    }
                    if (v.isChar() != f.want.isChar()) {
                        pass = false;
                        break;
                    }
                    if (v.isChar()) {
                        if (v.toString() != f.want.toString()) {
                            pass = false;
                            break;
                        }
                    } else {
                        if (v.numel() != f.want.numel()) {
                            pass = false;
                            break;
                        }
                        bool near = true;
                        for (size_t k = 0; k < v.numel(); ++k)
                            if (std::abs(v.elemAsDouble(k)
                                         - f.want.elemAsDouble(k))
                                > 1e-9)
                                near = false;
                        if (!near) {
                            pass = false;
                            break;
                        }
                    }
                }
                if (pass)
                    hits.push_back(kv.first);   // registry key = handle id
            }
            std::sort(hits.begin(), hits.end());   // creation order
            std::string cls = hits.empty()
                ? std::string()
                : fm.findHandle(hits[0])->className;
            outs[0] = makeHandleArray(cls, hits, mr);
            (void)engine;
        };
        engine.registerFunction("", "findobj", std::move(fn));
    }
    // isgraphics(h [, type])
    {
        ExternalFunc fn = [&engine](
                Span<const Value> args, size_t,
                Span<Value> outs, CallContext &) {
            if (args.empty()) { outs[0] = Value(); return; }
            auto &fm = engine.figureManager();
            const int id = graphicsHandleIdOf(args[0]);
            auto *rec = id >= 0 ? fm.findHandle(id) : nullptr;
            bool ok = rec != nullptr;
            if (ok && args.size() >= 2
                && (args[1].isChar() || args[1].isString())) {
                const std::string want = args[1].toString();
                ok = rec->type == want || want == "chart"
                     || want == "graphics" || want == "axes"
                     || want == "figure";
            }
            outs[0] = Value::logicalScalar(ok);
        };
        engine.registerFunction("", "isgraphics", std::move(fn));
    }
    // groot — the root graphics object (figure.Parent).
    {
        ExternalFunc fn = [&engine](
                Span<const Value>, size_t,
                Span<Value> outs, CallContext &) {
            outs[0] = makeRootHandle(engine.figureManager(),
                                     engine.resource());
        };
        engine.registerFunction("", "groot", std::move(fn));
    }
    // ishandle(h) / isvalid(h)
    {
        ExternalFunc fn = [&engine](
                Span<const Value> args, size_t,
                Span<Value> outs, CallContext &) {
            if (args.empty()) { outs[0] = Value(); return; }
            auto &fm = engine.figureManager();
            const int id = graphicsHandleIdOf(args[0]);
            outs[0] = Value::logicalScalar(
                id >= 0 && fm.findHandle(id) != nullptr);
        };
        engine.registerFunction("", "ishandle", fn);
        engine.registerFunction("", "isvalid", std::move(fn));
    }
}

// ============================================================
// BuiltinClass per MATLAB class — dot-syntax + isa + eq
// ============================================================
void registerGraphicsClasses(Engine &engine)
{
    // Distinct MATLAB class names across the chart table + axes + figure
    // + text/legend/root records.
    std::unordered_set<std::string> names = {kFigureClass, kAxesClass,
                                             kTextClass, kLegendClass,
                                             kRootClass};
    for (const auto &kv : handleTypes())
        names.insert(kv.second.second);

    for (const std::string &clsName : names) {
        BuiltinClass cls;
        cls.name = clsName;
        cls.isHandle = true;
        cls.superclasses = {"matlab.mixin.Handle", "handle"};
        // propGet / propSet mirror set()/get() exactly — one shared body
        // (recordGetProp / recordSetProp), same registry, same defaults,
        // same render-effect.
        cls.propGet = [&engine](const Value &self, const std::string &name,
                                Value &out, CallContext &) -> bool {
            const int id = graphicsHandleIdOf(self);
            if (id < 0)
                return false;
            auto *rec = engine.figureManager().findHandle(id);
            if (!rec)
                throw std::runtime_error("Invalid or deleted object.");
            Value v = recordGetProp(engine.figureManager(), rec, name,
                                    engine.resource(), /*strict=*/true);
            if (v.isEmpty())
                return false;   // unknown prop → engine's MATLAB error
            out = std::move(v);
            return true;
        };
        cls.propSet = [&engine](Value &self, const std::string &name,
                                const Value &val, CallContext &) -> bool {
            const int id = graphicsHandleIdOf(self);
            if (id < 0)
                return false;
            auto *rec = engine.figureManager().findHandle(id);
            if (!rec)
                throw std::runtime_error("Invalid or deleted object.");
            recordSetProp(engine.figureManager(), rec, name, val);
            engine.figureManager().emitModified();
            return true;
        };
        // Handle identity comparison (h1 == h2 ⇔ same registry record).
        // args carries (lhs, rhs) in source order — self is a copy of the
        // dominant operand, NOT an extra argument.
        cls.ops["eq"] = [](Value &, Span<const Value> args, size_t,
                           Span<Value> outs, CallContext &) {
            bool eq = args.size() >= 2
                      && graphicsHandleIdOf(args[0]) >= 0
                      && graphicsHandleIdOf(args[0]) == graphicsHandleIdOf(args[1]);
            outs[0] = Value::logicalScalar(eq);
        };
        cls.ops["ne"] = [](Value &, Span<const Value> args, size_t,
                           Span<Value> outs, CallContext &) {
            bool eq = args.size() >= 2
                      && graphicsHandleIdOf(args[0]) >= 0
                      && graphicsHandleIdOf(args[0]) == graphicsHandleIdOf(args[1]);
            outs[0] = Value::logicalScalar(!eq);
        };
        cls.dispText = [&engine](const Value &self) -> std::string {
            const int id = graphicsHandleIdOf(self);
            auto *rec = id >= 0 ? engine.figureManager().findHandle(id) : nullptr;
            if (!rec)
                return "  deleted handle\n";
            const std::string shortName = rec->className.substr(
                rec->className.find_last_of('.') + 1);
            std::string s = "  " + shortName + " with properties:\n\n";
            auto line = [&](const char *k, const std::string &v) {
                s += std::string("    ") + k + ": " + v + "\n";
            };
            for (const char *p : {"Color", "LineStyle", "LineWidth",
                                  "Marker", "Visible"}) {
                auto it = rec->props.find(p);
                if (it != rec->props.end()) {
                    line(p, it->second);
                    continue;
                }
                Value d = rec->isChart()
                              ? chartDefaultProp(rec->type, p, engine.resource())
                              : Value();
                if (!d.isEmpty())
                    line(p, encodeProp(d));
            }
            return s;
        };
        engine.registerClass(std::move(cls));
    }
}

} // namespace

// ============================================================
// Install hub
// ============================================================
void GraphicsLibrary::install(Engine &engine)
{
    registerHandleAccessors(engine);
    registerGraphicsClasses(engine);
    registerFigureExport(engine);

    std::vector<PlotEntry> table;
    buildPlotTable(table);

    // Handle-accessor names registered above — skip their (noop)
    // table entries (registerFunctionImpl_ throws on duplicates).
    static const std::unordered_set<std::string> kHandleAccessorNames = {
        "set", "get", "isgraphics", "ishandle", "isvalid",
    };
    // Layout functions whose RESULT the adapter wraps into a handle
    // object (the core-free body only prepares the state / label field).
    static const std::unordered_set<std::string> kHandleReturning = {
        "figure", "gcf", "gca", "axes",
        "title", "subtitle", "sgtitle", "xlabel", "ylabel", "zlabel",
        "legend",
    };

    for (PlotEntry &entry : table) {
        if (kHandleAccessorNames.count(entry.name))
            continue;
        const std::string name(entry.name);
        const bool hasHandle = handleTypes().count(name) > 0;
        const bool wrapHandle = kHandleReturning.count(name) > 0;

        // The core-free plotting body; moved into the adapter closure.
        ExternalFunc ext =
            [fn = std::move(entry.fn), hasHandle, wrapHandle, name](
                Span<const Value> args, std::size_t nargout,
                Span<Value> outs, CallContext &ctx) {
                auto &fm = ctx.engine->figureManager();
                // Chart calls only: snapshot the handle base. Calling this
                // unconditionally would materialise a default figure before
                // `figure`'s own body runs — and the first figure() would
                // then number itself 2 instead of 1 (regression caught by
                // the handle smokes).
                if (hasHandle)
                    fm.beginChartCall();

                // Per-call bridge: bind the figure session + scratch arena, and
                // close the two escape hatches over the live ctx (engine + the
                // name-resolution env).
                GraphicsContext gc{
                    fm,
                    ctx.engine->resource(),
                    // callBuiltin — forward to another registered plot builtin.
                    [&ctx](std::string_view nm, Span<const Value> a,
                           std::size_t no, Span<Value> o) -> bool {
                        const ExternalFunc *cf =
                            ctx.engine->findExternal(std::string(nm), ctx.env);
                        if (!cf)
                            return false;
                        (*cf)(a, no, o, ctx);
                        return true;
                    },
                    // callHandle — evaluate a user function-handle value.
                    [&ctx](const Value &fh, Span<const Value> a) -> Value {
                        return ctx.engine->callFunctionHandle(fh, a);
                    },
                };
                fn(args, nargout, outs, gc);

                // Post-hook: bind the handle object(s) so h = plot(...)
                // works (bugs/closed/graphics/plot-family-no-return-value.md).
                // Statement calls get the handle in `ans`, like MATLAB.
                if (hasHandle) {
                    const size_t base = fm.chartCallBase();
                    const size_t after = fm.currentAxes().datasets.size();
                    if (after > base) {
                        const auto &types = handleTypes();
                        auto hit = types.find(name);
                        const int nnew = static_cast<int>(after - base);
                        if (nnew == 1) {
                            const int id = fm.createHandle(hit->second.first,
                                                           hit->second.second,
                                                           base);
                            outs[0] = makeGraphicsHandleValue(
                                hit->second.second, id, ctx.engine->resource());
                        } else {
                            std::vector<std::shared_ptr<ObjectState>> states;
                            for (int i = 0; i < nnew; ++i) {
                                const int id = fm.createHandle(
                                    hit->second.first, hit->second.second,
                                    base + static_cast<size_t>(i));
                                auto payload = std::make_shared<GraphicsHandlePayload>();
                                payload->handleId = id;
                                auto st = std::make_shared<ObjectState>(
                                    ctx.engine->resource());
                                st->native = std::move(payload);
                                states.push_back(std::move(st));
                            }
                            Dims d{1, static_cast<size_t>(nnew)};
                            outs[0] = Value::objectArray(
                                hit->second.second, d, std::move(states),
                                /*isHandle=*/true, ctx.engine->resource());
                        }
                    }
                } else if (wrapHandle) {
                    // figure/gcf → Figure, gca/axes → Axes, the title/
                    // label family → slot-bound Text, legend → Legend.
                    auto *mr = ctx.engine->resource();
                    if (name == "figure" || name == "gcf") {
                        outs[0] = makeFigureHandle(fm, mr);
                    } else if (const std::string &slot = textSlotOf(name);
                               !slot.empty()) {
                        // sgtitle is figure-scoped (kNoAxes) so scoped
                        // axes sweeps keep it; the rest bind the axes.
                        outs[0] =
                            (name == "sgtitle")
                                ? makeTextHandleFor(
                                      fm, fm.currentFigureId(),
                                      FigureManager::HandleRecord::kNoAxes,
                                      slot, mr)
                                : makeTextHandleFor(
                                      fm, fm.currentFigureId(),
                                      static_cast<size_t>(
                                          fm.current().currentAxes),
                                      slot, mr);
                    } else if (name == "legend") {
                        outs[0] = makeLegendHandleFor(
                            fm, fm.currentFigureId(),
                            static_cast<size_t>(fm.current().currentAxes),
                            mr);
                    } else {
                        outs[0] = makeAxesHandle(fm, mr);
                    }
                }
            };

        engine.registerFunction(std::string("graphics.") + entry.sub, entry.name, ext);
        if (entry.core)
            engine.registerFunction("", entry.name, ext);  // bare core name
    }
}

} // namespace numkit
