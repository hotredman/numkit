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

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace numkit {

void GraphicsLibrary::install(Engine &engine)
{
    std::vector<PlotEntry> table;
    buildPlotTable(table);

    for (PlotEntry &entry : table) {
        // The core-free plotting body; moved into the adapter closure.
        ExternalFunc ext =
            [fn = std::move(entry.fn)](Span<const Value> args, std::size_t nargout,
                                       Span<Value> outs, CallContext &ctx) {
                // Per-call bridge: bind the figure session + scratch arena, and
                // close the two escape hatches over the live ctx (engine + the
                // name-resolution env).
                GraphicsContext gc{
                    ctx.engine->figureManager(),
                    ctx.engine->resource(),
                    // callBuiltin — forward to another registered plot builtin.
                    [&ctx](std::string_view name, Span<const Value> a,
                           std::size_t no, Span<Value> o) -> bool {
                        const ExternalFunc *cf =
                            ctx.engine->findExternal(std::string(name), ctx.env);
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
            };

        engine.registerFunction(std::string("graphics.") + entry.sub, entry.name, ext);
        engine.registerFunction("compat", entry.name, ext);
        if (entry.core)
            engine.registerFunction("", entry.name, ext);  // bare core name
    }
}

} // namespace numkit
