#pragma once

#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace numkit {

struct DatasetInfo
{
    std::string xJson;
    std::string yJson;
    std::string zJson;     // 2D matrix for imagesc, e.g. [[1,2],[3,4]]
    std::string type;      // "line", "bar", "scatter", "stem", "stairs", "imagesc"
    std::string label;     // for legend
    std::string style;     // MATLAB style hint, e.g. "r--o", "b:", "g-."
    double lineWidth = 0;  // 0 = default
    double markerSize = 0; // 0 = default

    // ── Imagesc storage (uint8 quantized) ──────────────────────────────
    // Display path is fundamentally indexed: each pixel is a colormap
    // index 0..255. Storing the full-precision float is overkill — for a
    // 16k×42k spectrogram the float32 backing store would be 2.7 GB and
    // wouldn't fit in the WASM heap. Quantizing once at imagesc-time
    // brings storage down to ~890 MB (with the LOD pyramid) for the same
    // shape and enables fast LUT-based rendering.
    //
    // Quantization: idx = clamp(round((v - cminOrig) / range * 254), 0, 254)
    //               idx 255 = NaN / Inf sentinel
    //
    // Layout: column-major (MATLAB convention), idx = c * rows + r.
    //
    // colorScaleBaked is true when log10 was applied before quantization
    // — toggling color scale post-imagesc requires re-emitting the figure.
    // Hover values reconstruct via cminOrig + (idx/254) * range; the
    // 256-level resolution gives ≤0.4% step on a typical [cmin..cmax].
    std::vector<uint8_t> zQuantized;
    double cminOrig = 0.0;
    double cmaxOrig = 1.0;
    bool   colorScaleBaked = false;   // log10 applied before quantization?

    // Set by imagesc when rows*cols > 2M and the inline JSON preview was
    // mean-pooled. originalRows/Cols always describe zQuantized's shape
    // regardless of whether the inline preview was downsampled.
    bool   downsampled  = false;
    size_t originalRows = 0;
    size_t originalCols = 0;
};

/** Per-axes state — one subplot panel has one AxesState */
struct AxesState
{
    std::vector<DatasetInfo> datasets;
    std::string title;
    std::string xlabel;
    std::string ylabel;
    std::string xlimJson;
    std::string ylimJson;
    std::string rlimJson;
    std::string thetalimJson;
    std::string climJson;     // color limits for imagesc, e.g. "[0,1]"
    std::string colormapName; // "parula","jet","hot","cool","gray","viridis","turbo","hsv"
    bool polar = false;
    bool holdOn = false;
    std::string gridMode; // "" = off, "on" = major, "minor" = major+minor
    std::vector<std::string> legendLabels;

    std::string xscale = "linear";
    std::string yscale = "linear";
    std::string axisMode;

    std::string thetaDir = "counterclockwise";
    std::string thetaZeroLocation = "right";

    // Position in subplot grid (1-based), 0 = not a subplot
    int subplotIndex = 0;
};

struct FigureState
{
    int id = 1;
    bool modified = false;

    // Subplot grid: 0 = no subplots (single axes)
    int subplotRows = 0;
    int subplotCols = 0;

    // All axes in this figure; for non-subplot figures, size == 1
    std::vector<AxesState> axes;
    int currentAxes = 0; // index into axes[]

    /** Get the current axes, creating if needed */
    AxesState &cur()
    {
        if (axes.empty())
            axes.push_back(AxesState{});
        return axes[currentAxes];
    }
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

class FigureManager
{
public:
    using OutputFunc = std::function<void(const std::string &)>;

    /** Set the output callback for figure/close markers.
     *  When set, emitModified() and close notifications route through this
     *  instead of std::cout, so debug and normal paths share the same channel. */
    void setOutputFunc(OutputFunc f) { outputFunc_ = std::move(f); }

    FigureState &current()
    {
        if (figures_.find(currentFigure_) == figures_.end()) {
            FigureState fs;
            fs.id = currentFigure_;
            fs.axes.push_back(AxesState{});
            figures_[currentFigure_] = fs;
        }
        return figures_[currentFigure_];
    }

    /** Convenience: current axes of current figure */
    AxesState &currentAxes() { return current().cur(); }

    int newFigure()
    {
        int id = 1;
        while (figures_.find(id) != figures_.end())
            id++;
        currentFigure_ = id;
        FigureState fs;
        fs.id = id;
        fs.axes.push_back(AxesState{});
        figures_[id] = fs;
        return id;
    }

    int setFigure(int n)
    {
        currentFigure_ = n;
        if (figures_.find(n) == figures_.end()) {
            FigureState fs;
            fs.id = n;
            fs.axes.push_back(AxesState{});
            figures_[n] = fs;
        }
        return n;
    }

    int currentFigureId() const { return currentFigure_; }

    /** subplot(m,n,p) — set grid and switch to axes at position p */
    void setSubplot(int m, int n, int p)
    {
        auto &fig = current();
        fig.subplotRows = m;
        fig.subplotCols = n;

        // Find or create axes for this subplot position
        for (int i = 0; i < static_cast<int>(fig.axes.size()); ++i) {
            if (fig.axes[i].subplotIndex == p) {
                fig.currentAxes = i;
                return;
            }
        }
        // Create new axes for position p
        AxesState ax;
        ax.subplotIndex = p;
        fig.axes.push_back(ax);
        fig.currentAxes = static_cast<int>(fig.axes.size()) - 1;
    }

    void prepareForPlot()
    {
        auto &ax = currentAxes();
        if (!ax.holdOn) {
            int savedSubplot = ax.subplotIndex;
            ax = AxesState{};
            ax.subplotIndex = savedSubplot;
        }
        current().modified = true;
    }

    /** Emit JSON for all modified figures */
    void emitModified()
    {
        for (auto &[id, fig] : figures_) {
            if (!fig.modified)
                continue;
            fig.modified = false;

            std::ostringstream os;
            os << "__FIGURE_DATA__:{\"id\":" << fig.id;

            // Subplot grid info
            if (fig.subplotRows > 0) {
                os << ",\"subplotGrid\":[" << fig.subplotRows << "," << fig.subplotCols << "]";
            }

            os << ",\"axes\":[";
            for (size_t ai = 0; ai < fig.axes.size(); ++ai) {
                if (ai)
                    os << ",";
                auto &ax = fig.axes[ai];
                os << "{";
                if (ax.subplotIndex > 0)
                    os << "\"subplotIndex\":" << ax.subplotIndex << ",";
                os << "\"datasets\":[";
                for (size_t i = 0; i < ax.datasets.size(); ++i) {
                    if (i)
                        os << ",";
                    auto &ds = ax.datasets[i];
                    os << "{\"x\":" << ds.xJson << ",\"y\":" << ds.yJson << ",\"type\":\""
                       << ds.type << "\"";
                    if (!ds.label.empty())
                        os << ",\"label\":\"" << jsonEscapeFig(ds.label) << "\"";
                    if (!ds.style.empty())
                        os << ",\"style\":\"" << ds.style << "\"";
                    if (ds.lineWidth > 0)
                        os << ",\"lineWidth\":" << ds.lineWidth;
                    if (ds.markerSize > 0)
                        os << ",\"markerSize\":" << ds.markerSize;
                    if (!ds.zJson.empty())
                        os << ",\"z\":" << ds.zJson;
                    if (!ds.zQuantized.empty()) {
                        os << ",\"cminOrig\":" << ds.cminOrig
                           << ",\"cmaxOrig\":" << ds.cmaxOrig;
                        if (ds.colorScaleBaked)
                            os << ",\"colorScaleBaked\":\"log\"";
                        os << ",\"originalRows\":" << ds.originalRows
                           << ",\"originalCols\":" << ds.originalCols;
                        if (ds.downsampled)
                            os << ",\"downsampled\":true";
                    }
                    os << "}";
                }
                os << "],\"config\":{";
                os << "\"title\":\"" << jsonEscapeFig(ax.title) << "\"";
                os << ",\"xlabel\":\"" << jsonEscapeFig(ax.xlabel) << "\"";
                os << ",\"ylabel\":\"" << jsonEscapeFig(ax.ylabel) << "\"";
                if (!ax.xlimJson.empty())
                    os << ",\"xlim\":" << ax.xlimJson;
                if (!ax.ylimJson.empty())
                    os << ",\"ylim\":" << ax.ylimJson;
                if (!ax.rlimJson.empty())
                    os << ",\"rlim\":" << ax.rlimJson;
                if (!ax.thetalimJson.empty())
                    os << ",\"thetalim\":" << ax.thetalimJson;
                if (!ax.climJson.empty())
                    os << ",\"clim\":" << ax.climJson;
                if (!ax.colormapName.empty())
                    os << ",\"colormap\":\"" << ax.colormapName << "\"";
                os << ",\"grid\":\"" << ax.gridMode << "\"";
                os << ",\"polar\":" << (ax.polar ? "true" : "false");
                os << ",\"xscale\":\"" << ax.xscale << "\"";
                os << ",\"yscale\":\"" << ax.yscale << "\"";
                if (!ax.axisMode.empty())
                    os << ",\"axisMode\":\"" << ax.axisMode << "\"";
                if (ax.polar) {
                    os << ",\"thetaDir\":\"" << ax.thetaDir << "\"";
                    os << ",\"thetaZeroLocation\":\"" << ax.thetaZeroLocation << "\"";
                }
                if (!ax.legendLabels.empty()) {
                    os << ",\"legend\":[";
                    for (size_t i = 0; i < ax.legendLabels.size(); ++i) {
                        if (i)
                            os << ",";
                        os << "\"" << jsonEscapeFig(ax.legendLabels[i]) << "\"";
                    }
                    os << "]";
                }
                os << "}}";
            }
            os << "]}";
            std::string line = os.str() + "\n";
            if (outputFunc_)
                outputFunc_(line);
            else
                std::cout << line;
        }
    }

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

    /** Close and emit notification marker */
    void closeFigureNotify(int id)
    {
        closeFigure(id);
        std::string marker = "__FIGURE_CLOSE__:" + std::to_string(id) + "\n";
        if (outputFunc_) outputFunc_(marker); else std::cout << marker;
    }

    void closeCurrent() { closeFigure(currentFigure_); }

    void closeCurrentNotify()
    {
        int id = currentFigure_;
        closeCurrent();
        std::string marker = "__FIGURE_CLOSE__:" + std::to_string(id) + "\n";
        if (outputFunc_) outputFunc_(marker); else std::cout << marker;
    }

    void closeAll()
    {
        figures_.clear();
        currentFigure_ = 1;
    }

    void closeAllNotify()
    {
        closeAll();
        std::string marker = "__FIGURE_CLOSE_ALL__\n";
        if (outputFunc_) outputFunc_(marker); else std::cout << marker;
    }

    const std::map<int, FigureState> &figures() const { return figures_; }

    /**
     * Tile fetcher for huge imagesc datasets. Reads a sub-rectangle
     *   rows [r0, r0+h) × cols [c0, c0+w)
     * from the dataset's zQuantized (uint8 indices), mean-pools by `lod×lod`,
     * writes row-major uint8 indices to `out`. Index 255 = NaN sentinel.
     *
     * Returns true with output dims via outRows/outCols, or false if the
     * request is out of range / dataset isn't an imagesc / no zQuantized.
     * Caller allocates `out` lazily — function .resize()s it.
     */
    bool getFigureTile(int figId, int axIdx, int dsIdx,
                       int r0, int c0, int h, int w, int lod,
                       std::vector<uint8_t> &out,
                       size_t &outRows, size_t &outCols) const
    {
        outRows = 0;
        outCols = 0;
        out.clear();

        auto figIt = figures_.find(figId);
        if (figIt == figures_.end()) return false;
        const auto &fig = figIt->second;
        if (axIdx < 0 || axIdx >= static_cast<int>(fig.axes.size())) return false;
        const auto &axState = fig.axes[axIdx];
        if (dsIdx < 0 || dsIdx >= static_cast<int>(axState.datasets.size())) return false;
        const auto &ds = axState.datasets[dsIdx];
        if (ds.zQuantized.empty() || ds.type != "imagesc") return false;

        const size_t fullRows = ds.originalRows;
        const size_t fullCols = ds.originalCols;
        if (fullRows == 0 || fullCols == 0) return false;
        if (ds.zQuantized.size() < fullRows * fullCols) return false;

        if (lod < 1) lod = 1;
        if (r0 < 0) r0 = 0;
        if (c0 < 0) c0 = 0;
        if (h < 0 || w < 0) return false;

        const size_t rEnd = std::min(fullRows, static_cast<size_t>(r0) + static_cast<size_t>(h));
        const size_t cEnd = std::min(fullCols, static_cast<size_t>(c0) + static_cast<size_t>(w));
        if (static_cast<size_t>(r0) >= fullRows || static_cast<size_t>(c0) >= fullCols
            || rEnd <= static_cast<size_t>(r0) || cEnd <= static_cast<size_t>(c0)) {
            return false;
        }

        const size_t srcH = rEnd - static_cast<size_t>(r0);
        const size_t srcW = cEnd - static_cast<size_t>(c0);
        const size_t L = static_cast<size_t>(lod);
        const size_t oH = (srcH + L - 1) / L;
        const size_t oW = (srcW + L - 1) / L;

        out.resize(oH * oW);

        // Mean-pool indices over L×L blocks. Index 255 = NaN, skipped.
        // Row-major output (suits canvas blit). zQuantized is column-major.
        for (size_t orow = 0; orow < oH; ++orow) {
            const size_t rA = static_cast<size_t>(r0) + orow * L;
            const size_t rB = std::min(rEnd, rA + L);
            for (size_t ocol = 0; ocol < oW; ++ocol) {
                const size_t cA = static_cast<size_t>(c0) + ocol * L;
                const size_t cB = std::min(cEnd, cA + L);
                int sum = 0;
                int n = 0;
                for (size_t cc = cA; cc < cB; ++cc) {
                    for (size_t rr = rA; rr < rB; ++rr) {
                        const uint8_t q = ds.zQuantized[cc * fullRows + rr];
                        if (q != 255) {
                            sum += q;
                            ++n;
                        }
                    }
                }
                out[orow * oW + ocol] = (n > 0)
                    ? static_cast<uint8_t>((sum + n / 2) / n)
                    : uint8_t{255};
            }
        }

        outRows = oH;
        outCols = oW;
        return true;
    }

private:
    std::map<int, FigureState> figures_;
    int currentFigure_ = 1;
    OutputFunc outputFunc_;
};

} // namespace numkit