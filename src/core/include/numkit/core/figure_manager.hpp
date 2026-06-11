#pragma once

#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>

#include <numkit/core/decimate.hpp>

namespace numkit {

// ── Flat JSON number-array helpers (line-series downsampling) ────────────
namespace figdetail {

// Count top-level elements in a flat JSON number array "[a,b,c]" cheaply
// (commas + 1) — gates the expensive parse on series size.
inline std::size_t countFlatElements(const std::string &j) {
    std::size_t commas = 0;
    bool any = false;
    for (char c : j) {
        if (c == ',') commas++;
        else if (c != '[' && c != ']' && c != ' ' && c != '\t' && c != '\n') any = true;
    }
    return any ? commas + 1 : 0;
}

// Parse a flat JSON number array → doubles. "null" → NaN; bare/quoted
// Inf/NaN handled by strtod. The figure xJson is engine-built (well-formed).
inline std::vector<double> parseFlatDoubles(const std::string &j) {
    std::vector<double> out;
    const char *p = j.c_str();
    const char *end = p + j.size();
    while (p < end && *p != '[') ++p;
    if (p < end) ++p;  // skip '['
    while (p < end) {
        while (p < end && (*p == ' ' || *p == ',' || *p == '"' || *p == '\t' || *p == '\n')) ++p;
        if (p >= end || *p == ']') break;
        if ((p[0] == 'n' || p[0] == 'N') && (p + 4 <= end) &&
            (p[1] == 'u') && (p[2] == 'l') && (p[3] == 'l')) {
            out.push_back(std::nan(""));
            p += 4;
            continue;
        }
        char *q = nullptr;
        double v = std::strtod(p, &q);
        if (q == p) { ++p; continue; }   // skip an unparseable token
        out.push_back(v);
        p = q;
    }
    return out;
}

// Serialize doubles → flat JSON array. NaN → null, ±Inf → "Inf"/"-Inf".
inline std::string serializeFlatDoubles(const std::vector<double> &v) {
    std::ostringstream os;
    os << "[";
    os.precision(17);
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) os << ",";
        double x = v[i];
        if (std::isnan(x)) os << "null";
        else if (std::isinf(x)) os << (x > 0 ? "\"Inf\"" : "\"-Inf\"");
        else os << x;
    }
    os << "]";
    return os.str();
}

}  // namespace figdetail

struct DatasetInfo
{
    std::string xJson;
    std::string yJson;
    std::string zJson;     // 2D matrix for imagesc, e.g. [[1,2],[3,4]]
    // Per-point auxiliary columns. Decoupled from xJson/yJson/zJson
    // because they carry semantically different data — overloading
    // zJson for marker sizes (as the original Phase 2e polar work
    // did) leaks coordinate semantics into a sizing channel and
    // breaks any future 3-D polar use case.
    //   sizeJson   — polarbubblechart / scatter point areas
    //                (points^2, MATLAB convention).
    //   colorJson  — per-point colour. May be a single RGB row
    //                "[[r,g,b]]" or N rows "[[r,g,b],...]". Renderer
    //                falls back to the dataset's `style.color` when
    //                empty.
    std::string sizeJson;
    std::string colorJson;
    std::string type;      // "line", "bar", "scatter", "stem", "stairs", "imagesc", "errorbar", …
    std::string label;     // for legend
    std::string style;     // MATLAB style hint, e.g. "r--o", "b:", "g-."
    double lineWidth = 0;  // 0 = default
    double markerSize = 0; // 0 = default

    // yyaxis side. Empty = left (default). When the user calls
    // yyaxis('right') and then plots, FigureManager::pushDataset stamps
    // 'right' here so the renderer can route the layer through the
    // secondary Y mapping. Emitted in JSON only when non-empty.
    std::string yside;

    // ── Errorbar — symmetric (eJson) or asymmetric (eNegJson + ePosJson) ──
    // For symmetric: errorbar(x, y, e) → eJson is non-empty, eNeg/ePos empty.
    // For asymmetric: errorbar(x, y, neg, pos) → eNegJson + ePosJson non-empty.
    // The renderer derives bar bounds as y - eNeg .. y + ePos (with eJson
    // doubled-up when symmetric). Empty for non-errorbar datasets.
    std::string eJson;
    std::string eNegJson;
    std::string ePosJson;

    // ── Quiver — vector field (per-point u/v components) ────────────────
    // quiver(x, y, u, v) → N arrows starting at (x, y) pointing
    // in direction (u, v). Empty for non-quiver datasets.
    std::string uJson;
    std::string vJson;

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

    // ── Phase 2 large line-series downsampling ──────────────────────────
    // For a huge line/stairs series (> kSeriesPreviewThreshold points) the
    // full x/y are parsed out of xJson/yJson into xRaw/yRaw, xJson/yJson are
    // replaced by a small M4 preview, and seriesDownsampled is set, so the
    // frontend receives O(previewCols) points instead of O(N).
    // getSeriesDisplayTile decimates xRaw/yRaw over a viewport for zoom
    // detail. Empty for small series (which keep their full inline JSON).
    std::vector<double> xRaw, yRaw;
    bool   seriesDownsampled = false;
    size_t seriesN = 0;
    double sxLo = 0, sxHi = 0, syLo = 0, syHi = 0;   // x/y data ranges
    // Lazy LOD pyramid (coarser levels only — xRaw/yRaw is the finest level)
    // built on the first getSeriesDisplayTile so every viewport tile is
    // O(width), not O(visible), even when fully zoomed out.
    mutable std::vector<DecimatedSeries> seriesPyramid;

    // ── Lazy LOD pyramid ───────────────────────────────────────────────
    // L0 is zQuantized itself (originalRows × originalCols, column-major).
    // Level k≥1 is lodLevels[k-1] with shape lodDims[k-1], built by 2×2
    // mean-pooling the previous level (NaN sentinel propagates: only finite
    // children contribute to a parent block, all-NaN block stays 255).
    //
    // Built lazily on getFigureDisplayTile when the resampler picks a
    // level higher than what's already cached. Bounded by ~10 levels —
    // beyond that the matrix is < 64×64 and further pooling is pointless.
    //
    // Memory cost: 4/3× original (geometric series). For 16k×42k uint8 =
    // 670 MB → pyramid total ~890 MB.
    mutable std::vector<std::vector<uint8_t>> lodLevels;
    mutable std::vector<std::pair<size_t, size_t>> lodDims;

    // ── Per-vertex colours for fill3 / polygon3d datasets.
    // Layout: parallel to xJson/yJson/zJson — for each finite (x, y, z)
    // sample there are three uint8 RGB bytes, in the SAME order. The
    // null separators between polygons / triangles do NOT consume
    // colour entries (the renderer skips them when walking xRaw/yRaw).
    // Empty for datasets that should fall back to the single-colour
    // `style` material.
    std::string vertexColorsJson;

    // ── animatedline storage. When type=="line" and the dataset was
    // created via animatedline / addpoints, these vectors hold the
    // raw point data; xJson/yJson are rebuilt from them on every
    // emit. Empty for non-animated datasets.
    std::vector<double> animatedX;
    std::vector<double> animatedY;
    bool isAnimated = false;

    // ── Image-RGB (truecolor, set by imshow with M×N×3 input) ──────────
    // Source bytes are ALWAYS uint8 0..255 — imshow casts double→u8 with
    // *255 and clamps before reaching here. Layout is row-major triplets
    // (r0,g0,b0, r1,g1,b1, ...). originalRows/originalCols give the
    // pixel dims (same field as imagesc reuses).
    //
    // rgbJson is the inline preview: "[[r,g,b],[r,g,b],...]" row-major,
    // mean-pooled if pixel count > 2M (same cap as imagesc). Empty for
    // non-RGB datasets. type == "image-rgb" gates the renderer.
    std::vector<uint8_t> rgbBytes;
    std::string rgbJson;
};

/** Per-axes state — one subplot panel has one AxesState */
struct AxesState
{
    std::vector<DatasetInfo> datasets;
    std::string title;
    std::string subtitle;
    std::string xlabel;
    std::string ylabel;
    std::string xlimJson;
    std::string ylimJson;
    std::string rlimJson;
    std::string thetalimJson;
    std::string climJson;     // color limits for imagesc, e.g. "[0,1]"
    std::string colormapName; // "parula","jet","hot","cool","gray","viridis","turbo","hsv"
    // Custom palette via `colormap(M)` with M an N×3 RGB matrix. JSON
    // array string "[[r,g,b],[r,g,b],...]" with values in [0, 1]. When
    // non-empty, overrides colormapName.
    std::string customColormapJson;
    bool polar = false;
    bool holdOn = false;
    // Major / minor grid as two independent booleans, matching MATLAB
    // semantics:
    //   grid          → toggles `gridMajor` (minor untouched)
    //   grid on       → gridMajor = true
    //   grid off      → both = false
    //   grid minor    → toggles `gridMinor` (major untouched)
    // gridUserTouched lets the adapter tell "user said off" apart from
    // "script never called grid()" — important because the JS side
    // wants different defaults for 2-D (off) and 3-D (on).
    bool gridMajor = false;
    bool gridMinor = false;
    bool gridUserTouched = false;
    std::vector<std::string> legendLabels;
    // Legend placement. MATLAB Location values: best (default), north,
    // south, east, west, northeast, northwest, southeast, southwest, and
    // the *outside variants. We normalise to lowercase. Empty = renderer
    // picks a default. Outside positions have a `outside` suffix.
    std::string legendLocation;
    // legendBoxOn — `legend boxoff` toggles the legend frame off.
    // Default true (MATLAB default). JSON emits only when off.
    bool legendBoxOn = true;
    // Colorbar placement. MATLAB: east (default), west, north, south,
    // and *outside variants. Same lowercase normalisation.
    std::string colorbarLocation;

    std::string xscale = "linear";
    std::string yscale = "linear";
    std::string colorScale = "linear";  // 'linear' | 'log' — survives prepareForPlot
    std::string axisMode;
    // Custom tick positions / labels. Empty = renderer auto-generates
    // (`niceTicks`). Non-empty values override the auto-generated set
    // for the corresponding axis. Labels override only when their length
    // matches the corresponding ticks count.
    std::string xTicksJson;
    std::string yTicksJson;
    std::string zTicksJson;
    std::string xTickLabelsJson;   // ["lo","mid","hi"] format
    std::string yTickLabelsJson;
    std::string zTickLabelsJson;
    // sprintf-style format string for tick labels (e.g. "%.2f").
    // Empty = renderer's auto format. Honoured only when no explicit
    // tick labels are set.
    std::string xTickFormat;
    std::string yTickFormat;
    std::string zTickFormat;
    // Tick-label rotation in degrees. 0 = horizontal (default).
    double xTickAngle = 0.0;
    double yTickAngle = 0.0;
    // axisVisible — MATLAB `axis off` / `axis on` controls whether the
    // axes lines / ticks / labels render. true = drawn (default), false
    // = hidden. imshow ships axis off implicitly (image-only viewport).
    // axisMode and axisVisible are independent: `axis image off` is
    // axisMode='image' + axisVisible=false. JSON emit only when false
    // so untouched figures keep the existing wire format.
    bool axisVisible = true;
    // boxOn — MATLAB `box on/off` controls the axis FRAME rectangle.
    // true (default) draws the full box; false draws only the X / Y
    // axis lines (left + bottom edges of the panel).
    bool boxOn = true;
    // Index into datasets[] of the most recent animatedline dataset.
    // -1 = no animated line yet. addpoints / clearpoints / getpoints
    // act on this dataset (numkit doesn't model graphics handles, so
    // animatedline operations target the axes' single active line).
    int animatedDatasetIdx = -1;
    // Axis direction. MATLAB: set(gca, 'XDir'/'YDir', 'normal'|'reverse').
    // 'normal' (default) is left-to-right / bottom-to-top. 'reverse'
    // flips. axis('ij') is shorthand for yDir='reverse'.
    std::string xDir = "normal";
    std::string yDir = "normal";

    // ── yyaxis (dual Y) ────────────────────────────────────────────
    // MATLAB:
    //   yyaxis left|right  — switches the active Y-side; subsequent
    //   plot/scatter/etc. write to that side, and ylim/ylabel/yscale
    //   target the active side too.
    // We model it as a "shadow" set of fields on the right side of the
    // existing axes. yyEnabled stays false until the first yyaxis call,
    // so single-axis plots keep their current JSON shape.
    bool yyEnabled = false;
    std::string activeYside = "left";  // "left" | "right"
    std::string ylabel2;
    std::string ylim2Json;
    std::string yscale2 = "linear";

    // 3-D camera view set by view(az, el); empty = renderer default
    // (-37.5°, 30°). Wire format: "[az,el]" with degrees.
    std::string viewJson;

    // Z-axis label / limits (3-D figures). Empty = auto-derived from
    // data extent, mirroring xlim/ylim semantics.
    std::string zlabel;
    std::string zlimJson;

    // 3-D lighting / material state. Empty strings = renderer default
    // (gouraud + plain Lambert + 1 ambient + 1 directional key).
    //   lightingMode: "flat" | "gouraud" | "phong" | "none"
    //   materialPreset: "shiny" | "metal" | "dull" | ""
    //   camlight: "left" | "right" | "headlight" | ""
    std::string lightingMode;
    std::string materialPreset;
    std::string camlightPos;

    // Interaction toggles for 3-D figures (rotate3d / pan3d / zoom3d).
    // "" = default (all enabled when interactive). "off" = disable
    // that specific axis of interaction. The renderer reads these and
    // updates OrbitControls accordingly.
    std::string rotate3dMode;
    std::string pan3dMode;
    std::string zoom3dMode;

    std::string thetaDir = "counterclockwise";
    std::string thetaZeroLocation = "right";
    // Custom polar tick positions + labels — MATLAB thetaticks(),
    // rticks(), thetaticklabels(), rticklabels(). Empty JSON arrays
    // = renderer falls back to its auto-generated nice ticks.
    //   thetaticks: array of DEGREES (e.g. [0 45 90 135 180])
    //   rticks:     array of radial values matching rlim units
    //   *ticklabels: array of strings, one per corresponding tick;
    //                length must match the tick array for it to take
    //                effect (renderer drops to auto when mismatched).
    std::string thetaticksJson;
    std::string rticksJson;
    std::string thetaticklabelsJson;
    std::string rticklabelsJson;

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

    // linkaxes mode for the figure's subplot cells. Empty / "off" = no
    // link. "x" / "y" / "xy" = pan/zoom propagates across all cells on
    // those axes. We model this at the figure level — handles aren't
    // a thing in numkit's graphics layer yet, so calling linkaxes(...)
    // unconditionally links every subplot cell in the current figure.
    std::string linkMode;

    // Figure-level "super title" set via sgtitle(...). MATLAB renders
    // it above the entire subplot grid, separate from per-axes
    // Title.String. Empty = no super-title (no header strip drawn).
    std::string superTitle;

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

    /**
     * Append a dataset to the current axes, stamping its `yside` from
     * AxesState::activeYside when yyaxis is on. All graphics builtins
     * route through this helper instead of `currentAxes().datasets.push_back`
     * directly so dual-Y routing stays consistent (no plot family forgotten).
     */
    // Large line-series → downsampled-preview thresholds. Below the
    // threshold the full array is shipped and the JS-side viewport
    // decimation (Phase 1) keeps full zoom detail; at/above it the engine
    // ships a small preview + retains raw x/y for getSeriesDisplayTile.
    static constexpr std::size_t kSeriesPreviewThreshold = 1000000; // points
    static constexpr int         kSeriesPreviewCols      = 2000;    // preview cols

    void pushDataset(DatasetInfo ds)
    {
        auto &ax = currentAxes();
        if (ax.yyEnabled && ds.yside.empty())
            ds.yside = ax.activeYside;
        maybeDownsampleSeries(ds);
        ax.datasets.push_back(std::move(ds));
    }

    // For a huge line/stairs series with ascending x, retain raw x/y for
    // viewport tiles and replace the inline JSON with a small M4 preview —
    // the frontend then receives O(previewCols) points, not O(N). Mirrors
    // imagesc's downsampled-preview path. No-op for small or non-monotonic
    // series (they keep their full inline JSON + the JS-side decimation).
    static void maybeDownsampleSeries(DatasetInfo &ds)
    {
        if (ds.type != "line" && ds.type != "stairs") return;
        if (ds.xJson.empty() || ds.yJson.empty()) return;
        if (figdetail::countFlatElements(ds.xJson) <= kSeriesPreviewThreshold) return;
        auto xr = figdetail::parseFlatDoubles(ds.xJson);
        auto yr = figdetail::parseFlatDoubles(ds.yJson);
        const std::size_t n = xr.size();
        if (n != yr.size() || n <= kSeriesPreviewThreshold) return;
        // Decimation buckets by x — valid only for ascending x. Bail on a
        // non-monotonic series (rare for huge data; keeps the full JSON).
        for (std::size_t i = 1; i < n; ++i) if (xr[i] < xr[i - 1]) return;
        double xlo = xr.front(), xhi = xr.back();
        if (xhi < xlo) std::swap(xlo, xhi);
        double ylo = INFINITY, yhi = -INFINITY;
        for (double v : yr) if (std::isfinite(v)) { ylo = std::min(ylo, v); yhi = std::max(yhi, v); }
        if (!std::isfinite(ylo)) { ylo = 0; yhi = 0; }
        auto prev = decimateM4(xr.data(), yr.data(), n, xlo, xhi, kSeriesPreviewCols);
        ds.xJson = figdetail::serializeFlatDoubles(prev.x);
        ds.yJson = figdetail::serializeFlatDoubles(prev.y);
        ds.xRaw = std::move(xr);
        ds.yRaw = std::move(yr);
        ds.seriesDownsampled = true;
        ds.seriesN = n;
        ds.sxLo = xlo; ds.sxHi = xhi; ds.syLo = ylo; ds.syHi = yhi;
    }

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
            // Preserve fields that "survive a fresh plot" — subplot position
            // and colorScale (the latter so `colorscale('log'); imagesc(M)`
            // bakes log into the new dataset's quantization).
            int savedSubplot = ax.subplotIndex;
            std::string savedColorScale = ax.colorScale;
            ax = AxesState{};
            ax.subplotIndex = savedSubplot;
            ax.colorScale = savedColorScale;
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
            if (!fig.linkMode.empty())
                os << ",\"linkMode\":\"" << fig.linkMode << "\"";
            if (!fig.superTitle.empty())
                os << ",\"superTitle\":\"" << jsonEscapeFig(fig.superTitle) << "\"";

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
                    if (ds.seriesDownsampled)
                        os << ",\"seriesDownsampled\":true,\"n\":" << ds.seriesN
                           << ",\"xRange\":[" << ds.sxLo << "," << ds.sxHi << "]"
                           << ",\"yRange\":[" << ds.syLo << "," << ds.syHi << "]";
                    if (!ds.label.empty())
                        os << ",\"label\":\"" << jsonEscapeFig(ds.label) << "\"";
                    if (!ds.style.empty())
                        os << ",\"style\":\"" << ds.style << "\"";
                    if (ds.lineWidth > 0)
                        os << ",\"lineWidth\":" << ds.lineWidth;
                    if (ds.markerSize > 0)
                        os << ",\"markerSize\":" << ds.markerSize;
                    // Per-point size / colour columns — used by
                    // polarbubblechart and (future) bubblechart on
                    // cartesian axes. Emitted as `size` / `pointColor`
                    // to avoid colliding with the dataset-level
                    // `color` style attribute.
                    if (!ds.sizeJson.empty())
                        os << ",\"size\":"       << ds.sizeJson;
                    if (!ds.colorJson.empty())
                        os << ",\"pointColor\":" << ds.colorJson;
                    if (!ds.eJson.empty())
                        os << ",\"e\":" << ds.eJson;
                    if (!ds.eNegJson.empty())
                        os << ",\"eNeg\":" << ds.eNegJson;
                    if (!ds.ePosJson.empty())
                        os << ",\"ePos\":" << ds.ePosJson;
                    if (!ds.uJson.empty())
                        os << ",\"u\":" << ds.uJson;
                    if (!ds.vJson.empty())
                        os << ",\"v\":" << ds.vJson;
                    if (!ds.yside.empty())
                        os << ",\"yside\":\"" << ds.yside << "\"";
                    if (!ds.zJson.empty())
                        os << ",\"z\":" << ds.zJson;
                    if (!ds.vertexColorsJson.empty())
                        os << ",\"vertexColors\":" << ds.vertexColorsJson;
                    if (!ds.rgbJson.empty()) {
                        os << ",\"rgb\":" << ds.rgbJson;
                        os << ",\"originalRows\":" << ds.originalRows
                           << ",\"originalCols\":" << ds.originalCols;
                        if (ds.downsampled)
                            os << ",\"downsampled\":true";
                    }
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
                if (!ax.subtitle.empty())
                    os << ",\"subtitle\":\"" << jsonEscapeFig(ax.subtitle) << "\"";
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
                if (!ax.customColormapJson.empty())
                    os << ",\"customColormap\":" << ax.customColormapJson;
                // Emit "grid" only when the user explicitly called
                // grid(...). Field absence means "script never asked";
                // the JS adapter then picks a type-appropriate default
                // (2-D off, 3-D on, mirroring MATLAB).
                if (ax.gridUserTouched)
                    os << ",\"grid\":\"" << (ax.gridMajor ? "on" : "off") << "\"";
                if (ax.gridMinor)
                    os << ",\"gridMinor\":\"on\"";
                os << ",\"polar\":" << (ax.polar ? "true" : "false");
                os << ",\"xscale\":\"" << ax.xscale << "\"";
                os << ",\"yscale\":\"" << ax.yscale << "\"";
                if (!ax.axisMode.empty())
                    os << ",\"axisMode\":\"" << ax.axisMode << "\"";
                if (!ax.xTicksJson.empty())
                    os << ",\"xticks\":" << ax.xTicksJson;
                if (!ax.yTicksJson.empty())
                    os << ",\"yticks\":" << ax.yTicksJson;
                if (!ax.zTicksJson.empty())
                    os << ",\"zticks\":" << ax.zTicksJson;
                if (!ax.xTickLabelsJson.empty())
                    os << ",\"xticklabels\":" << ax.xTickLabelsJson;
                if (!ax.yTickLabelsJson.empty())
                    os << ",\"yticklabels\":" << ax.yTickLabelsJson;
                if (!ax.zTickLabelsJson.empty())
                    os << ",\"zticklabels\":" << ax.zTickLabelsJson;
                if (!ax.xTickFormat.empty())
                    os << ",\"xtickformat\":\"" << jsonEscapeFig(ax.xTickFormat) << "\"";
                if (!ax.yTickFormat.empty())
                    os << ",\"ytickformat\":\"" << jsonEscapeFig(ax.yTickFormat) << "\"";
                if (!ax.zTickFormat.empty())
                    os << ",\"ztickformat\":\"" << jsonEscapeFig(ax.zTickFormat) << "\"";
                if (ax.xTickAngle != 0.0)
                    os << ",\"xtickangle\":" << ax.xTickAngle;
                if (ax.yTickAngle != 0.0)
                    os << ",\"ytickangle\":" << ax.yTickAngle;
                // axisVisible default = true; emit only when off
                // (imshow / `axis off` set this).
                if (!ax.axisVisible)
                    os << ",\"axisVisible\":false";
                if (!ax.boxOn)
                    os << ",\"box\":\"off\"";
                if (ax.xDir == "reverse")
                    os << ",\"xDir\":\"reverse\"";
                if (ax.yDir == "reverse")
                    os << ",\"yDir\":\"reverse\"";
                if (ax.polar) {
                    os << ",\"thetaDir\":\"" << ax.thetaDir << "\"";
                    os << ",\"thetaZeroLocation\":\"" << ax.thetaZeroLocation << "\"";
                    if (!ax.thetaticksJson.empty())
                        os << ",\"thetaticks\":"      << ax.thetaticksJson;
                    if (!ax.rticksJson.empty())
                        os << ",\"rticks\":"          << ax.rticksJson;
                    if (!ax.thetaticklabelsJson.empty())
                        os << ",\"thetaticklabels\":" << ax.thetaticklabelsJson;
                    if (!ax.rticklabelsJson.empty())
                        os << ",\"rticklabels\":"     << ax.rticklabelsJson;
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
                if (!ax.legendLocation.empty())
                    os << ",\"legendLocation\":\"" << ax.legendLocation << "\"";
                if (!ax.legendBoxOn)
                    os << ",\"legendBox\":\"off\"";
                if (!ax.colorbarLocation.empty())
                    os << ",\"colorbarLocation\":\"" << ax.colorbarLocation << "\"";
                if (ax.yyEnabled) {
                    os << ",\"yyEnabled\":true";
                    if (!ax.ylabel2.empty())
                        os << ",\"ylabel2\":\"" << jsonEscapeFig(ax.ylabel2) << "\"";
                    if (!ax.ylim2Json.empty())
                        os << ",\"ylim2\":" << ax.ylim2Json;
                    if (ax.yscale2 != "linear")
                        os << ",\"yscale2\":\"" << ax.yscale2 << "\"";
                }
                if (!ax.viewJson.empty())
                    os << ",\"view\":" << ax.viewJson;
                if (!ax.zlabel.empty())
                    os << ",\"zlabel\":\"" << jsonEscapeFig(ax.zlabel) << "\"";
                if (!ax.zlimJson.empty())
                    os << ",\"zlim\":" << ax.zlimJson;
                if (!ax.lightingMode.empty())
                    os << ",\"lighting\":\"" << ax.lightingMode << "\"";
                if (!ax.materialPreset.empty())
                    os << ",\"material\":\"" << ax.materialPreset << "\"";
                if (!ax.camlightPos.empty())
                    os << ",\"camlight\":\"" << ax.camlightPos << "\"";
                if (!ax.rotate3dMode.empty())
                    os << ",\"rotate3d\":\"" << ax.rotate3dMode << "\"";
                if (!ax.pan3dMode.empty())
                    os << ",\"pan3d\":\"" << ax.pan3dMode << "\"";
                if (!ax.zoom3dMode.empty())
                    os << ",\"zoom3d\":\"" << ax.zoom3dMode << "\"";
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
     * Display-grid tile resampler. Fills a displayH × displayW row-major
     * uint8 buffer with mean-pooled samples from the dataset's zQuantized,
     * applying optional log10 inverse on either axis so the buffer can be
     * blitted directly to the panel's pixel grid.
     *
     *   srcR0, srcC0, srcH, srcW : visible source-rect in source-cell coords
     *                              (fractional allowed for log inverse)
     *   xLog, yLog               : log10 axis transforms — when set, the
     *                              source-cell at display-pixel u (or v) is
     *                              srcC0 * (srcC1/srcC0)^(u/displayW), etc.
     *                              Requires srcC0 > 0 (yLog: srcR0 > 0).
     *   out                      : caller-allocated, size ≥ displayH*displayW
     *
     * Per display pixel:
     *  - compute its source-cell centre via linear or log map
     *  - mean-pool over a small ⌈srcStep⌉×⌈cStep⌉ block centred there
     *  - skip NaN sentinels (idx 255), write 255 if the block is all-NaN
     *
     * Cost: O(displayH * displayW * ⌈rStep⌉ * ⌈cStep⌉) — for a 800×600 panel
     * showing the full 10000² extent that's ~30M ops, ~30ms in WASM. The
     * upcoming Stage D LOD pyramid drops this to ~3 ms by reading from a
     * pre-pooled level rather than the L0 source.
     *
     * Returns false if request is out of range / not imagesc / no zQuantized
     * / log requested with non-positive source coords.
     */
    // Decimate a downsampled line series' raw x/y over the viewport [x0,x1]
    // to `width` columns. algo: 0=M4 (default), 1=LTTB, 2=none. Empty when
    // the dataset isn't a downsampled series or indices are out of range —
    // the frontend refines zoom detail without ever shipping the full array.
    DecimatedSeries getSeriesDisplayTile(int figId, int axIdx, int dsIdx,
                                         double x0, double x1, int width, int algo) const
    {
        DecimatedSeries out;
        auto it = figures_.find(figId);
        if (it == figures_.end()) return out;
        const auto &fig = it->second;
        if (axIdx < 0 || axIdx >= static_cast<int>(fig.axes.size())) return out;
        const auto &ax = fig.axes[axIdx];
        if (dsIdx < 0 || dsIdx >= static_cast<int>(ax.datasets.size())) return out;
        const auto &ds = ax.datasets[dsIdx];
        if (ds.xRaw.empty() || ds.xRaw.size() != ds.yRaw.size()) return out;
        // Build the coarse LOD levels once, then pick the cheapest level for
        // this viewport so the decimation is O(width) even fully zoomed out.
        if (ds.seriesPyramid.empty() && ds.xRaw.size() > 8000)
            ds.seriesPyramid = buildPyramid(ds.xRaw.data(), ds.yRaw.data(), ds.xRaw.size());
        DecimAlgo a = (algo == 1) ? DecimAlgo::LTTB
                    : (algo == 2) ? DecimAlgo::None
                    : (algo == 3) ? DecimAlgo::M2 : DecimAlgo::M4;
        return decimateLOD(ds.xRaw.data(), ds.yRaw.data(), ds.xRaw.size(),
                           ds.seriesPyramid, x0, x1, width, a);
    }

    bool getFigureDisplayTile(int figId, int axIdx, int dsIdx,
                              double srcR0, double srcC0,
                              double srcH, double srcW,
                              int displayH, int displayW,
                              bool xLog, bool yLog,
                              uint8_t *out) const
    {
        if (!out || displayH <= 0 || displayW <= 0) return false;

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

        // Clamp source-rect to the dataset bounds.
        if (srcR0 < 0) { srcH += srcR0; srcR0 = 0; }
        if (srcC0 < 0) { srcW += srcC0; srcC0 = 0; }
        const double srcR1 = std::min(static_cast<double>(fullRows), srcR0 + srcH);
        const double srcC1 = std::min(static_cast<double>(fullCols), srcC0 + srcW);
        if (srcR1 <= srcR0 || srcC1 <= srcC0) return false;

        // Log axes need a strictly-positive lower bound.
        if (yLog && srcR0 <= 0) return false;
        if (xLog && srcC0 <= 0) return false;

        // ── LOD pyramid: pick the smallest level whose cells are ≤1 per
        // display-pixel. At level k each cell covers 2^k × 2^k of L0, so
        // the optimal level is floor(log2(min(rStep, cStep))) clamped to
        // [0, 10]. Building higher levels lazily — first zoom-out builds
        // up the tree once, subsequent zooms reuse cached levels.
        const double rStep0 = (srcR1 - srcR0) / displayH;
        const double cStep0 = (srcC1 - srcC0) / displayW;
        const double minStep = std::min(rStep0, cStep0);
        int level = 0;
        if (minStep > 1.5) {
            level = static_cast<int>(std::floor(std::log2(minStep)));
            if (level < 0) level = 0;
            if (level > 10) level = 10;
        }
        ensureLOD(ds, level);

        // Resolve the chosen level's data + dims.
        const uint8_t *lvlData;
        size_t lvlRows, lvlCols;
        if (level == 0) {
            lvlData = ds.zQuantized.data();
            lvlRows = fullRows;
            lvlCols = fullCols;
        } else if (level <= static_cast<int>(ds.lodLevels.size())) {
            lvlData = ds.lodLevels[level - 1].data();
            lvlRows = ds.lodDims[level - 1].first;
            lvlCols = ds.lodDims[level - 1].second;
        } else {
            // ensureLOD couldn't build (e.g. dataset too small) — fall back to L0.
            lvlData = ds.zQuantized.data();
            lvlRows = fullRows;
            lvlCols = fullCols;
            level = 0;
        }

        // Map source-rect from L0 coords to the chosen level's coords.
        const double scaleFactor = static_cast<double>(1 << level);
        const double lvlR0 = srcR0 / scaleFactor;
        const double lvlC0 = srcC0 / scaleFactor;
        const double lvlR1 = srcR1 / scaleFactor;
        const double lvlC1 = srcC1 / scaleFactor;
        const double rStep = (lvlR1 - lvlR0) / displayH;
        const double cStep = (lvlC1 - lvlC0) / displayW;
        const int rBlock = std::max(1, static_cast<int>(std::ceil(rStep)));
        const int cBlock = std::max(1, static_cast<int>(std::ceil(cStep)));

        // Log-ratios computed in level coords (log preserves ratios).
        const double yLogRatio = yLog ? std::log(lvlR1 / lvlR0) : 0.0;
        const double xLogRatio = xLog ? std::log(lvlC1 / lvlC0) : 0.0;

        for (int orow = 0; orow < displayH; ++orow) {
            const double v = (orow + 0.5) / displayH;
            const double rCenter = yLog
                ? lvlR0 * std::exp(v * yLogRatio)
                : lvlR0 + v * (lvlR1 - lvlR0);
            const int rA = std::max(0, static_cast<int>(rCenter) - rBlock / 2);
            const int rB = std::min(static_cast<int>(lvlRows), rA + rBlock);
            for (int ocol = 0; ocol < displayW; ++ocol) {
                const double u = (ocol + 0.5) / displayW;
                const double cCenter = xLog
                    ? lvlC0 * std::exp(u * xLogRatio)
                    : lvlC0 + u * (lvlC1 - lvlC0);
                const int cA = std::max(0, static_cast<int>(cCenter) - cBlock / 2);
                const int cB = std::min(static_cast<int>(lvlCols), cA + cBlock);
                int sum = 0, n = 0;
                for (int cc = cA; cc < cB; ++cc) {
                    for (int rr = rA; rr < rB; ++rr) {
                        const uint8_t q = lvlData[cc * lvlRows + rr];
                        if (q != 255) { sum += q; ++n; }
                    }
                }
                out[orow * displayW + ocol] = (n > 0)
                    ? static_cast<uint8_t>((sum + n / 2) / n)
                    : uint8_t{255};
            }
        }
        return true;
    }

    /**
     * Build the LOD pyramid up to `targetLevel` if it isn't already cached.
     * Each level halves dims via 2×2 mean-pooling of the parent (NaN-skip).
     * Idempotent — repeated calls with smaller targetLevel are no-ops.
     *
     * Stops early if a level would shrink below 32×32 (further pooling is
     * useless detail-wise) or if memory allocation throws.
     */
    void ensureLOD(const DatasetInfo &ds, int targetLevel) const
    {
        while (static_cast<int>(ds.lodLevels.size()) < targetLevel) {
            size_t pRows, pCols;
            const uint8_t *parent;
            if (ds.lodLevels.empty()) {
                parent = ds.zQuantized.data();
                pRows  = ds.originalRows;
                pCols  = ds.originalCols;
            } else {
                const auto &dims = ds.lodDims.back();
                pRows = dims.first;
                pCols = dims.second;
                parent = ds.lodLevels.back().data();
            }
            const size_t cRows = (pRows + 1) / 2;
            const size_t cCols = (pCols + 1) / 2;
            if (cRows < 32 || cCols < 32) break;       // stop refining

            std::vector<uint8_t> child(cRows * cCols);
            for (size_t cc = 0; cc < cCols; ++cc) {
                const size_t pc0 = 2 * cc;
                const size_t pc1 = std::min(pCols, pc0 + 2);
                for (size_t rr = 0; rr < cRows; ++rr) {
                    const size_t pr0 = 2 * rr;
                    const size_t pr1 = std::min(pRows, pr0 + 2);
                    int sum = 0, n = 0;
                    for (size_t pc = pc0; pc < pc1; ++pc) {
                        for (size_t pr = pr0; pr < pr1; ++pr) {
                            const uint8_t q = parent[pc * pRows + pr];
                            if (q != 255) { sum += q; ++n; }
                        }
                    }
                    child[cc * cRows + rr] = (n > 0)
                        ? static_cast<uint8_t>((sum + n / 2) / n)
                        : uint8_t{255};
                }
            }
            ds.lodLevels.push_back(std::move(child));
            ds.lodDims.emplace_back(cRows, cCols);
        }
    }

    /**
     * Source-grid tile fetcher (kept for Stage A/B compat tests). Reads a
     * sub-rectangle rows [r0, r0+h) × cols [c0, c0+w) from zQuantized,
     * mean-pools by `lod×lod`, writes row-major uint8 indices to `out`.
     * Stage C uses getFigureDisplayTile instead — that one resamples to
     * display-pixel grid in one pass.
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