// numkit/graphics — plots/helpers.cpp
//
// Definitions of the pure JSON/parse helpers declared in plot_internal.hpp.
// Core-free, no figure side effects: data -> JSON string and option parsing,
// shared by the per-family plot-table builders. Extracted verbatim from the
// helper lambdas that used to live at the top of buildPlotTable().

#include "plot_internal.hpp"

#include <cctype>
#include <cmath>

namespace numkit::detail {

std::string vecToJson(const Value &v)
{
    std::ostringstream os;
    os << "[";
    if (v.isComplex()) {
        for (size_t i = 0; i < v.numel(); ++i) {
            if (i)
                os << ",";
            os << std::abs(v.complexData()[i]);
        }
    } else {
        for (size_t i = 0; i < v.numel(); ++i) {
            if (i)
                os << ",";
            double val = v.doubleData()[i];
            if (std::isnan(val))
                os << "null";
            else if (std::isinf(val))
                os << (val > 0 ? "1e308" : "-1e308");
            else
                os << val;
        }
    }
    os << "]";
    return os.str();
}

std::string makeIndexJson(std::size_t n)
{
    std::ostringstream xs;
    xs << "[";
    for (size_t i = 0; i < n; ++i) {
        if (i)
            xs << ",";
        xs << (i + 1);
    }
    xs << "]";
    return xs.str();
}

std::string argStr(const Value &v) { return v.toString(); }

void parsePlotArgs(Span<const Value> args, std::size_t startIdx, DatasetInfo &ds)
{
    for (size_t i = startIdx; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar())
            continue;
        std::string key = args[i].toString();
        for (auto &c : key)
            c = std::tolower(c);
        if (key == "linewidth")
            ds.lineWidth = args[i + 1].toScalar();
        else if (key == "markersize")
            ds.markerSize = args[i + 1].toScalar();
    }
}

std::size_t parsePlotXYStyle(Span<const Value> args, DatasetInfo &ds)
{
    size_t nvStart = 2;
    if (args.size() >= 2 && !args[1].isChar()) {
        ds.xJson = vecToJson(args[0]);
        ds.yJson = vecToJson(args[1]);
        if (args.size() >= 3 && args[2].isChar()) {
            ds.style = args[2].toString();
            nvStart = 3;
        }
    } else {
        ds.xJson = makeIndexJson(args[0].numel());
        ds.yJson = vecToJson(args[0]);
        if (args.size() >= 2 && args[1].isChar()) {
            ds.style = args[1].toString();
            nvStart = 2;
        } else {
            nvStart = 1;
        }
    }
    return nvStart;
}

void doubleToJson(std::ostringstream &os, double val)
{
    if (std::isnan(val))
        os << "null";
    else if (std::isinf(val))
        os << (val > 0 ? "1e308" : "-1e308");
    else
        os << val;
}

}  // namespace numkit::detail
