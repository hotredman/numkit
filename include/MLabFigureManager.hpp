#pragma once

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace mlab {

struct DatasetInfo
{
    std::string xJson;
    std::string yJson;
    std::string type;      // "line", "bar", "scatter", "stem", "stairs"
    std::string label;     // for legend
    std::string style;     // MATLAB style hint, e.g. "r--o", "b:", "g-."
    double lineWidth = 0;  // 0 = default
    double markerSize = 0; // 0 = default
};

struct FigureState
{
    int id = 1;
    std::vector<DatasetInfo> datasets;
    std::string title;
    std::string xlabel;
    std::string ylabel;
    std::string xlimJson; // "" or "[min,max]"
    std::string ylimJson;
    std::string rlimJson;     // polar: radial limits
    std::string thetalimJson; // polar: angular limits (degrees)
    bool grid = false;
    bool polar = false;
    bool holdOn = false;
    bool modified = false;
    std::vector<std::string> legendLabels;

    // Scale types: "linear" or "log"
    std::string xscale = "linear";
    std::string yscale = "linear";

    // Axis mode: "" (default/auto), "equal", "tight", "ij", "xy"
    std::string axisMode;

    // Polar axis config
    std::string thetaDir = "counterclockwise";
    std::string thetaZeroLocation = "right";

    // Subplot layout: 0 = not a subplot
    int subplotRows = 0;
    int subplotCols = 0;
    int subplotIndex = 0;
};

static std::string jsonEscapeFig(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"')
            out += "\\\"";
        else if (c == '\\')
            out += "\\\\";
        else if (c == '\n')
            out += "\\n";
        else
            out += c;
    }
    return out;
}

/**
 * FigureManager — tracks multiple figures with MATLAB-like semantics.
 */
class FigureManager
{
public:
    FigureState &current()
    {
        if (figures_.find(currentFigure_) == figures_.end()) {
            FigureState fs;
            fs.id = currentFigure_;
            figures_[currentFigure_] = fs;
        }
        return figures_[currentFigure_];
    }

    /** figure() — create a new figure, return its ID (smallest free ID) */
    int newFigure()
    {
        int id = 1;
        while (figures_.find(id) != figures_.end())
            id++;
        currentFigure_ = id;
        FigureState fs;
        fs.id = id;
        figures_[id] = fs;
        return id;
    }

    /** figure(n) — switch to figure n, create if needed */
    int setFigure(int n)
    {
        currentFigure_ = n;
        return n;
    }

    int currentFigureId() const { return currentFigure_; }

    /** Prepare for a new plot command: clear datasets if hold is off */
    void prepareForPlot()
    {
        auto &fig = current();
        if (!fig.holdOn) {
            fig.datasets.clear();
        }
        fig.modified = true;
    }

    /** Emit __FIGURE_DATA__ JSON to stdout for all modified figures */
    void emitModified()
    {
        for (auto &[id, fig] : figures_) {
            if (!fig.modified)
                continue;
            fig.modified = false;

            std::ostringstream os;
            os << "__FIGURE_DATA__:{\"id\":" << fig.id << ",\"datasets\":[";
            for (size_t i = 0; i < fig.datasets.size(); ++i) {
                if (i)
                    os << ",";
                auto &ds = fig.datasets[i];
                os << "{\"x\":" << ds.xJson << ",\"y\":" << ds.yJson << ",\"type\":\"" << ds.type
                   << "\"";
                if (!ds.label.empty())
                    os << ",\"label\":\"" << jsonEscapeFig(ds.label) << "\"";
                if (!ds.style.empty())
                    os << ",\"style\":\"" << ds.style << "\"";
                if (ds.lineWidth > 0)
                    os << ",\"lineWidth\":" << ds.lineWidth;
                if (ds.markerSize > 0)
                    os << ",\"markerSize\":" << ds.markerSize;
                os << "}";
            }
            os << "],\"config\":{";
            os << "\"title\":\"" << jsonEscapeFig(fig.title) << "\"";
            os << ",\"xlabel\":\"" << jsonEscapeFig(fig.xlabel) << "\"";
            os << ",\"ylabel\":\"" << jsonEscapeFig(fig.ylabel) << "\"";
            if (!fig.xlimJson.empty())
                os << ",\"xlim\":" << fig.xlimJson;
            if (!fig.ylimJson.empty())
                os << ",\"ylim\":" << fig.ylimJson;
            if (!fig.rlimJson.empty())
                os << ",\"rlim\":" << fig.rlimJson;
            if (!fig.thetalimJson.empty())
                os << ",\"thetalim\":" << fig.thetalimJson;
            os << ",\"grid\":" << (fig.grid ? "true" : "false");
            os << ",\"polar\":" << (fig.polar ? "true" : "false");
            os << ",\"xscale\":\"" << fig.xscale << "\"";
            os << ",\"yscale\":\"" << fig.yscale << "\"";
            if (!fig.axisMode.empty())
                os << ",\"axisMode\":\"" << fig.axisMode << "\"";
            if (fig.polar) {
                os << ",\"thetaDir\":\"" << fig.thetaDir << "\"";
                os << ",\"thetaZeroLocation\":\"" << fig.thetaZeroLocation << "\"";
            }
            if (fig.subplotRows > 0) {
                os << ",\"subplot\":[" << fig.subplotRows << "," << fig.subplotCols << ","
                   << fig.subplotIndex << "]";
            }
            if (!fig.legendLabels.empty()) {
                os << ",\"legend\":[";
                for (size_t i = 0; i < fig.legendLabels.size(); ++i) {
                    if (i)
                        os << ",";
                    os << "\"" << jsonEscapeFig(fig.legendLabels[i]) << "\"";
                }
                os << "]";
            }
            os << "}}";
            std::cout << os.str() << "\n";
        }
    }

    /** Remove a single figure */
    void closeFigure(int id)
    {
        figures_.erase(id);
        if (currentFigure_ == id) {
            if (!figures_.empty())
                currentFigure_ = figures_.rbegin()->first;
            else
                currentFigure_ = 1;
        }
    }

    /** Close current figure */
    void closeCurrent() { closeFigure(currentFigure_); }

    /** Close all figures and reset state */
    void closeAll()
    {
        figures_.clear();
        currentFigure_ = 1;
    }

    /** Get the figures map (for inspection) */
    const std::map<int, FigureState> &figures() const { return figures_; }

private:
    std::map<int, FigureState> figures_;
    int currentFigure_ = 1;
};

} // namespace mlab