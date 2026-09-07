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

namespace {

constexpr const char *kFigureClass = "matlab.ui.Figure";
constexpr const char *kAxesClass   = "matlab.graphics.axis.Axes";

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
    else touched = false;
    if (touched)
        fm.markFigureModified(rec->figureId);
    return touched;
}

Value axesGetProp(FigureManager &fm, FigureManager::HandleRecord *rec,
                  const std::string &name, std::pmr::memory_resource *mr)
{
    auto str = [&](const char *s) { return Value::fromString(s, mr); };
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

} // anonymous namespace

// ============================================================
// Handle-aware accessor registration (set/get/isgraphics/ishandle/
// isvalid — delete lives in general_reg.cpp, type-dispatched)
// ============================================================
namespace {

void registerHandleAccessors(Engine &engine)
{
    // set(h, 'Prop', val [, 'Prop2', val2, …]) — chart/axes/figure
    // records; dot-syntax h.Prop = val routes through the SAME body via
    // the BuiltinClass propSet hook.
    auto setProp = [&engine](FigureManager::HandleRecord *rec,
                             const std::string &name, const Value &val) {
        rec->props[name] = encodeProp(val);
        if (rec->type == "axes")
            applyAxesProp(engine.figureManager(), rec, name, val);
        else
            applyChartProp(engine.figureManager(), rec, name, val);
    };

    // get(h, 'Prop')
    auto getProp = [&engine](FigureManager::HandleRecord *rec,
                             const std::string &name) -> Value {
        auto &fm = engine.figureManager();
        auto *mr = engine.resource();
        auto it = rec->props.find(name);
        if (it != rec->props.end())
            return decodeProp(it->second, mr);
        if (rec->type == "figure") {
            if (name == "Type")   return Value::fromString("figure", mr);
            if (name == "Number") return Value::scalar(rec->figureId, mr);
            if (name == "CurrentAxes") return makeAxesHandle(fm, mr);
            if (name == "Children") {
                auto fit = fm.figures().find(rec->figureId);
                size_t n = fit != fm.figures().end() ? fit->second.axes.size() : 0;
                return n ? makeAxesHandle(fm, mr)   // ≥1 axes → current (gap: full array)
                         : Value::matrix(0, 0, ValueType::DOUBLE, mr);
            }
            return Value::fromString("", mr);
        }
        if (rec->type == "axes") {
            if (name == "Parent") {
                // Axes.Parent → the containing Figure (MATLAB: Root is
                // above that; root handles are a deferred gap).
                return makeFigureHandleFor(fm, rec->figureId, mr);
            }
            Value v = axesGetProp(fm, rec, name, mr);
            if (!v.isEmpty())
                return v;
            return Value::fromString("", mr);
        }
        if (name == "Parent")
            return makeAxesHandle(fm, mr);
        {
            Value v = chartDataProp(fm, rec, name, mr);
            if (!v.isEmpty())
                return v;
        }
        Value d = chartDefaultProp(rec->type, name, mr);
        return d.isEmpty() ? Value::fromString("", mr) : d;
    };

    // set(h, …)
    {
        ExternalFunc fn = [&engine, setProp](
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
                setProp(rec, args[i].toString(), args[i + 1]);
            }
            engine.figureManager().emitModified();
            outs[0] = Value();
        };
        engine.registerFunction("", "set", fn);
        engine.registerFunction("compat", "set", std::move(fn));
    }
    // get(h [, 'Prop']) — without a property name: struct of all
    {
        ExternalFunc fn = [&engine, getProp](
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
                outs[0] = getProp(rec, args[1].toString());
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
            }
            for (const auto &[k, v] : fields)
                st.setFieldAll(k, decodeProp(v, mr));
            outs[0] = std::move(st);
        };
        engine.registerFunction("", "get", fn);
        engine.registerFunction("compat", "get", std::move(fn));
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
        engine.registerFunction("", "isgraphics", fn);
        engine.registerFunction("compat", "isgraphics", std::move(fn));
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
        engine.registerFunction("compat", "ishandle", fn);
        engine.registerFunction("", "isvalid", fn);
        engine.registerFunction("compat", "isvalid", std::move(fn));
    }
}

// ============================================================
// BuiltinClass per MATLAB class — dot-syntax + isa + eq
// ============================================================
void registerGraphicsClasses(Engine &engine)
{
    // Distinct MATLAB class names across the chart table + axes + figure.
    std::unordered_set<std::string> names = {kFigureClass, kAxesClass};
    for (const auto &kv : handleTypes())
        names.insert(kv.second.second);

    for (const std::string &clsName : names) {
        BuiltinClass cls;
        cls.name = clsName;
        cls.isHandle = true;
        cls.superclasses = {"matlab.mixin.Handle", "handle"};
        // propGet / propSet mirror set()/get() exactly (same registry,
        // same defaults, same render-effect).
        cls.propGet = [&engine](const Value &self, const std::string &name,
                                Value &out, CallContext &) -> bool {
            const int id = graphicsHandleIdOf(self);
            if (id < 0)
                return false;
            auto *rec = engine.figureManager().findHandle(id);
            if (!rec)
                throw std::runtime_error("Invalid or deleted object.");
            auto it = rec->props.find(name);
            if (it != rec->props.end()) {
                out = decodeProp(it->second, engine.resource());
                return true;
            }
            if (rec->type == "figure") {
                if (name == "Type")   { out = Value::fromString("figure", engine.resource()); return true; }
                if (name == "Number") { out = Value::scalar(rec->figureId, engine.resource()); return true; }
                if (name == "CurrentAxes") { out = makeAxesHandle(engine.figureManager(), engine.resource()); return true; }
                return false;
            }
            if (rec->type == "axes") {
                if (name == "Parent") {
                    out = makeFigureHandleFor(engine.figureManager(),
                                              rec->figureId, engine.resource());
                    return true;
                }
                Value v = axesGetProp(engine.figureManager(), rec, name,
                                      engine.resource());
                if (v.isEmpty())
                    return false;
                out = std::move(v);
                return true;
            }
            if (name == "Parent") {
                out = makeAxesHandle(engine.figureManager(), engine.resource());
                return true;
            }
            {
                Value v = chartDataProp(engine.figureManager(), rec, name,
                                        engine.resource());
                if (!v.isEmpty()) {
                    out = std::move(v);
                    return true;
                }
            }
            Value d = chartDefaultProp(rec->type, name, engine.resource());
            if (d.isEmpty())
                return false;
            out = std::move(d);
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
            rec->props[name] = encodeProp(val);
            if (rec->type == "axes")
                applyAxesProp(engine.figureManager(), rec, name, val);
            else
                applyChartProp(engine.figureManager(), rec, name, val);
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

    std::vector<PlotEntry> table;
    buildPlotTable(table);

    // Handle-accessor names registered above — skip their (noop)
    // table entries (registerFunctionImpl_ throws on duplicates).
    static const std::unordered_set<std::string> kHandleAccessorNames = {
        "set", "get", "isgraphics", "ishandle", "isvalid",
    };
    // Layout functions whose RESULT the adapter wraps into a handle
    // object (the core-free body only prepares the state / numeric id).
    static const std::unordered_set<std::string> kHandleReturning = {
        "figure", "gcf", "gca", "axes",
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
                    // figure/gcf → matlab.ui.Figure, gca/axes → Axes.
                    auto *mr = ctx.engine->resource();
                    if (name == "figure" || name == "gcf")
                        outs[0] = makeFigureHandle(fm, mr);
                    else
                        outs[0] = makeAxesHandle(fm, mr);
                }
            };

        engine.registerFunction(std::string("graphics.") + entry.sub, entry.name, ext);
        engine.registerFunction("compat", entry.name, ext);
        if (entry.core)
            engine.registerFunction("", entry.name, ext);  // bare core name
    }
}

} // namespace numkit
