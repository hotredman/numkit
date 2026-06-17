#pragma once
// numkit/graphics — plots/plot_internal.hpp
//
// Private (src-only) header shared by the per-family plot-table builders carved
// out of plots.cpp. NOT installed — only the graphics .cpp TUs include it.
//
// It declares (1) the pure JSON/parse helpers (numkit::detail) that many plot
// bodies share, and (2) the per-family build*Plots(table) entry points that
// buildPlotTable() dispatches to.

#include <numkit/graphics/graphics_context.hpp>  // GraphicsContext, PlotEntry, GraphicsFn
#include <numkit/figure/figure_manager.hpp>      // DatasetInfo
#include <numkit/value/span.hpp>                 // Span
#include <numkit/value/value.hpp>                // Value

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace numkit {

struct PlotEntry;  // defined in numkit/graphics/graphics_context.hpp

namespace detail {

// Render a numeric/complex Value as a JSON number array ("[1,2,3]"). Complex
// uses magnitude; NaN -> null; +/-Inf -> +/-1e308.
std::string vecToJson(const Value &v);

// "[1,2,...,n]" — the 1-based implicit-X axis for a plot given only Y.
std::string makeIndexJson(std::size_t n);

// A char Value as its raw string (title / label text).
std::string argStr(const Value &v);

// Parse trailing 'LineWidth' / 'MarkerSize' N-V pairs from args[startIdx..].
void parsePlotArgs(Span<const Value> args, std::size_t startIdx, DatasetInfo &ds);

// Parse the leading (x,y[,style]) / (y[,style]) of a plot call into ds and
// return the index where trailing N-V pairs begin.
std::size_t parsePlotXYStyle(Span<const Value> args, DatasetInfo &ds);

// Append a double to os as JSON (NaN -> null; +/-Inf -> +/-1e308).
void doubleToJson(std::ostringstream &os, double val);


// Plot bodies shared by >1 family (defined in plots/shared.cpp).
void heatmapImpl(const char *typeName, bool axisIj, Span<const Value> args,
                 std::size_t nargout, Span<Value> outs, GraphicsContext &gc);
void geoForward(const char *target, Span<const Value> args, Span<Value> outs,
                GraphicsContext &gc);
void delegateTo(const char *target, Span<const Value> args, Span<Value> outs,
                GraphicsContext &gc);

}  // namespace detail

// Per-family plot-table builders (carved out of plots.cpp). Each appends its
// graphics.<sub>.<name> entries to `table`; buildPlotTable() calls them in turn.
void buildContourPlots(std::vector<PlotEntry> &table);
void buildPolarPlots(std::vector<PlotEntry> &table);
void buildSurfacePlots(std::vector<PlotEntry> &table);
void buildLayoutPlots(std::vector<PlotEntry> &table);
// BUILDER-DECLS — the family split appends new declarations directly above here.

}  // namespace numkit
