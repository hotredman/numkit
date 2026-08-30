/// @file graphics_context.hpp
/// @ingroup group_graphics
#pragma once
// numkit/graphics/graphics_context.hpp
//
// The core-free execution context handed to every graphics plotting body.
//
// Graphics is a plotting *service* (figure / plot / surf / imshow / …): each
// function's job is a side effect — build a wire-JSON dataset and push it onto
// the active figure's axes via the FigureManager, then emit a "modified"
// notification. None of that needs the Engine; it needs (1) the figure session
// state, (2) a scratch allocator, and (3) two escape hatches that DO reach back
// into the runtime: forwarding to another registered plot builtin by name
// (surfc → surf, fcontour → contour, geoplot → plot, …) and evaluating a user
// function-handle (fplot/fsurf sampling f(x)).
//
// `GraphicsContext` bundles exactly those four things. The two runtime escape
// hatches are std::function callbacks bound by the bundle-side adapter (see
// library.cpp) to the live Engine — so the plotting bodies (plots.cpp) stay
// core-free and the only Engine-coupled glue is the one generic adapter.
//
// This mirrors the FsContext decoupling: stateful/runtime surfaces are pulled
// behind a context object so the L2 compute layer never includes <numkit/core>.

#include <numkit/figure/figure_manager.hpp>  // FigureManager, DatasetInfo, AxesState
#include <numkit/value/value.hpp>            // Value, ValueType
#include <numkit/value/span.hpp>             // Span

#include <cstddef>
#include <functional>
#include <memory_resource>
#include <string_view>
#include <vector>

namespace numkit {

/// @addtogroup group_graphics
/// @{


// Context passed to every graphics plotting body. Core-free: it knows the
// figure session (fm), a scratch arena (mr), and how to re-enter the runtime
// for the two operations that genuinely require it (callBuiltin / callHandle).
struct GraphicsContext {
    FigureManager &fm;                  // active figure / axes session state
    std::pmr::memory_resource *mr;      // scratch allocator for Value building

    // Forward to another *registered* plot builtin by name (e.g. surfc → surf).
    // Resolves the builtin at call time, invokes it with (args, nargout, outs),
    // and returns true; returns false if no such builtin is registered (callers
    // treat that as a no-op and return an empty value). Bound by the adapter to
    // Engine::findExternal + invoke.
    std::function<bool(std::string_view name,
                       Span<const Value> args,
                       std::size_t nargout,
                       Span<Value> outs)>
        callBuiltin;

    // Evaluate a user function-handle Value (the @(x)… callbacks fed to
    // fplot / fcontour / fsurf / fplot3). Bound by the adapter to
    // Engine::callFunctionHandle; works on both the TreeWalker and VM backends.
    std::function<Value(const Value &handle, Span<const Value> args)> callHandle;
};

// A graphics plotting body. Same shape as ExternalFunc but takes the core-free
// GraphicsContext instead of the Engine-coupled CallContext.
using GraphicsFn = std::function<void(Span<const Value> args,
                                      std::size_t nargout,
                                      Span<Value> outs,
                                      GraphicsContext &gc)>;

// One registration record. `sub` is the graphics sub-namespace (layout / line /
// bar / surface / polar / contour / image); `core` requests the extra bare-name
// registration into core (figure / close / hold etc., namespace_design.md §7).
struct PlotEntry {
    const char *sub;
    const char *name;
    bool core;
    GraphicsFn fn;
};

// Builds the full table of graphics plotting bodies (core-free; defined in
// plots.cpp). The bundle-side installer (library.cpp GraphicsLibrary::install)
// wraps each entry's GraphicsFn in a CallContext→GraphicsContext adapter and
// registers it under graphics.<sub>.<name> + compat.<name> (+ core when set).
void buildPlotTable(std::vector<PlotEntry> &table);


/// @}
} // namespace numkit
