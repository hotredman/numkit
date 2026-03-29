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
    std::string type;  // "line", "bar", "scatter"
    std::string label; // for legend
    std::string style; // MATLAB style hint, e.g. "r--", "b:", "g-."
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
    bool grid = false;
    bool polar = false;
    bool holdOn = false;
    bool modified = false;
    std::vector<std::string> legendLabels;
};

/**
 * FigureManager — tracks multiple figures with MATLAB-like semantics.
 *
 * - figure()    creates a new figure
 * - figure(n)   switches to figure n (creates if needed)
 * - plot/bar/scatter/hist add datasets to the current figure
 * - hold on/off controls whether new plots replace or append
 * - title/xlabel/ylabel/xlim/ylim/grid/legend modify current figure
 * - emitModified() outputs __FIGURE_DATA__ JSON for all changed figures
 *
 * Owned by Engine — destroyed and recreated on reset.
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
                    os << ",\"label\":\"" << ds.label << "\"";
                if (!ds.style.empty())
                    os << ",\"style\":\"" << ds.style << "\"";
                os << "}";
            }
            os << "],\"config\":{";
            os << "\"title\":\"" << fig.title << "\"";
            os << ",\"xlabel\":\"" << fig.xlabel << "\"";
            os << ",\"ylabel\":\"" << fig.ylabel << "\"";
            if (!fig.xlimJson.empty())
                os << ",\"xlim\":" << fig.xlimJson;
            if (!fig.ylimJson.empty())
                os << ",\"ylim\":" << fig.ylimJson;
            os << ",\"grid\":" << (fig.grid ? "true" : "false");
            os << ",\"polar\":" << (fig.polar ? "true" : "false");
            if (!fig.legendLabels.empty()) {
                os << ",\"legend\":[";
                for (size_t i = 0; i < fig.legendLabels.size(); ++i) {
                    if (i)
                        os << ",";
                    os << "\"" << fig.legendLabels[i] << "\"";
                }
                os << "]";
            }
            os << "}}";
            std::cout << os.str() << "\n";
        }
    }

    /** Remove a single figure */
    void closeFigure(int id) { figures_.erase(id); }

    /** Close current figure */
    void closeCurrent() { figures_.erase(currentFigure_); }

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