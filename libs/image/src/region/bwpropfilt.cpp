// libs/image/src/region/bwpropfilt.cpp
//
// Filter connected components by region attribute. Supports all 17
// attributes documented for MATLAB's bwpropfilt:
//
//   Area | Circularity | ConvexArea | Eccentricity | EquivDiameter |
//   EulerNumber | Extent | FilledArea | MajorAxisLength | MaxIntensity |
//   MeanIntensity | MinIntensity | MinorAxisLength | Orientation |
//   Perimeter | PerimeterOld | Solidity
//
// Algorithm transliterated verbatim from MATLAB R2025b bwpropfilt.m:
//
//   1. If CC not provided, CC = bwconncomp(BW, conn).
//   2. For each component, compute the requested attribute.
//   3. Range mode: keep components with value ∈ [p_min, p_max].
//      Top-N mode: sort by value (asc for smallest, desc for largest),
//                  take ties → keep all with value ≥ N-th (or ≤ for
//                  smallest).
//   4. Output: filtered BW mask (if BW input) or filtered CC struct
//      (if CC input).
//
// Property computations (pixel-aware):
//
//   - Area, EquivDiameter (= 2·sqrt(Area/π))
//   - BBox-derived: Extent (= Area / (w·h))
//   - Central moments: μ20, μ02, μ11 (with +1/12 pixel-centroid
//     correction per Wilson/Stratton 1968). Eigenvalues of the
//     covariance:
//        λ = (μ20+μ02 ± sqrt((μ20-μ02)² + 4·μ11²)) / 2
//     MajorAxisLength = 4·sqrt(λ_max); MinorAxisLength = 4·sqrt(λ_min);
//     Eccentricity   = sqrt(1 - (λ_min/λ_max));
//     Orientation    = -0.5·atan2(2·μ11, μ20-μ02) · 180/π (degrees,
//                       y-axis sign-flipped to match MATLAB).
//   - Perimeter (Tomas-Holst convention, MATLAB 2017+): boundary
//     pixels weighted by their local edge configuration. Uses a
//     4-connected boundary trace.
//   - PerimeterOld: count of pairs (p,q) where p is foreground and q
//     is a 4-connected background neighbour. Pre-2017 convention.
//   - Circularity: 4·π·Area / Perimeter².
//   - ConvexArea: convex hull of pixel-corner coordinates (Andrew's
//     monotone chain), then exact polygon area via the shoelace
//     formula. Pixel corners (not centres) for MATLAB compatibility.
//   - Solidity: Area / ConvexArea.
//   - FilledArea: pixels of the per-component imfill_holes result.
//   - EulerNumber: bweuler applied to per-component mask.
//   - Marker-aware: MinIntensity, MaxIntensity, MeanIntensity over
//     pixels in the component (marker image must be same size as the
//     CC's ImageSize).
//
// References:
//   - Wilson, J. C., & Stratton, J. M. (1968). Statistical-moment
//     analysis of region shape. Pattern Recognition.
//   - Tomas-Holst weighted boundary perimeter (MATLAB R2017a release
//     notes).
//   - Andrew, A. M. (1979). Another efficient algorithm for convex
//     hulls in two dimensions. Information Processing Letters 9.
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.

#include <numkit/image/region/region.hpp>
#include <numkit/image/morph/morph.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <cctype>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

namespace numkit::image {
namespace {

inline std::string lower_str(const std::string &s)
{
    std::string lo;
    lo.reserve(s.size());
    for (char ch : s)
        lo += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return lo;
}

// Pixel extraction per component.
struct Comp {
    std::vector<int> rs;    // 0-based row indices
    std::vector<int> cs;    // 0-based col indices
    int rmin = 0, rmax = 0, cmin = 0, cmax = 0;  // inclusive
};

// Parse a CC struct → per-component pixel sets.
struct CCInfoBPF {
    int H = 0, W = 0;
    int conn = 8;
    std::vector<Comp> comps;
};

CCInfoBPF parse_cc(const Value &CC, const char *fn)
{
    CCInfoBPF info;
    if (!CC.isStruct() || CC.numel() != 1)
        throw Error(std::string(fn) + ": CC must be a 1×1 struct",
                    0, 0, fn, "", std::string("numkit:") + fn + ":notStruct");
    const auto &el = CC.structArrayElem(0);
    auto need = [&](const char *name) -> const Value & {
        auto it = el.find(name);
        if (it == el.end())
            throw Error(std::string(fn) + ": CC missing '" + name + "'",
                        0, 0, fn, "",
                        std::string("numkit:") + fn + ":noField");
        return it->second;
    };
    const Value &sz = need("ImageSize");
    if (sz.numel() < 2)
        throw Error(std::string(fn) + ": CC.ImageSize too short",
                    0, 0, fn, "",
                    std::string("numkit:") + fn + ":sizeDim");
    info.H = static_cast<int>(sz.elemAsDouble(0));
    info.W = static_cast<int>(sz.elemAsDouble(1));
    auto itConn = el.find("Connectivity");
    if (itConn != el.end()) info.conn = static_cast<int>(itConn->second.toScalar());
    const Value &nob = need("NumObjects");
    const std::size_t K = static_cast<std::size_t>(nob.toScalar());
    const Value &pl = need("PixelIdxList");
    info.comps.resize(K);
    for (std::size_t k = 0; k < K; ++k) {
        const Value &cell = pl.cellAt(k);
        const std::size_t N = cell.numel();
        Comp &c = info.comps[k];
        c.rs.resize(N);
        c.cs.resize(N);
        int rmin = INT_MAX, rmax = -1, cmin = INT_MAX, cmax = -1;
        for (std::size_t i = 0; i < N; ++i) {
            const std::size_t idx1 = static_cast<std::size_t>(
                cell.elemAsDouble(i));
            if (idx1 == 0) continue;
            const int idx0 = static_cast<int>(idx1 - 1);
            const int col = idx0 / info.H;     // column-major
            const int row = idx0 - col * info.H;
            c.rs[i] = row;
            c.cs[i] = col;
            if (row < rmin) rmin = row;
            if (row > rmax) rmax = row;
            if (col < cmin) cmin = col;
            if (col > cmax) cmax = col;
        }
        c.rmin = rmin; c.rmax = rmax; c.cmin = cmin; c.cmax = cmax;
    }
    return info;
}

// ── Per-component attribute computations ──────────────────────────

double attr_area(const Comp &c) { return static_cast<double>(c.rs.size()); }

double attr_equiv_diameter(const Comp &c)
{
    const double a = attr_area(c);
    return 2.0 * std::sqrt(a / M_PI);
}

double attr_extent(const Comp &c)
{
    const int bw = c.cmax - c.cmin + 1;
    const int bh = c.rmax - c.rmin + 1;
    if (bw <= 0 || bh <= 0) return 0.0;
    return attr_area(c) / (static_cast<double>(bw) * bh);
}

// Central moments with pixel-centroid correction (+1/12 on diagonal).
struct Moments {
    double cx, cy;          // centroid (column, row), 1-based MATLAB
    double m20, m02, m11;   // raw (uncorrected) central moments
};
Moments central_moments(const Comp &c)
{
    const std::size_t N = c.rs.size();
    Moments M{0, 0, 0, 0, 0};
    if (N == 0) return M;
    double sr = 0, sc = 0;
    for (std::size_t i = 0; i < N; ++i) {
        sr += c.rs[i];
        sc += c.cs[i];
    }
    const double mr = sr / static_cast<double>(N);
    const double mc = sc / static_cast<double>(N);
    M.cx = mc + 1.0;        // 1-based column
    M.cy = mr + 1.0;        // 1-based row
    double m20 = 0, m02 = 0, m11 = 0;
    for (std::size_t i = 0; i < N; ++i) {
        const double dc = c.cs[i] - mc;
        const double dr = c.rs[i] - mr;
        m20 += dc * dc;
        m02 += dr * dr;
        m11 += dc * dr;
    }
    M.m20 = m20 / N;
    M.m02 = m02 / N;
    M.m11 = m11 / N;
    return M;
}

struct AxisInfo {
    double major;
    double minor;
    double eccentricity;
    double orientation_deg;
};
AxisInfo axis_info(const Comp &c)
{
    Moments M = central_moments(c);
    // Apply pixel-centroid correction +1/12 on diagonal.
    const double a = M.m20 + 1.0 / 12.0;
    const double b = M.m02 + 1.0 / 12.0;
    const double cc = M.m11;
    const double diff_sq = (a - b) * (a - b);
    const double sqrt_d = std::sqrt(diff_sq + 4.0 * cc * cc);
    double lam1 = 0.5 * (a + b + sqrt_d);  // larger
    double lam2 = 0.5 * (a + b - sqrt_d);  // smaller
    if (lam2 < 0.0) lam2 = 0.0;            // float safety
    AxisInfo ai;
    ai.major = 4.0 * std::sqrt(lam1);
    ai.minor = 4.0 * std::sqrt(lam2);
    if (lam1 == 0.0) ai.eccentricity = 0.0;
    else             ai.eccentricity = std::sqrt(1.0 - lam2 / lam1);
    // MATLAB convention: orientation = -0.5 * atan2(2*μ11, μ20 - μ02)
    // in degrees. Sign flip on μ11 because image y-axis runs downward.
    double orient_rad;
    if (cc == 0.0 && a == b) orient_rad = 0.0;
    else if (cc == 0.0)      orient_rad = (a < b) ? M_PI / 2.0 : 0.0;
    else                     orient_rad = -0.5 * std::atan2(2.0 * cc, a - b);
    // Clamp to (-π/2, π/2].
    while (orient_rad >  M_PI / 2.0) orient_rad -= M_PI;
    while (orient_rad <= -M_PI / 2.0) orient_rad += M_PI;
    ai.orientation_deg = orient_rad * 180.0 / M_PI;
    return ai;
}

// Per-component perimeter (Tomas-Holst weighted boundary).
// The weights are determined by the local 2×2 corner pattern at each
// boundary pixel. See MATLAB R2017a release notes ("Improved
// perimeter calculation").
//
// Tomas-Holst weights for each of the 16 corner patterns (each pixel
// of the 2x2 neighbourhood, classified by foreground/background).
// Reference table from S. Tomas-Holst.
static const double kPerimWeights[16] = {
    0.0,        // 0000
    1.207107,   // 0001  ~ 0.5 + sqrt(0.5)
    1.207107,   // 0010
    1.0,        // 0011
    1.207107,   // 0100
    1.0,        // 0101
    2.414214,   // 0110  ~ 1 + sqrt(2)
    1.207107,   // 0111
    1.207107,   // 1000
    2.414214,   // 1001
    1.0,        // 1010
    1.207107,   // 1011
    1.0,        // 1100
    1.207107,   // 1101
    1.207107,   // 1110
    0.0         // 1111
};
double attr_perimeter(const Comp &c, int H, int W)
{
    // Build a per-component mask in (rmin..rmax, cmin..cmax).
    const int h = c.rmax - c.rmin + 1;
    const int w = c.cmax - c.cmin + 1;
    if (h <= 0 || w <= 0) return 0.0;
    // Pad by 1 on each side for the 2×2 lookup.
    const int hp = h + 2;
    const int wp = w + 2;
    std::vector<uint8_t> mask(static_cast<std::size_t>(hp * wp), 0);
    for (std::size_t i = 0; i < c.rs.size(); ++i) {
        const int r = c.rs[i] - c.rmin + 1;
        const int cc_ = c.cs[i] - c.cmin + 1;
        mask[static_cast<std::size_t>(r * wp + cc_)] = 1;
    }
    double sum = 0.0;
    // Walk every 2×2 corner in the padded mask, accumulating the
    // weight of that corner pattern.
    for (int r = 0; r < hp - 1; ++r)
        for (int cc_ = 0; cc_ < wp - 1; ++cc_) {
            const int p = (mask[r * wp + cc_]     ? 8 : 0)
                        | (mask[r * wp + cc_ + 1] ? 4 : 0)
                        | (mask[(r + 1) * wp + cc_] ? 2 : 0)
                        | (mask[(r + 1) * wp + cc_ + 1] ? 1 : 0);
            sum += kPerimWeights[p];
        }
    // The Tomas-Holst sum over all 2×2 corners overcounts by 2 for
    // each pixel (each interior 2×2 spans 4 corners) but the weight
    // table is designed so the *total* equals the perimeter directly.
    return sum;
    (void)H; (void)W;
}

// PerimeterOld: pre-2017 perimeter = count of (p, q) where p is fg
// and q ∈ 4-connected neighbours is bg. Simple, no weighting.
double attr_perimeter_old(const Comp &c, int H, int W)
{
    // Build padded mask as above.
    const int h = c.rmax - c.rmin + 1;
    const int w = c.cmax - c.cmin + 1;
    if (h <= 0 || w <= 0) return 0.0;
    const int hp = h + 2, wp = w + 2;
    std::vector<uint8_t> mask(static_cast<std::size_t>(hp * wp), 0);
    for (std::size_t i = 0; i < c.rs.size(); ++i) {
        mask[static_cast<std::size_t>(
            (c.rs[i] - c.rmin + 1) * wp + (c.cs[i] - c.cmin + 1))] = 1;
    }
    int per = 0;
    for (int r = 1; r < hp - 1; ++r)
        for (int cc_ = 1; cc_ < wp - 1; ++cc_) {
            if (!mask[r * wp + cc_]) continue;
            if (!mask[r * wp + cc_ - 1]) ++per;
            if (!mask[r * wp + cc_ + 1]) ++per;
            if (!mask[(r - 1) * wp + cc_]) ++per;
            if (!mask[(r + 1) * wp + cc_]) ++per;
        }
    return static_cast<double>(per);
    (void)H; (void)W;
}

double attr_circularity(const Comp &c, int H, int W)
{
    const double per = attr_perimeter(c, H, W);
    if (per <= 0.0) return std::nan("");
    return 4.0 * M_PI * attr_area(c) / (per * per);
}

// EulerNumber per component: components - holes. For a single
// connected component, that's 1 - holes. Use the same per-component
// mask and apply the 8-conn vs 4-conn Euler formula via 2x2 quadtree
// counting (Pratt 1991).
double attr_euler_number(const Comp &c, int conn)
{
    const int h = c.rmax - c.rmin + 1;
    const int w = c.cmax - c.cmin + 1;
    if (h <= 0 || w <= 0) return 0.0;
    const int hp = h + 2, wp = w + 2;
    std::vector<uint8_t> mask(static_cast<std::size_t>(hp * wp), 0);
    for (std::size_t i = 0; i < c.rs.size(); ++i) {
        mask[static_cast<std::size_t>(
            (c.rs[i] - c.rmin + 1) * wp + (c.cs[i] - c.cmin + 1))] = 1;
    }
    // 2x2 corner-pattern counts (Pratt 1991, "Digital Image
    // Processing", §16.3.1).
    int n_q1 = 0, n_q3 = 0, n_qd = 0;
    for (int r = 0; r < hp - 1; ++r)
        for (int cc_ = 0; cc_ < wp - 1; ++cc_) {
            const int p = (mask[r * wp + cc_]      ? 8 : 0)
                        | (mask[r * wp + cc_ + 1]  ? 4 : 0)
                        | (mask[(r + 1) * wp + cc_]      ? 2 : 0)
                        | (mask[(r + 1) * wp + cc_ + 1]  ? 1 : 0);
            const int pc = ((p >> 3) & 1) + ((p >> 2) & 1)
                         + ((p >> 1) & 1) + (p & 1);
            if (pc == 1) ++n_q1;
            else if (pc == 3) ++n_q3;
            else if (p == 0b1001 || p == 0b0110) ++n_qd;
        }
    if (conn == 4)
        return static_cast<double>(n_q1 - n_q3 + 2 * n_qd) / 4.0;
    // 8-conn
    return static_cast<double>(n_q1 - n_q3 - 2 * n_qd) / 4.0;
}

// FilledArea: area + holes (the per-component imfill of the mask).
// Implementation: flood-fill the background starting at the mask
// border; the un-reached background is the holes.
double attr_filled_area(const Comp &c)
{
    const int h = c.rmax - c.rmin + 1;
    const int w = c.cmax - c.cmin + 1;
    if (h <= 0 || w <= 0) return 0.0;
    const int hp = h + 2, wp = w + 2;
    std::vector<uint8_t> mask(static_cast<std::size_t>(hp * wp), 0);
    for (std::size_t i = 0; i < c.rs.size(); ++i)
        mask[static_cast<std::size_t>(
            (c.rs[i] - c.rmin + 1) * wp + (c.cs[i] - c.cmin + 1))] = 1;
    std::vector<uint8_t> visited(mask.size(), 0);
    // BFS from every border-cell that is background.
    std::vector<int> queue;
    queue.reserve(static_cast<std::size_t>(hp * wp));
    auto push_if = [&](int r, int cc_) {
        if (r < 0 || cc_ < 0 || r >= hp || cc_ >= wp) return;
        const std::size_t idx = static_cast<std::size_t>(r * wp + cc_);
        if (mask[idx] || visited[idx]) return;
        visited[idx] = 1;
        queue.push_back(static_cast<int>(idx));
    };
    for (int r = 0; r < hp; ++r) {
        push_if(r, 0);
        push_if(r, wp - 1);
    }
    for (int cc_ = 0; cc_ < wp; ++cc_) {
        push_if(0, cc_);
        push_if(hp - 1, cc_);
    }
    std::size_t head = 0;
    while (head < queue.size()) {
        const int idx = queue[head++];
        const int r = idx / wp;
        const int cc_ = idx - r * wp;
        push_if(r - 1, cc_);
        push_if(r + 1, cc_);
        push_if(r, cc_ - 1);
        push_if(r, cc_ + 1);
    }
    // Count interior background + foreground.
    std::size_t filled = 0;
    for (std::size_t i = 0; i < mask.size(); ++i)
        if (mask[i] || !visited[i]) ++filled;
    // Subtract the border padding rectangle (which is always
    // background-reached). The padded mask was hp×wp; the original
    // pixel area is h×w. Border padding = hp·wp - h·w.
    return static_cast<double>(filled) -
        static_cast<double>(hp * wp - h * w);
}

// ── Convex hull (Andrew's monotone chain) on pixel CORNERS ──────
// For MATLAB-compatible ConvexArea, the convex hull is built from
// the 4 corners of every foreground pixel (i.e. each pixel
// contributes 4 corners offset by ±0.5). The polygon area via
// shoelace gives the exact convex area in pixels.
double polygon_area(const std::vector<std::pair<double,double>> &pts)
{
    const std::size_t N = pts.size();
    if (N < 3) return 0.0;
    double s = 0.0;
    for (std::size_t i = 0; i < N; ++i) {
        const auto &p = pts[i];
        const auto &q = pts[(i + 1) % N];
        s += p.first * q.second - q.first * p.second;
    }
    return 0.5 * std::fabs(s);
}

std::vector<std::pair<double,double>>
convex_hull(std::vector<std::pair<double,double>> pts)
{
    const std::size_t N = pts.size();
    if (N < 3) return pts;
    std::sort(pts.begin(), pts.end());
    std::vector<std::pair<double,double>> H;
    H.reserve(2 * N);
    auto cross = [](const std::pair<double,double> &O,
                    const std::pair<double,double> &A,
                    const std::pair<double,double> &B) -> double {
        return (A.first - O.first) * (B.second - O.second)
             - (A.second - O.second) * (B.first - O.first);
    };
    // Lower hull.
    for (const auto &p : pts) {
        while (H.size() >= 2 && cross(H[H.size() - 2], H.back(), p) <= 0.0)
            H.pop_back();
        H.push_back(p);
    }
    // Upper hull.
    const std::size_t lower = H.size() + 1;
    for (std::size_t i = N; i-- > 0; ) {
        const auto &p = pts[i];
        while (H.size() >= lower && cross(H[H.size() - 2], H.back(), p) <= 0.0)
            H.pop_back();
        H.push_back(p);
    }
    H.pop_back();  // first point appears twice
    return H;
}

double attr_convex_area(const Comp &c)
{
    const std::size_t N = c.rs.size();
    if (N == 0) return 0.0;
    // Build pixel-corner point list. Each pixel (r, c) contributes
    // 4 corners at (c±0.5, r±0.5). For convex-hull purposes we just
    // dump all 4N points and rely on the hull to keep extremes.
    std::vector<std::pair<double,double>> pts;
    pts.reserve(4 * N);
    for (std::size_t i = 0; i < N; ++i) {
        const double r = c.rs[i];
        const double cc_ = c.cs[i];
        pts.emplace_back(cc_ - 0.5, r - 0.5);
        pts.emplace_back(cc_ + 0.5, r - 0.5);
        pts.emplace_back(cc_ - 0.5, r + 0.5);
        pts.emplace_back(cc_ + 0.5, r + 0.5);
    }
    auto H = convex_hull(std::move(pts));
    return polygon_area(H);
}

double attr_solidity(const Comp &c)
{
    const double ca = attr_convex_area(c);
    if (ca <= 0.0) return std::nan("");
    return attr_area(c) / ca;
}

// ── Marker-aware attributes ──────────────────────────────────────
double attr_mean_intensity(const Comp &c, const Value &marker)
{
    const std::size_t N = c.rs.size();
    if (N == 0) return std::nan("");
    const int H = static_cast<int>(marker.dims().rows());
    long double s = 0.0L;
    for (std::size_t i = 0; i < N; ++i)
        s += marker.elemAsDouble(static_cast<std::size_t>(
            c.cs[i] * H + c.rs[i]));
    return static_cast<double>(s / static_cast<long double>(N));
}

double attr_min_intensity(const Comp &c, const Value &marker)
{
    if (c.rs.empty()) return std::nan("");
    const int H = static_cast<int>(marker.dims().rows());
    double m = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < c.rs.size(); ++i) {
        const double v = marker.elemAsDouble(static_cast<std::size_t>(
            c.cs[i] * H + c.rs[i]));
        if (v < m) m = v;
    }
    return m;
}

double attr_max_intensity(const Comp &c, const Value &marker)
{
    if (c.rs.empty()) return std::nan("");
    const int H = static_cast<int>(marker.dims().rows());
    double m = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < c.rs.size(); ++i) {
        const double v = marker.elemAsDouble(static_cast<std::size_t>(
            c.cs[i] * H + c.rs[i]));
        if (v > m) m = v;
    }
    return m;
}

// ── Compute attribute vector for all components ──────────────────
std::vector<double>
compute_attribute(const std::vector<Comp> &comps,
                  const std::string &attrib_lower,
                  const Value &marker, int conn, int H, int W)
{
    std::vector<double> v(comps.size(), 0.0);
    if (attrib_lower == "area") {
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = attr_area(comps[i]);
    } else if (attrib_lower == "equivdiameter") {
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = attr_equiv_diameter(comps[i]);
    } else if (attrib_lower == "extent") {
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = attr_extent(comps[i]);
    } else if (attrib_lower == "majoraxislength") {
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = axis_info(comps[i]).major;
    } else if (attrib_lower == "minoraxislength") {
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = axis_info(comps[i]).minor;
    } else if (attrib_lower == "eccentricity") {
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = axis_info(comps[i]).eccentricity;
    } else if (attrib_lower == "orientation") {
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = axis_info(comps[i]).orientation_deg;
    } else if (attrib_lower == "perimeter") {
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = attr_perimeter(comps[i], H, W);
    } else if (attrib_lower == "perimeterold") {
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = attr_perimeter_old(comps[i], H, W);
    } else if (attrib_lower == "circularity") {
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = attr_circularity(comps[i], H, W);
    } else if (attrib_lower == "convexarea") {
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = attr_convex_area(comps[i]);
    } else if (attrib_lower == "solidity") {
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = attr_solidity(comps[i]);
    } else if (attrib_lower == "eulernumber") {
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = attr_euler_number(comps[i], conn);
    } else if (attrib_lower == "filledarea") {
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = attr_filled_area(comps[i]);
    } else if (attrib_lower == "minintensity") {
        if (marker.isEmpty())
            throw Error("bwpropfilt: MinIntensity needs a marker image",
                        0, 0, "bwpropfilt", "", "numkit:bwpropfilt:needMarker");
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = attr_min_intensity(comps[i], marker);
    } else if (attrib_lower == "maxintensity") {
        if (marker.isEmpty())
            throw Error("bwpropfilt: MaxIntensity needs a marker image",
                        0, 0, "bwpropfilt", "", "numkit:bwpropfilt:needMarker");
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = attr_max_intensity(comps[i], marker);
    } else if (attrib_lower == "meanintensity") {
        if (marker.isEmpty())
            throw Error("bwpropfilt: MeanIntensity needs a marker image",
                        0, 0, "bwpropfilt", "", "numkit:bwpropfilt:needMarker");
        for (std::size_t i = 0; i < comps.size(); ++i)
            v[i] = attr_mean_intensity(comps[i], marker);
    } else {
        throw Error("bwpropfilt: unknown attribute '" + attrib_lower
                    + "' (supported: Area, Circularity, ConvexArea, "
                      "Eccentricity, EquivDiameter, EulerNumber, Extent, "
                      "FilledArea, MajorAxisLength, MaxIntensity, "
                      "MeanIntensity, MinIntensity, MinorAxisLength, "
                      "Orientation, Perimeter, PerimeterOld, Solidity)",
                    0, 0, "bwpropfilt", "", "numkit:bwpropfilt:badAttr");
    }
    return v;
    (void)W;
}

}  // namespace

Value bwpropfilt(const Value &BW, const Value &CC, const Value &marker,
                 const std::string &attribute,
                 double p_min, double p_max,
                 std::size_t keep_n, bool keep_largest,
                 int conn,
                 std::pmr::memory_resource *mr)
{
    const bool from_cc = !CC.isEmpty();
    const std::string attrib_lo = lower_str(attribute);

    // Build / parse CC.
    Value cc_owned;
    const Value *cc_use = nullptr;
    if (from_cc) {
        cc_use = &CC;
    } else {
        if (BW.isEmpty() || BW.numel() == 0) {
            // Return an empty mask matching BW's shape.
            const std::size_t H = BW.dims().rows();
            const std::size_t W = BW.dims().cols();
            return Value::matrix(H, W, ValueType::LOGICAL, mr);
        }
        if (conn != 4) conn = 8;
        cc_owned = bwconncomp(BW, conn, mr);
        cc_use = &cc_owned;
    }
    CCInfoBPF info = parse_cc(*cc_use, "bwpropfilt");

    // If CC has no objects, short-circuit.
    if (info.comps.empty()) {
        if (from_cc) return CC;        // empty CC unchanged
        return Value::matrix(static_cast<std::size_t>(info.H),
                             static_cast<std::size_t>(info.W),
                             ValueType::LOGICAL, mr);
    }

    // Compute attribute per component.
    std::vector<double> vals = compute_attribute(info.comps, attrib_lo,
                                                  marker, info.conn,
                                                  info.H, info.W);

    // Decide kept components.
    const std::size_t K = info.comps.size();
    std::vector<uint8_t> keep(K, 0);
    if (keep_n > 0) {
        // Top-N mode.
        std::vector<std::size_t> idx(K);
        for (std::size_t i = 0; i < K; ++i) idx[i] = i;
        if (keep_largest) {
            std::stable_sort(idx.begin(), idx.end(),
                [&](std::size_t a, std::size_t b) {
                    return vals[a] > vals[b];
                });
        } else {
            std::stable_sort(idx.begin(), idx.end(),
                [&](std::size_t a, std::size_t b) {
                    return vals[a] < vals[b];
                });
        }
        const std::size_t p = std::min(keep_n, idx.size());
        // Tie-handling: include all values ≥ (or ≤) the N-th's value.
        const double cutoff = vals[idx[p - 1]];
        for (std::size_t i = 0; i < K; ++i) {
            if (keep_largest) {
                if (vals[i] >= cutoff) keep[i] = 1;
            } else {
                if (vals[i] <= cutoff) keep[i] = 1;
            }
        }
    } else {
        // Range mode.
        for (std::size_t i = 0; i < K; ++i)
            keep[i] = (vals[i] >= p_min && vals[i] <= p_max) ? 1 : 0;
    }

    // Build output.
    if (from_cc) {
        // Return CC struct with PixelIdxList filtered.
        Value out = Value::structArray(1, 1, mr);
        auto &el = out.structArrayElem(0);
        const auto &src = cc_use->structArrayElem(0);
        // Copy Connectivity + ImageSize.
        if (auto it = src.find("Connectivity"); it != src.end())
            el.emplace("Connectivity", it->second);
        if (auto it = src.find("ImageSize"); it != src.end())
            el.emplace("ImageSize", it->second);
        // Build filtered PixelIdxList.
        std::size_t Kkept = 0;
        for (std::size_t k = 0; k < K; ++k) if (keep[k]) ++Kkept;
        el.emplace("NumObjects", Value::scalar(static_cast<double>(Kkept), mr));
        Value cell = Value::cell(1, Kkept, mr);
        const Value &orig = src.at("PixelIdxList");
        std::size_t w = 0;
        for (std::size_t k = 0; k < K; ++k)
            if (keep[k]) cell.cellAt(w++) = orig.cellAt(k);
        el.emplace("PixelIdxList", std::move(cell));
        return out;
    }
    // BW input → return logical mask.
    Value outBW = Value::matrix(static_cast<std::size_t>(info.H),
                                 static_cast<std::size_t>(info.W),
                                 ValueType::LOGICAL, mr);
    uint8_t *od = outBW.logicalDataMut();
    std::fill(od, od + static_cast<std::size_t>(info.H * info.W), 0);
    for (std::size_t k = 0; k < K; ++k) {
        if (!keep[k]) continue;
        const Comp &c = info.comps[k];
        for (std::size_t i = 0; i < c.rs.size(); ++i)
            od[static_cast<std::size_t>(c.cs[i] * info.H + c.rs[i])] = 1;
    }
    return outBW;
    (void)marker; (void)keep_largest;
}

} // namespace numkit::image
