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

#include <numkit/graphics/library.hpp>
#include <numkit/graphics/graphics_context.hpp>

#include <numkit/core/engine.hpp>  // Engine, CallContext, ExternalFunc, Span, Value
#include <numkit/value/object.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace numkit {

// ── Graphics handle (bugs/closed/graphics/plot-family-no-return-value.md)
//
// NativePayload carrying the FigureManager handle-registry id.
namespace {
struct GraphicsHandlePayload : public NativePayload {
    int handleId = 0;
    std::shared_ptr<NativePayload> clone() const override
    {
        auto p = std::make_shared<GraphicsHandlePayload>();
        p->handleId = handleId;
        return p;
    }
};

Value makeHandleObject(int id, Engine &engine)
{
    auto st = std::make_shared<ObjectState>(engine.resource());
    auto payload = std::make_shared<GraphicsHandlePayload>();
    payload->handleId = id;
    st->native = payload;
    return Value::object("graphics_handle", std::move(st), /*isHandle=*/true,
                         engine.resource());
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
} // anonymous namespace

// Handle-aware set/get/isgraphics/ishandle/isvalid/delete — registered
// in place of the noop versions that layout.cpp puts in the table.
// (registerFunctionImpl_ throws on duplicates, so we skip these names
// in the table loop below and register our versions here instead.)
namespace {
int handleIdOf(const Value &v)
{
    if (!v.isObject() || v.objectClassName() != "graphics_handle")
        return -1;
    auto *st = v.objectStateConst();
    if (!st || !st->native)
        return -1;
    auto *p = dynamic_cast<const GraphicsHandlePayload *>(st->native.get());
    return p ? p->handleId : -1;
}

std::string encodeProp(const Value &v)
{
    if (v.isScalar() && !v.isChar())
        return std::to_string(v.toScalar());
    if (v.isChar() || v.isString())
        return v.toString();
    if (v.isNumeric() && v.numel() >= 2 && v.numel() <= 4) {
        std::string s = "[";
        for (size_t i = 0; i < v.numel(); ++i) {
            if (i) s += ",";
            s += std::to_string(v.elemAsDouble(i));
        }
        return s + "]";
    }
    return v.toString();
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

Value defaultProp(const std::string &name, std::pmr::memory_resource *mr)
{
    if (name == "LineWidth")  return Value::scalar(0.5, mr);
    if (name == "MarkerSize") return Value::scalar(6.0, mr);
    if (name == "Marker")     return Value::fromString("none", mr);
    if (name == "LineStyle")  return Value::fromString("-", mr);
    if (name == "Visible")    return Value::fromString("on", mr);
    if (name == "Type")       return Value::fromString("line", mr);
    if (name == "Parent")     return Value::scalar(1.0, mr);
    return Value();
}

void registerHandleAccessors(Engine &engine)
{
    // set(h, 'Prop', val [, 'Prop2', val2, …])
    {
        ExternalFunc fn = [&engine](Span<const Value> args, size_t,
                                    Span<Value> outs, CallContext &) {
            if (args.empty()) { outs[0] = Value(); return; }
            const int id = handleIdOf(args[0]);
            if (id < 0) { outs[0] = Value(); return; }
            auto *rec = engine.figureManager().findHandle(id);
            if (!rec) { outs[0] = Value(); return; }
            for (size_t i = 1; i + 1 < args.size(); i += 2) {
                if (!args[i].isChar() && !args[i].isString()) continue;
                rec->props[args[i].toString()] = encodeProp(args[i + 1]);
            }
            outs[0] = Value();
        };
        engine.registerFunction("", "set", fn);
        engine.registerFunction("compat", "set", std::move(fn));
    }
    // get(h, 'Prop')
    {
        ExternalFunc fn = [&engine](Span<const Value> args, size_t,
                                    Span<Value> outs, CallContext &) {
            if (args.empty()) { outs[0] = Value(); return; }
            const int id = handleIdOf(args[0]);
            if (id >= 0 && args.size() >= 2
                && (args[1].isChar() || args[1].isString())) {
                auto *rec = engine.figureManager().findHandle(id);
                if (rec) {
                    const std::string prop = args[1].toString();
                    auto it = rec->props.find(prop);
                    if (it != rec->props.end())
                        outs[0] = decodeProp(it->second, engine.resource());
                    else {
                        outs[0] = defaultProp(prop, engine.resource());
                        if (outs[0].isEmpty())
                            outs[0] = Value::fromString("", engine.resource());
                    }
                    return;
                }
            }
            // Not a handle (figure number, axes, …) → scalar 1 (legacy
            // noop contract: the table's get was noop_ret1).
            outs[0] = Value::scalar(1.0, engine.resource());
        };
        engine.registerFunction("", "get", fn);
        engine.registerFunction("compat", "get", std::move(fn));
    }
    // isgraphics(h [, type])
    {
        ExternalFunc fn = [&engine](Span<const Value> args, size_t,
                                    Span<Value> outs, CallContext &) {
            if (args.empty()) { outs[0] = Value(); return; }
            auto &fm = engine.figureManager();
            const int id = handleIdOf(args[0]);
            bool ok = id >= 0 && fm.findHandle(id) != nullptr;
            if (ok && args.size() >= 2
                && (args[1].isChar() || args[1].isString())) {
                auto *rec = fm.findHandle(id);
                const std::string want = args[1].toString();
                ok = rec && (rec->type == want || want == "chart"
                             || want == "graphics");
            }
            outs[0] = Value::logicalScalar(ok);
        };
        engine.registerFunction("", "isgraphics", fn);
        engine.registerFunction("compat", "isgraphics", std::move(fn));
    }
    // ishandle(h)
    {
        ExternalFunc fn = [&engine](Span<const Value> args, size_t,
                                    Span<Value> outs, CallContext &) {
            if (args.empty()) { outs[0] = Value(); return; }
            auto &fm = engine.figureManager();
            const int id = handleIdOf(args[0]);
            outs[0] = Value::logicalScalar(id >= 0
                                           && fm.findHandle(id) != nullptr);
        };
        engine.registerFunction("", "ishandle", fn);
        engine.registerFunction("compat", "ishandle", std::move(fn));
    }
    // delete(h)
    {
        ExternalFunc fn = [&engine](Span<const Value> args, size_t,
                                    Span<Value> outs, CallContext &) {
            auto &fm = engine.figureManager();
            for (auto &a : args) {
                const int id = handleIdOf(a);
                if (id >= 0)
                    fm.deleteHandle(id);
            }
            outs[0] = Value();
        };
        engine.registerFunction("", "delete", fn);
        engine.registerFunction("compat", "delete", std::move(fn));
    }
    // isvalid(h)
    {
        ExternalFunc fn = [&engine](Span<const Value> args, size_t,
                                    Span<Value> outs, CallContext &) {
            if (args.empty()) { outs[0] = Value(); return; }
            auto &fm = engine.figureManager();
            const int id = handleIdOf(args[0]);
            outs[0] = Value::logicalScalar(id >= 0
                                           && fm.findHandle(id) != nullptr);
        };
        engine.registerFunction("", "isvalid", fn);
        engine.registerFunction("compat", "isvalid", std::move(fn));
    }
}
} // namespace

void GraphicsLibrary::install(Engine &engine)
{
    registerHandleAccessors(engine);

    std::vector<PlotEntry> table;
    buildPlotTable(table);

    // The handle-accessor names registered above — skip their (noop)
    // table entries (registerFunctionImpl_ throws on duplicates).
    static const std::unordered_set<std::string> kHandleAccessorNames = {
        "set", "get", "isgraphics", "ishandle", "isvalid", "delete",
    };

    for (PlotEntry &entry : table) {
        if (kHandleAccessorNames.count(entry.name))
            continue;
        const std::string name(entry.name);
        const bool hasHandle = handleTypes().count(name) > 0;

        // The core-free plotting body; moved into the adapter closure.
        ExternalFunc ext =
            [fn = std::move(entry.fn), hasHandle, name](
                Span<const Value> args, std::size_t nargout,
                Span<Value> outs, CallContext &ctx) {
                auto &fm = ctx.engine->figureManager();
                const size_t before = fm.currentAxes().datasets.size();

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

                // Post-hook: bind the handle object(s) for plot-family
                // builtins so h = plot(...) works
                // (bugs/closed/graphics/plot-family-no-return-value.md).
                if (hasHandle) {
                    const size_t after = fm.currentAxes().datasets.size();
                    if (after > before) {
                        const auto &types = handleTypes();
                        auto hit = types.find(name);
                        const int nnew =
                            static_cast<int>(after - before);
                        if (nnew == 1) {
                            const int id = fm.createHandle(
                                hit->second.first, hit->second.second);
                            outs[0] = makeHandleObject(id, *ctx.engine);
                        } else {
                            std::vector<std::shared_ptr<ObjectState>> states;
                            for (int i = 0; i < nnew; ++i) {
                                const int id = fm.createHandle(
                                    hit->second.first, hit->second.second);
                                auto st = std::make_shared<ObjectState>(
                                    ctx.engine->resource());
                                auto pl =
                                    std::make_shared<GraphicsHandlePayload>();
                                pl->handleId = id;
                                st->native = pl;
                                states.push_back(std::move(st));
                            }
                            Dims d{1, static_cast<size_t>(nnew)};
                            outs[0] = Value::objectArray(
                                "graphics_handle", d, std::move(states),
                                /*isHandle=*/true, ctx.engine->resource());
                        }
                    }
                }
            };

        engine.registerFunction(std::string("graphics.") + entry.sub, entry.name, ext);
        engine.registerFunction("compat", entry.name, ext);
        if (entry.core)
            engine.registerFunction("", entry.name, ext);  // bare core name
    }
}

} // namespace numkit
